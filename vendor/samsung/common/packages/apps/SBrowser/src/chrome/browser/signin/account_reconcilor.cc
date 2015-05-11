// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_reconcilor.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"

// Fetches a refresh token from the given session in the GAIA cookie.  This is
// a best effort only.  If it should fail, another reconcile action will occur
// shortly anyway.
class AccountReconcilor::RefreshTokenFetcher : public GaiaAuthConsumer {
 public:
  RefreshTokenFetcher(AccountReconcilor* reconcilor,
                      const std::string& account_id,
                      int session_index);
  virtual ~RefreshTokenFetcher() {}

 private:
  // Overridden from GaiaAuthConsumer:
  virtual void OnClientOAuthSuccess(const ClientOAuthResult& result) OVERRIDE;
  virtual void OnClientOAuthFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  AccountReconcilor* reconcilor_;
  GaiaAuthFetcher fetcher_;
  const std::string account_id_;
  int session_index_;

  DISALLOW_COPY_AND_ASSIGN(RefreshTokenFetcher);
};

AccountReconcilor::RefreshTokenFetcher::RefreshTokenFetcher(
    AccountReconcilor* reconcilor,
    const std::string& account_id,
    int session_index)
    : reconcilor_(reconcilor),
      fetcher_(this, GaiaConstants::kChromeSource,
               reconcilor_->profile()->GetRequestContext()),
      account_id_(account_id),
      session_index_(session_index) {
  DCHECK(reconcilor_);
  DCHECK(!account_id.empty());
  fetcher_.StartCookieForOAuthLoginTokenExchange(
      base::IntToString(session_index_));
}

void AccountReconcilor::RefreshTokenFetcher::OnClientOAuthSuccess(
    const ClientOAuthResult& result) {
  VLOG(1) << "RefreshTokenFetcher::OnClientOAuthSuccess:"
          << " account=" << account_id_
          << " session_index=" << session_index_;

  reconcilor_->HandleRefreshTokenFetched(account_id_,
                                         result.refresh_token);
}

void AccountReconcilor::RefreshTokenFetcher::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "RefreshTokenFetcher::OnClientOAuthFailure:"
          << " account=" << account_id_
          << " session_index=" << session_index_;
  reconcilor_->HandleRefreshTokenFetched(account_id_, std::string());
}


class AccountReconcilor::UserIdFetcher
    : public gaia::GaiaOAuthClient::Delegate {
 public:
  UserIdFetcher(AccountReconcilor* reconcilor,
                const std::string& access_token,
                const std::string& account_id);

  // Returns the scopes needed by the UserIdFetcher.
  static OAuth2TokenService::ScopeSet GetScopes();

 private:
  // Overriden from gaia::GaiaOAuthClient::Delegate.
  virtual void OnGetUserIdResponse(const std::string& user_id) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

  AccountReconcilor* const reconcilor_;
  const std::string account_id_;
  const std::string access_token_;
  gaia::GaiaOAuthClient gaia_auth_client_;

  DISALLOW_COPY_AND_ASSIGN(UserIdFetcher);
};

AccountReconcilor::UserIdFetcher::UserIdFetcher(AccountReconcilor* reconcilor,
                                                const std::string& access_token,
                                                const std::string& account_id)
    : reconcilor_(reconcilor),
      account_id_(account_id),
      access_token_(access_token),
      gaia_auth_client_(reconcilor_->profile()->GetRequestContext()) {
  DCHECK(reconcilor_);
  DCHECK(!account_id_.empty());

  const int kMaxRetries = 5;
  gaia_auth_client_.GetUserId(access_token_, kMaxRetries, this);
}

// static
OAuth2TokenService::ScopeSet AccountReconcilor::UserIdFetcher::GetScopes() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert("https://www.googleapis.com/auth/userinfo.profile");
  return scopes;
}

void AccountReconcilor::UserIdFetcher::OnGetUserIdResponse(
    const std::string& user_id) {
  VLOG(1) << "AccountReconcilor::OnGetUserIdResponse: " << account_id_;
  reconcilor_->HandleSuccessfulAccountIdCheck(account_id_);
}

void AccountReconcilor::UserIdFetcher::OnOAuthError() {
  VLOG(1) << "AccountReconcilor::OnOAuthError: " << account_id_;
  reconcilor_->HandleFailedAccountIdCheck(account_id_);

  // Invalidate the access token to force a refetch next time.
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(reconcilor_->profile());
  token_service->InvalidateToken(account_id_, GetScopes(), access_token_);
}

void AccountReconcilor::UserIdFetcher::OnNetworkError(int response_code) {
  VLOG(1) << "AccountReconcilor::OnNetworkError: " << account_id_
          << " response_code=" << response_code;

  // TODO(rogerta): some response error should not be treated like
  // permanent errors.  Figure out appropriate ones.
  reconcilor_->HandleFailedAccountIdCheck(account_id_);
}

AccountReconcilor::AccountReconcilor(Profile* profile)
    : OAuth2TokenService::Consumer("account_reconcilor"),
      profile_(profile),
      merge_session_helper_(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          profile->GetRequestContext(),
          this),
      registered_with_token_service_(false),
      is_reconcile_started_(false),
      are_gaia_accounts_set_(false),
      requests_(NULL) {
  VLOG(1) << "AccountReconcilor::AccountReconcilor";
  RegisterWithSigninManager();

  // If this profile is not connected, the reconcilor should do nothing but
  // wait for the connection.
  if (IsProfileConnected()) {
    RegisterWithCookieMonster();
    RegisterWithTokenService();
    StartPeriodicReconciliation();
  }
}

AccountReconcilor::~AccountReconcilor() {
  // Make sure shutdown was called first.
  DCHECK(!registered_with_token_service_);
  DCHECK(registrar_.IsEmpty());
  DCHECK(!reconciliation_timer_.IsRunning());
  DCHECK(!requests_);
  DCHECK_EQ(0u, user_id_fetchers_.size());
  DCHECK_EQ(0u, refresh_token_fetchers_.size());
}

void AccountReconcilor::Shutdown() {
  VLOG(1) << "AccountReconcilor::Shutdown";
  merge_session_helper_.CancelAll();
  merge_session_helper_.RemoveObserver(this);
  gaia_fetcher_.reset();
  DeleteFetchers();
  UnregisterWithSigninManager();
  UnregisterWithTokenService();
  UnregisterWithCookieMonster();
  StopPeriodicReconciliation();
}

void AccountReconcilor::AddMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  merge_session_helper_.AddObserver(observer);
}

void AccountReconcilor::RemoveMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  merge_session_helper_.RemoveObserver(observer);
}

void AccountReconcilor::DeleteFetchers() {
  delete[] requests_;
  requests_ = NULL;

  user_id_fetchers_.clear();
  refresh_token_fetchers_.clear();
}

bool AccountReconcilor::AreAllRefreshTokensChecked() const {
  return chrome_accounts_.size() ==
      (valid_chrome_accounts_.size() + invalid_chrome_accounts_.size());
}

void AccountReconcilor::RegisterWithCookieMonster() {
  content::Source<Profile> source(profile_);
  if (!registrar_.IsRegistered(this, chrome::NOTIFICATION_COOKIE_CHANGED,
                               source)) {
    registrar_.Add(this, chrome::NOTIFICATION_COOKIE_CHANGED, source);
  }
}

void AccountReconcilor::UnregisterWithCookieMonster() {
  content::Source<Profile> source(profile_);
  if (registrar_.IsRegistered(this, chrome::NOTIFICATION_COOKIE_CHANGED,
                              source)) {
    registrar_.Remove(this, chrome::NOTIFICATION_COOKIE_CHANGED, source);
  }
}

void AccountReconcilor::RegisterWithSigninManager() {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  signin_manager->AddObserver(this);
}

void AccountReconcilor::UnregisterWithSigninManager() {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  signin_manager->RemoveObserver(this);
}

void AccountReconcilor::RegisterWithTokenService() {
  VLOG(1) << "AccountReconcilor::RegisterWithTokenService";
  // During re-auth, the reconcilor will get a GOOGLE_SIGNIN_SUCCESSFUL
  // even when the profile is already connected.  Avoid re-registering
  // with the token service since this will DCHECK.
  if (registered_with_token_service_)
    return;

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->AddObserver(this);
  registered_with_token_service_ = true;
}

void AccountReconcilor::UnregisterWithTokenService() {
  if (!registered_with_token_service_)
    return;

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->RemoveObserver(this);
  registered_with_token_service_ = false;
}

bool AccountReconcilor::IsProfileConnected() {
  return !SigninManagerFactory::GetForProfile(profile_)->
      GetAuthenticatedUsername().empty();
}

void AccountReconcilor::StartPeriodicReconciliation() {
  VLOG(1) << "AccountReconcilor::StartPeriodicReconciliation";
  // TODO(rogerta): pick appropriate thread and timeout value.
  reconciliation_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(300),
      this,
      &AccountReconcilor::PeriodicReconciliation);
}

void AccountReconcilor::StopPeriodicReconciliation() {
  VLOG(1) << "AccountReconcilor::StopPeriodicReconciliation";
  reconciliation_timer_.Stop();
}

void AccountReconcilor::PeriodicReconciliation() {
  VLOG(1) << "AccountReconcilor::PeriodicReconciliation";
  StartReconcile();
}

void AccountReconcilor::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_COOKIE_CHANGED:
      OnCookieChanged(content::Details<ChromeCookieDetails>(details).ptr());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AccountReconcilor::OnCookieChanged(ChromeCookieDetails* details) {
  if (details->cookie->Name() == "LSID" &&
      details->cookie->Domain() == GaiaUrls::GetInstance()->gaia_url().host() &&
      details->cookie->IsSecure() &&
      details->cookie->IsHttpOnly()) {
    VLOG(1) << "AccountReconcilor::OnCookieChanged: LSID changed";
    StartReconcile();
  }
}

void AccountReconcilor::OnRefreshTokenAvailable(const std::string& account_id) {
  VLOG(1) << "AccountReconcilor::OnRefreshTokenAvailable: " << account_id;
  StartReconcile();
}

void AccountReconcilor::OnRefreshTokenRevoked(const std::string& account_id) {
  VLOG(1) << "AccountReconcilor::OnRefreshTokenRevoked: " << account_id;
  StartRemoveAction(account_id);
}

void AccountReconcilor::OnRefreshTokensLoaded() {}

void AccountReconcilor::GoogleSigninSucceeded(
    const std::string& username, const std::string& password) {
  VLOG(1) << "AccountReconcilor::GoogleSigninSucceeded: signed in";
  RegisterWithCookieMonster();
  RegisterWithTokenService();
  StartPeriodicReconciliation();
}

void AccountReconcilor::GoogleSignedOut(const std::string& username) {
  VLOG(1) << "AccountReconcilor::GoogleSignedOut: signed out";
  UnregisterWithTokenService();
  UnregisterWithCookieMonster();
  StopPeriodicReconciliation();
}

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  VLOG(1) << "AccountReconcilor::PerformMergeAction: " << account_id;
  merge_session_helper_.LogIn(account_id);
}

void AccountReconcilor::StartRemoveAction(const std::string& account_id) {
  VLOG(1) << "AccountReconcilor::StartRemoveAction: " << account_id;
  GetAccountsFromCookie(
      base::Bind(&AccountReconcilor::FinishRemoveAction,
                 base::Unretained(this),
                 account_id));
}

void AccountReconcilor::FinishRemoveAction(
    const std::string& account_id,
    const GoogleServiceAuthError& error,
    const std::vector<std::pair<std::string, bool> >& accounts) {
  VLOG(1) << "AccountReconcilor::FinishRemoveAction:"
          << " account=" << account_id
          << " error=" << error.ToString();
  if (error.state() == GoogleServiceAuthError::NONE) {
    AbortReconcile();
    std::vector<std::string> accounts_only;
    for (std::vector<std::pair<std::string, bool> >::const_iterator i =
             accounts.begin(); i != accounts.end(); ++i) {
      accounts_only.push_back(i->first);
    }
    merge_session_helper_.LogOut(account_id, accounts_only);
  }
  // Wait for the next ReconcileAction if there is an error.
}

void AccountReconcilor::PerformAddToChromeAction(
    const std::string& account_id,
    int session_index) {
  VLOG(1) << "AccountReconcilor::PerformAddToChromeAction:"
          << " account=" << account_id
          << " session_index=" << session_index;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  refresh_token_fetchers_.push_back(
      new RefreshTokenFetcher(this, account_id, session_index));
#endif
}

void AccountReconcilor::PerformLogoutAllAccountsAction() {
  VLOG(1) << "AccountReconcilor::PerformLogoutAllAccountsAction";
  merge_session_helper_.LogOutAllAccounts();
}

void AccountReconcilor::StartReconcile() {
  if (!IsProfileConnected() || is_reconcile_started_)
    return;

  is_reconcile_started_ = true;

  // Reset state for validating gaia cookie.
  are_gaia_accounts_set_ = false;
  gaia_accounts_.clear();
  GetAccountsFromCookie(base::Bind(
      &AccountReconcilor::ContinueReconcileActionAfterGetGaiaAccounts,
          base::Unretained(this)));

  // Reset state for validating oauth2 tokens.
  primary_account_.clear();
  chrome_accounts_.clear();
  DeleteFetchers();
  valid_chrome_accounts_.clear();
  invalid_chrome_accounts_.clear();
  add_to_cookie_.clear();
  add_to_chrome_.clear();
  ValidateAccountsFromTokenService();
}

void AccountReconcilor::GetAccountsFromCookie(
    GetAccountsFromCookieCallback callback) {
  get_gaia_accounts_callbacks_.push_back(callback);
  if (!gaia_fetcher_) {
    // There is no list account request in flight.
    gaia_fetcher_.reset(new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                                            profile_->GetRequestContext()));
    gaia_fetcher_->StartListAccounts();
  }
}

void AccountReconcilor::OnListAccountsSuccess(const std::string& data) {
  gaia_fetcher_.reset();

  // Get account information from response data.
  std::vector<std::pair<std::string, bool> > gaia_accounts;
  bool valid_json = gaia::ParseListAccountsData(data, &gaia_accounts);
  if (!valid_json) {
    VLOG(1) << "AccountReconcilor::OnListAccountsSuccess: parsing error";
  } else if (gaia_accounts.size() > 0) {
    VLOG(1) << "AccountReconcilor::OnListAccountsSuccess: "
            << "Gaia " << gaia_accounts.size() << " accounts, "
            << "Primary is '" << gaia_accounts[0].first << "'";
  } else {
    VLOG(1) << "AccountReconcilor::OnListAccountsSuccess: No accounts";
  }

  // There must be at least one callback waiting for result.
  DCHECK(!get_gaia_accounts_callbacks_.empty());

  GoogleServiceAuthError error = !valid_json
      ? GoogleServiceAuthError(
            GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE)
      : GoogleServiceAuthError::AuthErrorNone();
  get_gaia_accounts_callbacks_.front().Run(error, gaia_accounts);
  get_gaia_accounts_callbacks_.pop_front();

  MayBeDoNextListAccounts();
}

void AccountReconcilor::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  gaia_fetcher_.reset();
  VLOG(1) << "AccountReconcilor::OnListAccountsFailure: " << error.ToString();
  std::vector<std::pair<std::string, bool> > empty_accounts;

  // There must be at least one callback waiting for result.
  DCHECK(!get_gaia_accounts_callbacks_.empty());

  get_gaia_accounts_callbacks_.front().Run(error, empty_accounts);
  get_gaia_accounts_callbacks_.pop_front();

  MayBeDoNextListAccounts();
}

void AccountReconcilor::MayBeDoNextListAccounts() {
  if (!get_gaia_accounts_callbacks_.empty()) {
    gaia_fetcher_.reset(new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                                            profile_->GetRequestContext()));
    gaia_fetcher_->StartListAccounts();
  }
}

void AccountReconcilor::ContinueReconcileActionAfterGetGaiaAccounts(
    const GoogleServiceAuthError& error,
    const std::vector<std::pair<std::string, bool> >& accounts) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    gaia_accounts_ = accounts;
    are_gaia_accounts_set_ = true;
    FinishReconcile();
  } else {
    AbortReconcile();
  }
}

void AccountReconcilor::ValidateAccountsFromTokenService() {
  primary_account_ =
      SigninManagerFactory::GetForProfile(profile_)->GetAuthenticatedUsername();
  DCHECK(!primary_account_.empty());

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  chrome_accounts_ = token_service->GetAccounts();
  DCHECK(chrome_accounts_.size() > 0);

  VLOG(1) << "AccountReconcilor::ValidateAccountsFromTokenService: "
          << "Chrome " << chrome_accounts_.size() << " accounts, "
          << "Primary is '" << primary_account_ << "'";

  DCHECK(!requests_);
  requests_ =
      new scoped_ptr<OAuth2TokenService::Request>[chrome_accounts_.size()];
  const OAuth2TokenService::ScopeSet scopes =
      AccountReconcilor::UserIdFetcher::GetScopes();
  for (size_t i = 0; i < chrome_accounts_.size(); ++i) {
    requests_[i] = token_service->StartRequest(chrome_accounts_[i],
                                               scopes,
                                               this);
  }

  DCHECK_EQ(0u, user_id_fetchers_.size());
  user_id_fetchers_.resize(chrome_accounts_.size());
}

void AccountReconcilor::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  size_t index;
  for (index = 0; index < chrome_accounts_.size(); ++index) {
    if (request == requests_[index].get())
      break;
  }
  DCHECK(index < chrome_accounts_.size());

  const std::string& account_id = chrome_accounts_[index];

  VLOG(1) << "AccountReconcilor::OnGetTokenSuccess: valid " << account_id;

  DCHECK(!user_id_fetchers_[index]);
  user_id_fetchers_[index] =
      new UserIdFetcher(this, access_token, account_id);
}

void AccountReconcilor::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  size_t index;
  for (index = 0; index < chrome_accounts_.size(); ++index) {
    if (request == requests_[index].get())
      break;
  }
  DCHECK(index < chrome_accounts_.size());

  const std::string& account_id = chrome_accounts_[index];

  VLOG(1) << "AccountReconcilor::OnGetTokenFailure: invalid "
          << account_id;
  HandleFailedAccountIdCheck(account_id);
}

void AccountReconcilor::FinishReconcile() {
  // Make sure that the process of validating the gaia cookie and the oauth2
  // tokens individually is done before proceeding with reconciliation.
  if (!are_gaia_accounts_set_ || !AreAllRefreshTokensChecked())
    return;

  VLOG(1) << "AccountReconcilor::FinishReconcile";

  DeleteFetchers();

  DCHECK(add_to_cookie_.empty());
  DCHECK(add_to_chrome_.empty());
  bool are_primaries_equal =
      gaia_accounts_.size() > 0 && primary_account_ == gaia_accounts_[0].first;

  if (are_primaries_equal) {
    // Determine if we need to merge accounts from gaia cookie to chrome.
    for (size_t i = 0; i < gaia_accounts_.size(); ++i) {
      const std::string& gaia_account = gaia_accounts_[i].first;
      if (gaia_accounts_[i].second &&
              valid_chrome_accounts_.find(gaia_account) ==
          valid_chrome_accounts_.end()) {
        add_to_chrome_.push_back(std::make_pair(gaia_account, i));
      }
    }

    // Determine if we need to merge accounts from chrome into gaia cookie.
    for (std::set<std::string>::const_iterator i =
             valid_chrome_accounts_.begin();
         i != valid_chrome_accounts_.end(); ++i) {
      bool add_to_cookie = true;
      for (size_t j = 0; j < gaia_accounts_.size(); ++j) {
        if (gaia_accounts_[j].first == *i) {
          add_to_cookie = !gaia_accounts_[j].second;
          break;
        }
      }
      if (add_to_cookie)
        add_to_cookie_.push_back(*i);
    }
  } else {
    VLOG(1) << "AccountReconcilor::FinishReconcile: rebuild cookie";
    // Really messed up state.  Blow away the gaia cookie completely and
    // rebuild it, making sure the primary account as specified by the
    // SigninManager is the first session in the gaia cookie.
    PerformLogoutAllAccountsAction();
    add_to_cookie_.push_back(primary_account_);
    for (std::set<std::string>::const_iterator i =
             valid_chrome_accounts_.begin();
         i != valid_chrome_accounts_.end(); ++i) {
      if (*i != primary_account_)
        add_to_cookie_.push_back(*i);
    }
  }

  // For each account known to chrome but not in the gaia cookie,
  // PerformMergeAction().
  for (size_t i = 0; i < add_to_cookie_.size(); ++i)
    PerformMergeAction(add_to_cookie_[i]);

  // For each account in the gaia cookie not known to chrome,
  // PerformAddToChromeAction.
  for (std::vector<std::pair<std::string, int> >::const_iterator i =
            add_to_chrome_.begin();
        i != add_to_chrome_.end(); ++i) {
    PerformAddToChromeAction(i->first, i->second);
  }

  CalculateIfReconcileIsDone();
}

void AccountReconcilor::AbortReconcile() {
  VLOG(1) << "AccountReconcilor::AbortReconcile: we'll try again later";
  DeleteFetchers();
  add_to_cookie_.clear();
  add_to_chrome_.clear();
  CalculateIfReconcileIsDone();
}

void AccountReconcilor::CalculateIfReconcileIsDone() {
  is_reconcile_started_ = !add_to_cookie_.empty() || !add_to_chrome_.empty();
  if (!is_reconcile_started_)
    VLOG(1) << "AccountReconcilor::CalculateIfReconcileIsDone: done";
}

void AccountReconcilor::MergeSessionCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::MergeSessionCompleted: account_id="
          << account_id;

  // Remove the account from the list that is being merged.
  for (std::vector<std::string>::iterator i = add_to_cookie_.begin();
       i != add_to_cookie_.end(); ++i) {
    if (account_id == *i) {
      add_to_cookie_.erase(i);
      break;
    }
  }

  CalculateIfReconcileIsDone();
}

void AccountReconcilor::HandleSuccessfulAccountIdCheck(
    const std::string& account_id) {
  valid_chrome_accounts_.insert(account_id);
  FinishReconcile();
}

void AccountReconcilor::HandleFailedAccountIdCheck(
    const std::string& account_id) {
  invalid_chrome_accounts_.insert(account_id);
  FinishReconcile();
}

void AccountReconcilor::HandleRefreshTokenFetched(
    const std::string& account_id,
    const std::string& refresh_token) {
  if (!refresh_token.empty()) {
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
    token_service->UpdateCredentials(account_id, refresh_token);
  }

  // Remove the account from the list that is being updated.
  for (std::vector<std::pair<std::string, int> >::iterator i =
           add_to_chrome_.begin();
       i != add_to_chrome_.end(); ++i) {
    if (account_id == i->first) {
      add_to_chrome_.erase(i);
      break;
    }
  }

  CalculateIfReconcileIsDone();
}
