// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_driver.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;

namespace autofill {
class AutofillManager;
}

namespace {

class MockPasswordManagerDriver : public PasswordManagerDriver {
 public:
  MockPasswordManagerDriver() {}
  virtual ~MockPasswordManagerDriver() {}

  MOCK_METHOD1(FillPasswordForm,
               void(const autofill::PasswordFormFillData& form_data));
  MOCK_METHOD0(DidLastPageLoadEncounterSSLErrors, bool());
  MOCK_METHOD0(IsOffTheRecord, bool());
  MOCK_METHOD0(GetPasswordGenerationManager, PasswordGenerationManager*());
  MOCK_METHOD0(GetPasswordManager, PasswordManager*());
  MOCK_METHOD0(GetAutofillManager, autofill::AutofillManager*());
  MOCK_METHOD1(AllowPasswordGenerationForForm,
               void(autofill::PasswordForm* form));
};

class TestPasswordManagerClient : public PasswordManagerClient {
 public:
  explicit TestPasswordManagerClient(Profile* profile,
                                     PasswordStore* password_store)
      : profile_(profile),
        password_store_(password_store) {}

  virtual void PromptUserToSavePassword(PasswordFormManager* form_to_save)
      OVERRIDE {}
  virtual PrefService* GetPrefs() OVERRIDE { return profile_->GetPrefs(); }
  virtual PasswordStore* GetPasswordStore() OVERRIDE { return password_store_;}
  virtual PasswordManagerDriver* GetDriver() OVERRIDE { return &driver_; }
  virtual void AuthenticateAutofillAndFillForm(
      scoped_ptr<autofill::PasswordFormFillData> fill_data) OVERRIDE {
    driver_.FillPasswordForm(*fill_data.get());
  }

  #if defined(S_AUTOCOMPLETE_ALERT_POPUP)
  virtual void ShowAutoCompleteAlertPopUp() OVERRIDE { }
  #endif
  
  MockPasswordManagerDriver* GetMockDriver() { return &driver_; }

 private:
  Profile* profile_;
  PasswordStore* password_store_;
  MockPasswordManagerDriver driver_;
};

class TestPasswordManager : public PasswordManager {
 public:
  explicit TestPasswordManager(PasswordManagerClient* client)
      : PasswordManager(client) {}

  virtual void Autofill(
      const autofill::PasswordForm& form_for_autofill,
      const autofill::PasswordFormMap& best_matches,
      const autofill::PasswordForm& preferred_match,
      bool wait_for_username) const OVERRIDE {
    best_matches_ = best_matches;
  }

  const autofill::PasswordFormMap& GetLatestBestMatches() {
    return best_matches_;
  }

 private:
  // Marked mutable to get around constness of Autofill().
  mutable autofill::PasswordFormMap best_matches_;
};

}  // namespace

class PasswordFormManagerTest : public testing::Test {
 public:
  PasswordFormManagerTest() {
  }

  virtual void SetUp() {
    observed_form_.origin = GURL("http://accounts.google.com/a/LoginAuth");
    observed_form_.action = GURL("http://accounts.google.com/a/Login");
    observed_form_.username_element = ASCIIToUTF16("Email");
    observed_form_.password_element = ASCIIToUTF16("Passwd");
    observed_form_.submit_element = ASCIIToUTF16("signIn");
    observed_form_.signon_realm = "http://accounts.google.com";

    saved_match_ = observed_form_;
    saved_match_.origin = GURL("http://accounts.google.com/a/ServiceLoginAuth");
    saved_match_.action = GURL("http://accounts.google.com/a/ServiceLogin");
    saved_match_.preferred = true;
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.password_value = ASCIIToUTF16("test1");
    saved_match_.other_possible_usernames.push_back(
        ASCIIToUTF16("test2@gmail.com"));
    profile_ = new TestingProfile();
  }

  virtual void TearDown() {
    delete profile_;
  }

  PasswordForm* GetPendingCredentials(PasswordFormManager* p) {
    return &p->pending_credentials_;
  }

  void SimulateMatchingPhase(PasswordFormManager* p, bool find_match) {
    // Roll up the state to mock out the matching phase.
    p->state_ = PasswordFormManager::POST_MATCHING_PHASE;
    if (!find_match)
      return;

    PasswordForm* match = new PasswordForm(saved_match_);
    // Heap-allocated form is owned by p.
    p->best_matches_[match->username_value] = match;
    p->preferred_match_ = match;
  }

  void SimulateFetchMatchingLoginsFromPasswordStore(
      PasswordFormManager* manager) {
    // Just need to update the internal states.
    manager->state_ = PasswordFormManager::MATCHING_PHASE;
  }

  void SimulateResponseFromPasswordStore(
      PasswordFormManager* manager,
      const std::vector<PasswordForm*>& result) {
    // Simply call the callback method when request done. This will transfer
    // the ownership of the objects in |result| to the |manager|.
    manager->OnGetPasswordStoreResults(result);
  }

  void SanitizePossibleUsernames(PasswordFormManager* p, PasswordForm* form) {
    p->SanitizePossibleUsernames(form);
  }

  bool IgnoredResult(PasswordFormManager* p, PasswordForm* form) {
    return p->IgnoreResult(*form);
  }

  Profile* profile() { return profile_; }

  PasswordForm* observed_form() { return &observed_form_; }
  PasswordForm* saved_match() { return &saved_match_; }
  PasswordForm* CreateSavedMatch(bool blacklisted) {
    // Owned by the caller of this method.
    PasswordForm* match = new PasswordForm(saved_match_);
    match->blacklisted_by_user = blacklisted;
    return match;
  }

 private:
  PasswordForm observed_form_;
  PasswordForm saved_match_;
  Profile* profile_;
};

TEST_F(PasswordFormManagerTest, TestNewLogin) {
  scoped_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient(profile(), NULL));
  scoped_ptr<MockPasswordManagerDriver> driver;
  PasswordFormManager* manager = new PasswordFormManager(
      NULL, client.get(), driver.get(), *observed_form(), false);

  SimulateMatchingPhase(manager, false);
  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.preferred = true;
  manager->ProvisionallySave(
      credentials,
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(manager->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db.
  EXPECT_EQ(observed_form()->origin.spec(),
            GetPendingCredentials(manager)->origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            GetPendingCredentials(manager)->signon_realm);
  EXPECT_EQ(observed_form()->action,
              GetPendingCredentials(manager)->action);
  EXPECT_TRUE(GetPendingCredentials(manager)->preferred);
  EXPECT_EQ(saved_match()->password_value,
            GetPendingCredentials(manager)->password_value);
  EXPECT_EQ(saved_match()->username_value,
            GetPendingCredentials(manager)->username_value);

  // Now, suppose the user re-visits the site and wants to save an additional
  // login for the site with a new username. In this case, the matching phase
  // will yield the previously saved login.
  SimulateMatchingPhase(manager, true);
  // Set up the new login.
  base::string16 new_user = ASCIIToUTF16("newuser");
  base::string16 new_pass = ASCIIToUTF16("newpass");
  credentials.username_value = new_user;
  credentials.password_value = new_pass;
  manager->ProvisionallySave(
      credentials,
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Again, the PasswordFormManager should know this is still a new login.
  EXPECT_TRUE(manager->IsNewLogin());
  // And make sure everything squares up again.
  EXPECT_EQ(observed_form()->origin.spec(),
            GetPendingCredentials(manager)->origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            GetPendingCredentials(manager)->signon_realm);
  EXPECT_TRUE(GetPendingCredentials(manager)->preferred);
  EXPECT_EQ(new_pass,
            GetPendingCredentials(manager)->password_value);
  EXPECT_EQ(new_user,
            GetPendingCredentials(manager)->username_value);
  delete manager;
}

TEST_F(PasswordFormManagerTest, TestUpdatePassword) {
  // Create a PasswordFormManager with observed_form, as if we just
  // saw this form and need to find matching logins.
  scoped_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient(profile(), NULL));
  scoped_ptr<MockPasswordManagerDriver> driver;
  PasswordFormManager* manager = new PasswordFormManager(
      NULL, client.get(), driver.get(), *observed_form(), false);

  SimulateMatchingPhase(manager, true);

  // User submits credentials for the observed form using a username previously
  // stored, but a new password. Note that the observed form may have different
  // origin URL (as it does in this case) than the saved_match, but we want to
  // make sure the updated password is reflected in saved_match, because that is
  // what we autofilled.
  base::string16 new_pass = ASCIIToUTF16("newpassword");
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = new_pass;
  credentials.preferred = true;
  manager->ProvisionallySave(
      credentials,
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(manager->IsNewLogin());

  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db. (This verifies correct
  // behaviour for bug 1074420).
  EXPECT_EQ(GetPendingCredentials(manager)->origin.spec(),
            saved_match()->origin.spec());
  EXPECT_EQ(GetPendingCredentials(manager)->signon_realm,
            saved_match()->signon_realm);
  EXPECT_TRUE(GetPendingCredentials(manager)->preferred);
  EXPECT_EQ(new_pass,
            GetPendingCredentials(manager)->password_value);
  // Done.
  delete manager;
}

TEST_F(PasswordFormManagerTest, TestIgnoreResult) {
  scoped_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient(profile(), NULL));
  scoped_ptr<MockPasswordManagerDriver> driver;
  PasswordFormManager* manager = new PasswordFormManager(
      NULL, client.get(), driver.get(), *observed_form(), false);

  // Make sure we don't match a PasswordForm if it was originally saved on
  // an SSL-valid page and we are now on a page with invalid certificate.
  saved_match()->ssl_valid = true;
  EXPECT_TRUE(IgnoredResult(manager, saved_match()));

  saved_match()->ssl_valid = false;
  // Different paths for action / origin are okay.
  saved_match()->action = GURL("http://www.google.com/b/Login");
  saved_match()->origin = GURL("http://www.google.com/foo");
  EXPECT_FALSE(IgnoredResult(manager, saved_match()));

  // Done.
  delete manager;
}

TEST_F(PasswordFormManagerTest, TestEmptyAction) {
  scoped_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient(profile(), NULL));
  scoped_ptr<MockPasswordManagerDriver> driver;
  scoped_ptr<PasswordFormManager> manager(new PasswordFormManager(
      NULL, client.get(), driver.get(), *observed_form(), false));

  saved_match()->action = GURL();
  SimulateMatchingPhase(manager.get(), true);
  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;
  manager->ProvisionallySave(
      login,
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_FALSE(manager->IsNewLogin());
  // We bless our saved PasswordForm entry with the action URL of the
  // observed form.
  EXPECT_EQ(observed_form()->action,
            GetPendingCredentials(manager.get())->action);
}

TEST_F(PasswordFormManagerTest, TestUpdateAction) {
  scoped_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient(profile(), NULL));
  scoped_ptr<MockPasswordManagerDriver> driver;
  scoped_ptr<PasswordFormManager> manager(new PasswordFormManager(
      NULL, client.get(), driver.get(), *observed_form(), false));

  SimulateMatchingPhase(manager.get(), true);
  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;

  manager->ProvisionallySave(
      login,
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_FALSE(manager->IsNewLogin());
  // The observed action URL is different from the previously saved one, and
  // is the same as the one that would be submitted on successful login.
  EXPECT_NE(observed_form()->action, saved_match()->action);
  EXPECT_EQ(observed_form()->action,
            GetPendingCredentials(manager.get())->action);
}

TEST_F(PasswordFormManagerTest, TestDynamicAction) {
  scoped_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient(profile(), NULL));
  scoped_ptr<MockPasswordManagerDriver> driver;
  scoped_ptr<PasswordFormManager> manager(new PasswordFormManager(
      NULL, client.get(), driver.get(), *observed_form(), false));

  SimulateMatchingPhase(manager.get(), false);
  PasswordForm login(*observed_form());
  // The submitted action URL is different from the one observed on page load.
  GURL new_action = GURL("http://www.google.com/new_action");
  login.action = new_action;

  manager->ProvisionallySave(
      login,
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_TRUE(manager->IsNewLogin());
  // Check that the provisionally saved action URL is the same as the submitted
  // action URL, not the one observed on page load.
  EXPECT_EQ(new_action,
            GetPendingCredentials(manager.get())->action);
}

TEST_F(PasswordFormManagerTest, TestAlternateUsername) {
  // Need a MessageLoop for callbacks.
  base::MessageLoop message_loop;
  scoped_refptr<TestPasswordStore> password_store = new TestPasswordStore;
  CHECK(password_store->Init());

  TestPasswordManagerClient client(profile(), password_store.get());
  TestPasswordManager password_manager(&client);
  scoped_ptr<PasswordFormManager> manager(
      new PasswordFormManager(&password_manager,
                              &client,
                              client.GetDriver(),
                              *observed_form(),
                              false));

  password_store->AddLogin(*saved_match());
  manager->FetchMatchingLoginsFromPasswordStore(PasswordStore::ALLOW_PROMPT);
  content::RunAllPendingInMessageLoop();

  // The saved match has the right username already.
  PasswordForm login(*observed_form());
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;
  login.preferred = true;
  manager->ProvisionallySave(
      login,
      PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(manager->IsNewLogin());
  manager->Save();
  content::RunAllPendingInMessageLoop();

  // Should be only one password stored, and should not have
  // |other_possible_usernames| set anymore.
  TestPasswordStore::PasswordMap passwords = password_store->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[saved_match()->signon_realm].size());
  EXPECT_EQ(saved_match()->username_value,
            passwords[saved_match()->signon_realm][0].username_value);
  EXPECT_EQ(
      0U,
      passwords[saved_match()->signon_realm][0].
      other_possible_usernames.size());

  // This time use an alternate username
  manager.reset(new PasswordFormManager(&password_manager,
                                        &client,
                                        client.GetDriver(),
                                        *observed_form(),
                                        false));
  password_store->Clear();
  password_store->AddLogin(*saved_match());
  manager->FetchMatchingLoginsFromPasswordStore(PasswordStore::ALLOW_PROMPT);
  content::RunAllPendingInMessageLoop();

  base::string16 new_username = saved_match()->other_possible_usernames[0];
  login.username_value = new_username;
  manager->ProvisionallySave(
      login,
      PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(manager->IsNewLogin());
  manager->Save();
  content::RunAllPendingInMessageLoop();

  // |other_possible_usernames| should also be empty, but username_value should
  // be changed to match |new_username|
  passwords = password_store->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[saved_match()->signon_realm].size());
  EXPECT_EQ(new_username,
            passwords[saved_match()->signon_realm][0].username_value);
  EXPECT_EQ(
      0U,
      passwords[saved_match()->signon_realm][0].
      other_possible_usernames.size());
  password_store->Shutdown();
}

TEST_F(PasswordFormManagerTest, TestValidForms) {
  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.scheme = PasswordForm::SCHEME_HTML;
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;

  // Form with both username_element and password_element.
  PasswordFormManager manager1(NULL, NULL, NULL, credentials, false);
  SimulateMatchingPhase(&manager1, false);
  EXPECT_TRUE(manager1.HasValidPasswordForm());

  // Form without a username_element but with a password_element.
  credentials.username_element.clear();
  PasswordFormManager manager2(NULL, NULL, NULL, credentials, false);
  SimulateMatchingPhase(&manager2, false);
  EXPECT_FALSE(manager2.HasValidPasswordForm());

  // Form without a password_element but with a username_element.
  credentials.username_element = saved_match()->username_element;
  credentials.password_element.clear();
  PasswordFormManager manager3(NULL, NULL, NULL, credentials, false);
  SimulateMatchingPhase(&manager3, false);
  EXPECT_FALSE(manager3.HasValidPasswordForm());

  // Form with neither a password_element nor a username_element.
  credentials.username_element.clear();
  credentials.password_element.clear();
  PasswordFormManager manager4(NULL, NULL, NULL, credentials, false);
  SimulateMatchingPhase(&manager4, false);
  EXPECT_FALSE(manager4.HasValidPasswordForm());
}

TEST_F(PasswordFormManagerTest, TestValidFormsBasic) {
  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.scheme = PasswordForm::SCHEME_BASIC;
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;

  // Form with both username_element and password_element.
  PasswordFormManager manager1(NULL, NULL, NULL, credentials, false);
  SimulateMatchingPhase(&manager1, false);
  EXPECT_TRUE(manager1.HasValidPasswordForm());

  // Form without a username_element but with a password_element.
  credentials.username_element.clear();
  PasswordFormManager manager2(NULL, NULL, NULL, credentials, false);
  SimulateMatchingPhase(&manager2, false);
  EXPECT_TRUE(manager2.HasValidPasswordForm());

  // Form without a password_element but with a username_element.
  credentials.username_element = saved_match()->username_element;
  credentials.password_element.clear();
  PasswordFormManager manager3(NULL, NULL, NULL, credentials, false);
  SimulateMatchingPhase(&manager3, false);
  EXPECT_TRUE(manager3.HasValidPasswordForm());

  // Form with neither a password_element nor a username_element.
  credentials.username_element.clear();
  credentials.password_element.clear();
  PasswordFormManager manager4(NULL, NULL, NULL, credentials, false);
  SimulateMatchingPhase(&manager4, false);
  EXPECT_TRUE(manager4.HasValidPasswordForm());
}

TEST_F(PasswordFormManagerTest, TestSendNotBlacklistedMessage) {
  base::MessageLoop message_loop;

  // A dumb password manager.
  TestPasswordManagerClient client(profile(), NULL);
  TestPasswordManager password_manager(&client);
  scoped_ptr<PasswordFormManager> manager(
      new PasswordFormManager(&password_manager,
                              &client,
                              client.GetDriver(),
                              *observed_form(),
                              false));

  // First time sign up attempt; No login result is found from password store;
  // We should send the not blacklisted message.
  EXPECT_CALL(*client.GetMockDriver(), AllowPasswordGenerationForForm(_))
      .Times(1);
  SimulateFetchMatchingLoginsFromPasswordStore(manager.get());
  std::vector<PasswordForm*> result;
  SimulateResponseFromPasswordStore(manager.get(), result);
  Mock::VerifyAndClearExpectations(client.GetMockDriver());

  // Sign up attempt to previously visited sites; Login result is found from
  // password store, and is not blacklisted; We should send the not blacklisted
  // message.
  manager.reset(new PasswordFormManager(&password_manager,
                                        &client,
                                        client.GetDriver(),
                                        *observed_form(),
                                        false));
  EXPECT_CALL(*client.GetMockDriver(), AllowPasswordGenerationForForm(_))
      .Times(1);
  SimulateFetchMatchingLoginsFromPasswordStore(manager.get());
  // We need add heap allocated objects to result.
  result.push_back(CreateSavedMatch(false));
  SimulateResponseFromPasswordStore(manager.get(), result);
  Mock::VerifyAndClearExpectations(client.GetMockDriver());

  // Sign up attempt to previously visited sites; Login result is found from
  // password store, but is blacklisted; We should not send the not blacklisted
  // message.
  manager.reset(new PasswordFormManager(&password_manager,
                                        &client,
                                        client.GetDriver(),
                                        *observed_form(),
                                        false));
  EXPECT_CALL(*client.GetMockDriver(), AllowPasswordGenerationForForm(_))
      .Times(0);
  SimulateFetchMatchingLoginsFromPasswordStore(manager.get());
  result.clear();
  result.push_back(CreateSavedMatch(true));
  SimulateResponseFromPasswordStore(manager.get(), result);
  Mock::VerifyAndClearExpectations(client.GetMockDriver());
}

TEST_F(PasswordFormManagerTest, TestForceInclusionOfGeneratedPasswords) {
  base::MessageLoop message_loop;

  TestPasswordManagerClient client(profile(), NULL);
  TestPasswordManager password_manager(&client);
  scoped_ptr<PasswordFormManager> manager(
      new PasswordFormManager(&password_manager,
                              &client,
                              client.GetDriver(),
                              *observed_form(),
                              false));

  // Simulate having two matches for this origin, one of which was from a form
  // with different HTML tags for elements. Because of scoring differences,
  // only the first form will be sent to Autofill().
  std::vector<PasswordForm*> results;
  results.push_back(CreateSavedMatch(false));
  results.push_back(CreateSavedMatch(false));
  results[1]->username_value = ASCIIToUTF16("other@gmail.com");
  results[1]->password_element = ASCIIToUTF16("signup_password");
  results[1]->username_element = ASCIIToUTF16("signup_username");
  SimulateFetchMatchingLoginsFromPasswordStore(manager.get());
  SimulateResponseFromPasswordStore(manager.get(), results);
  EXPECT_EQ(1u, password_manager.GetLatestBestMatches().size());
  results.clear();

  // Same thing, except this time the credentials that don't match quite as
  // well are generated. They should now be sent to Autofill().
  manager.reset(new PasswordFormManager(&password_manager,
                                        &client,
                                        client.GetDriver(),
                                        *observed_form(),
                                        false));
  results.push_back(CreateSavedMatch(false));
  results.push_back(CreateSavedMatch(false));
  results[1]->username_value = ASCIIToUTF16("other@gmail.com");
  results[1]->password_element = ASCIIToUTF16("signup_password");
  results[1]->username_element = ASCIIToUTF16("signup_username");
  results[1]->type = PasswordForm::TYPE_GENERATED;
  SimulateFetchMatchingLoginsFromPasswordStore(manager.get());
  SimulateResponseFromPasswordStore(manager.get(), results);
  EXPECT_EQ(2u, password_manager.GetLatestBestMatches().size());
}

TEST_F(PasswordFormManagerTest, TestSanitizePossibleUsernames) {
  scoped_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient(profile(), NULL));
  scoped_ptr<MockPasswordManagerDriver> driver;
  scoped_ptr<PasswordFormManager> manager(new PasswordFormManager(
      NULL, client.get(), driver.get(), *observed_form(), false));
  PasswordForm credentials(*observed_form());
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("543-43-1234"));
  credentials.other_possible_usernames.push_back(
      ASCIIToUTF16("378282246310005"));
  credentials.other_possible_usernames.push_back(
      ASCIIToUTF16("other username"));
  credentials.username_value = ASCIIToUTF16("test@gmail.com");

  SanitizePossibleUsernames(manager.get(), &credentials);

  // Possible credit card number and SSN are stripped.
  std::vector<base::string16> expected;
  expected.push_back(ASCIIToUTF16("other username"));
  EXPECT_THAT(credentials.other_possible_usernames, Eq(expected));

  credentials.other_possible_usernames.clear();
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("511-32-9830"));
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("duplicate"));
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("duplicate"));
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("random"));
  credentials.other_possible_usernames.push_back(
      ASCIIToUTF16("test@gmail.com"));

  SanitizePossibleUsernames(manager.get(), &credentials);

  // SSN, duplicate in |other_possible_usernames| and duplicate of
  // |username_value| all removed.
  expected.clear();
  expected.push_back(ASCIIToUTF16("duplicate"));
  expected.push_back(ASCIIToUTF16("random"));
  EXPECT_THAT(credentials.other_possible_usernames, Eq(expected));
}
