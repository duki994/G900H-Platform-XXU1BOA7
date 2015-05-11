// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_H_

#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/signin/signin_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/merge_session_helper.h"
#include "google_apis/gaia/oauth2_token_service.h"

class GaiaAuthFetcher;
class Profile;
struct ChromeCookieDetails;

class AccountReconcilor : public BrowserContextKeyedService,
                          public content::NotificationObserver,
                          public GaiaAuthConsumer,
                          public MergeSessionHelper::Observer,
                          public OAuth2TokenService::Consumer,
                          public OAuth2TokenService::Observer,
                          public SigninManagerBase::Observer {
 public:
  explicit AccountReconcilor(Profile* profile);
  virtual ~AccountReconcilor();

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Add or remove observers for the merge session notification.
  void AddMergeSessionObserver(MergeSessionHelper::Observer* observer);
  void RemoveMergeSessionObserver(MergeSessionHelper::Observer* observer);

  Profile* profile() { return profile_; }

  bool IsPeriodicReconciliationRunning() const {
    return reconciliation_timer_.IsRunning();
  }

  bool IsRegisteredWithTokenService() const {
    return registered_with_token_service_;
  }

  bool AreGaiaAccountsSet() const { return are_gaia_accounts_set_; }

  bool AreAllRefreshTokensChecked() const;

  const std::vector<std::pair<std::string, bool> >&
      GetGaiaAccountsForTesting() const {
    return gaia_accounts_;
  }

 private:
  const std::set<std::string>& GetValidChromeAccountsForTesting() const {
    return valid_chrome_accounts_;
  }

  const std::set<std::string>& GetInvalidChromeAccountsForTesting() const {
    return invalid_chrome_accounts_;
  }

  // Used during GetAccountsFromCookie.
  // Stores a callback for the next action to perform.
  typedef base::Callback<void(
      const GoogleServiceAuthError& error,
      const std::vector<std::pair<std::string, bool> >&)>
          GetAccountsFromCookieCallback;

  friend class AccountReconcilorTest;
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, GetAccountsFromCookieSuccess);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, GetAccountsFromCookieFailure);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, ValidateAccountsFromTokens);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           ValidateAccountsFromTokensFailedUserInfo);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           ValidateAccountsFromTokensFailedTokenRequest);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoop);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoopMultiple);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileAddToCookie);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileAddToChrome);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileBadPrimary);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileWithSessionInfoExpiredDefault);

  class RefreshTokenFetcher;
  class UserIdFetcher;

  // Register and unregister with dependent services.
  void RegisterWithCookieMonster();
  void UnregisterWithCookieMonster();
  void RegisterWithSigninManager();
  void UnregisterWithSigninManager();
  void RegisterWithTokenService();
  void UnregisterWithTokenService();

  bool IsProfileConnected();

  void DeleteFetchers();

  // Start and stop the periodic reconciliation.
  void StartPeriodicReconciliation();
  void StopPeriodicReconciliation();
  void PeriodicReconciliation();

  // All actions with side effects.  Virtual so that they can be overridden
  // in tests.
  virtual void PerformMergeAction(const std::string& account_id);
  virtual void PerformAddToChromeAction(const std::string& account_id,
                                        int session_index);
  virtual void PerformLogoutAllAccountsAction();

  // Used to remove an account from chrome and the cookie jar.
  virtual void StartRemoveAction(const std::string& account_id);
  virtual void FinishRemoveAction(
      const std::string& account_id,
      const GoogleServiceAuthError& error,
      const std::vector<std::pair<std::string, bool> >& accounts);

  // Used during periodic reconciliation.
  void StartReconcile();
  void FinishReconcile();
  void AbortReconcile();
  void CalculateIfReconcileIsDone();
  void HandleSuccessfulAccountIdCheck(const std::string& account_id);
  void HandleFailedAccountIdCheck(const std::string& account_id);
  void HandleRefreshTokenFetched(const std::string& account_id,
                                 const std::string& refresh_token);

  void GetAccountsFromCookie(GetAccountsFromCookieCallback callback);
  void ContinueReconcileActionAfterGetGaiaAccounts(
      const GoogleServiceAuthError& error,
      const std::vector<std::pair<std::string, bool> >& accounts);
  void ValidateAccountsFromTokenService();

  void OnCookieChanged(ChromeCookieDetails* details);

  // Overriden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overriden from GaiaAuthConsumer.
  virtual void OnListAccountsSuccess(const std::string& data) OVERRIDE;
  virtual void OnListAccountsFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // Overriden from MergeSessionHelper::Observer.
  virtual void MergeSessionCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) OVERRIDE;

  // Overriden from OAuth2TokenService::Consumer.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // Overriden from OAuth2TokenService::Observer.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokensLoaded() OVERRIDE;

  // Overriden from SigninManagerBase::Observer.
  virtual void GoogleSigninSucceeded(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void GoogleSignedOut(const std::string& username) OVERRIDE;

  void MayBeDoNextListAccounts();

  // The profile that this reconcilor belongs to.
  Profile* profile_;
  content::NotificationRegistrar registrar_;
  base::RepeatingTimer<AccountReconcilor> reconciliation_timer_;
  MergeSessionHelper merge_session_helper_;
  scoped_ptr<GaiaAuthFetcher> gaia_fetcher_;
  bool registered_with_token_service_;

  // True while the reconcilor is busy checking or managing the accounts in
  // this profile.
  bool is_reconcile_started_;

  // Used during reconcile action.
  // These members are used used to validate the gaia cookie.  |gaia_accounts_|
  // holds the state of google accounts in the gaia cookie.  Each element is
  // a pair that holds the email address of the account and a boolean that
  // indicates whether the account is valid or not.  The accounts in the vector
  // are ordered the in same way as the gaia cookie.
  bool are_gaia_accounts_set_;
  std::vector<std::pair<std::string, bool> > gaia_accounts_;

  // Used during reconcile action.
  // These members are used to validate the tokens in OAuth2TokenService.
  std::string primary_account_;
  std::vector<std::string> chrome_accounts_;
  scoped_ptr<OAuth2TokenService::Request>* requests_;
  ScopedVector<UserIdFetcher> user_id_fetchers_;
  ScopedVector<RefreshTokenFetcher> refresh_token_fetchers_;
  std::set<std::string> valid_chrome_accounts_;
  std::set<std::string> invalid_chrome_accounts_;
  std::vector<std::string> add_to_cookie_;
  std::vector<std::pair<std::string, int> > add_to_chrome_;

  std::deque<GetAccountsFromCookieCallback> get_gaia_accounts_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilor);
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_H_
