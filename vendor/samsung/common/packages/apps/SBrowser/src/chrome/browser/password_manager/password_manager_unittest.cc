// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_driver.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class PasswordGenerationManager;

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;
using testing::DoAll;
using testing::Exactly;
using testing::Return;
using testing::WithArg;

namespace autofill {
class AutofillManager;
}

namespace {

class MockPasswordManagerClient : public PasswordManagerClient {
 public:
  MOCK_METHOD1(PromptUserToSavePassword, void(PasswordFormManager*));
  MOCK_METHOD0(GetProfile, Profile*());
  MOCK_METHOD0(GetPasswordStore, PasswordStore*());
  MOCK_METHOD0(GetPrefs, PrefService*());
  MOCK_METHOD0(GetDriver, PasswordManagerDriver*());
  MOCK_METHOD1(GetProbabilityForExperiment,
               base::FieldTrial::Probability(const std::string&));

  // The following is required because GMock does not support move-only
  // parameters.
  MOCK_METHOD1(AuthenticateAutofillAndFillFormPtr,
               void(autofill::PasswordFormFillData* fill_data));
  virtual void AuthenticateAutofillAndFillForm(
      scoped_ptr<autofill::PasswordFormFillData> fill_data) OVERRIDE {
    return AuthenticateAutofillAndFillFormPtr(fill_data.release());
  }
  
  #if defined(S_AUTOCOMPLETE_ALERT_POPUP)
  virtual void ShowAutoCompleteAlertPopUp() OVERRIDE { }
  #endif
};

class MockPasswordManagerDriver : public PasswordManagerDriver {
 public:
  MOCK_METHOD1(FillPasswordForm, void(const autofill::PasswordFormFillData&));
  MOCK_METHOD0(DidLastPageLoadEncounterSSLErrors, bool());
  MOCK_METHOD0(IsOffTheRecord, bool());
  MOCK_METHOD0(GetPasswordGenerationManager, PasswordGenerationManager*());
  MOCK_METHOD0(GetPasswordManager, PasswordManager*());
  MOCK_METHOD0(GetAutofillManager, autofill::AutofillManager*());
  MOCK_METHOD1(AllowPasswordGenerationForForm,
               void(autofill::PasswordForm* form));
};

ACTION_P(InvokeConsumer, forms) {
  arg0->OnGetPasswordStoreResults(forms);
}

ACTION_P(SaveToScopedPtr, scoped) {
  scoped->reset(arg0);
}

class TestPasswordManager : public PasswordManager {
 public:
  explicit TestPasswordManager(PasswordManagerClient* client)
      : PasswordManager(client) {}
  virtual ~TestPasswordManager() {}

  virtual void OnPasswordFormSubmitted(const PasswordForm& form) OVERRIDE {
    PasswordManager::OnPasswordFormSubmitted(form);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPasswordManager);
};

}  // namespace

class PasswordManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    store_ = new MockPasswordStore;
    CHECK(store_->Init());

    EXPECT_CALL(client_, GetPasswordStore()).WillRepeatedly(Return(store_));
    EXPECT_CALL(client_, GetPrefs())
        .WillRepeatedly(Return(profile()->GetTestingPrefService()));
    EXPECT_CALL(client_, GetDriver()).WillRepeatedly(Return(&driver_));

    manager_.reset(new TestPasswordManager(&client_));

    EXPECT_CALL(driver_, DidLastPageLoadEncounterSSLErrors())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(driver_, GetPasswordGenerationManager())
        .WillRepeatedly(Return(static_cast<PasswordGenerationManager*>(NULL)));
    EXPECT_CALL(driver_, GetPasswordManager())
        .WillRepeatedly(Return(manager_.get()));
  }

  virtual void TearDown() {
    store_->Shutdown();
    store_ = NULL;

    // Destroy the PasswordManager before tearing down the Profile to avoid
    // crashes due to prefs accesses.
    manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PasswordForm MakeSimpleForm() {
    PasswordForm form;
    form.origin = GURL("http://www.google.com/a/LoginAuth");
    form.action = GURL("http://www.google.com/a/Login");
    form.username_element = ASCIIToUTF16("Email");
    form.password_element = ASCIIToUTF16("Passwd");
    form.username_value = ASCIIToUTF16("google");
    form.password_value = ASCIIToUTF16("password");
    // Default to true so we only need to add tests in autocomplete=off cases.
    form.password_autocomplete_set = true;
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "http://www.google.com";
    return form;
  }

  // Reproduction of the form present on twitter's login page.
  PasswordForm MakeTwitterLoginForm() {
    PasswordForm form;
    form.origin = GURL("https://twitter.com/");
    form.action = GURL("https://twitter.com/sessions");
    form.username_element = ASCIIToUTF16("Email");
    form.password_element = ASCIIToUTF16("Passwd");
    form.username_value = ASCIIToUTF16("twitter");
    form.password_value = ASCIIToUTF16("password");
    form.password_autocomplete_set = true;
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "https://twitter.com";
    return form;
  }

  // Reproduction of the form present on twitter's failed login page.
  PasswordForm MakeTwitterFailedLoginForm() {
    PasswordForm form;
    form.origin =
        GURL("https://twitter.com/login/error?redirect_after_login");
    form.action = GURL("https://twitter.com/sessions");
    form.username_element = ASCIIToUTF16("EmailField");
    form.password_element = ASCIIToUTF16("PasswdField");
    form.username_value = ASCIIToUTF16("twitter");
    form.password_value = ASCIIToUTF16("password");
    form.password_autocomplete_set = true;
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "https://twitter.com";
    return form;
  }

  bool FormsAreEqual(const autofill::PasswordForm& lhs,
                     const autofill::PasswordForm& rhs) {
    if (lhs.origin != rhs.origin)
      return false;
    if (lhs.action != rhs.action)
      return false;
    if (lhs.username_element != rhs.username_element)
      return false;
    if (lhs.password_element != rhs.password_element)
      return false;
    if (lhs.username_value != rhs.username_value)
      return false;
    if (lhs.password_value != rhs.password_value)
      return false;
    if (lhs.password_autocomplete_set != rhs.password_autocomplete_set)
      return false;
    if (lhs.submit_element != rhs.submit_element)
      return false;
    if (lhs.signon_realm != rhs.signon_realm)
      return false;
    return true;
  }

  TestPasswordManager* manager() { return manager_.get(); }

  void OnPasswordFormSubmitted(const autofill::PasswordForm& form) {
    manager()->OnPasswordFormSubmitted(form);
  }

  PasswordManager::PasswordSubmittedCallback SubmissionCallback() {
    return base::Bind(&PasswordManagerTest::FormSubmitted,
                      base::Unretained(this));
  }

  void FormSubmitted(const autofill::PasswordForm& form) {
    submitted_form_ = form;
  }

  scoped_refptr<MockPasswordStore> store_;
  MockPasswordManagerClient client_;
  MockPasswordManagerDriver driver_;
  scoped_ptr<TestPasswordManager> manager_;
  PasswordForm submitted_form_;
};

MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm &&
         form.origin == arg.origin &&
         form.action == arg.action &&
         form.username_element == arg.username_element &&
         form.password_element == arg.password_element &&
         form.password_autocomplete_set ==
             arg.password_autocomplete_set &&
         form.submit_element == arg.submit_element;
}

TEST_F(PasswordManagerTest, FormSubmitEmptyStore) {
  // Test that observing a newly submitted form shows the save password bar.
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillOnce(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  // And the form submit contract is to call ProvisionallySavePassword.
  manager()->ProvisionallySavePassword(form);

  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_, PromptUserToSavePassword(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(observed);  // The post-navigation layout.

  ASSERT_TRUE(form_to_save.get());
  EXPECT_CALL(*store_.get(), AddLogin(FormMatches(form)));

  // Simulate saving the form, as if the info bar was accepted.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, GeneratedPasswordFormSubmitEmptyStore) {
  // This test is the same FormSubmitEmptyStore, except that it simulates the
  // user generating the password through the browser.
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillOnce(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  // Simulate the user generating the password and submitting the form.
  manager()->SetFormHasGeneratedPassword(form);
  manager()->ProvisionallySavePassword(form);

  // The user should not be presented with an infobar as they have already given
  // consent by using the generated password. The form should be saved once
  // navigation occurs.
  EXPECT_CALL(client_, PromptUserToSavePassword(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), AddLogin(FormMatches(form)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(observed);  // The post-navigation layout.
}

TEST_F(PasswordManagerTest, FormSubmitNoGoodMatch) {
  // Same as above, except with an existing form for the same signon realm,
  // but different origin.  Detailed cases like this are covered by
  // PasswordFormManagerTest.
  std::vector<PasswordForm*> result;
  PasswordForm* existing_different = new PasswordForm(MakeSimpleForm());
  existing_different->username_value = ASCIIToUTF16("google2");
  result.push_back(existing_different);
  EXPECT_CALL(driver_, FillPasswordForm(_));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillOnce(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.
  manager()->ProvisionallySavePassword(form);

  // We still expect an add, since we didn't have a good match.
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_, PromptUserToSavePassword(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(observed);  // The post-navigation layout.

  ASSERT_TRUE(form_to_save.get());
  EXPECT_CALL(*store_.get(), AddLogin(FormMatches(form)));

  // Simulate saving the form.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, FormSeenThenLeftPage) {
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillOnce(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  // No message from the renderer that a password was submitted. No
  // expected calls.
  EXPECT_CALL(client_, PromptUserToSavePassword(_)).Times(0);
  observed.clear();
  manager()->OnPasswordFormsParsed(observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(observed);  // The post-navigation layout.
}

TEST_F(PasswordManagerTest, FormSubmitAfterNavigateInPage) {
  // Test that navigating in the page does not prevent us from showing the save
  // password infobar.
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillOnce(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  // Simulate navigating in the page.
  manager()->DidNavigateMainFrame(true);

  // Simulate submitting the password.
  OnPasswordFormSubmitted(form);

  // Now the password manager waits for the navigation to complete.
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_, PromptUserToSavePassword(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  observed.clear();
  manager()->OnPasswordFormsParsed(observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(observed);  // The post-navigation layout.

  ASSERT_FALSE(NULL == form_to_save.get());
  EXPECT_CALL(*store_.get(), AddLogin(FormMatches(form)));

  // Simulate saving the form, as if the info bar was accepted.
  form_to_save->Save();
}

// This test verifies a fix for http://crbug.com/236673
TEST_F(PasswordManagerTest, FormSubmitWithFormOnPreviousPage) {
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillRepeatedly(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  PasswordForm first_form(MakeSimpleForm());
  first_form.origin = GURL("http://www.nytimes.com/");
  first_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  first_form.signon_realm = "http://www.nytimes.com/";
  PasswordForm second_form(MakeSimpleForm());
  second_form.origin = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.signon_realm = "https://myaccount.nytimes.com/";

  // Pretend that the form is hidden on the first page.
  std::vector<PasswordForm> observed;
  observed.push_back(first_form);
  manager()->OnPasswordFormsParsed(observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(observed);

  // Now navigate to a second page.
  manager()->DidNavigateMainFrame(false);

  // This page contains a form with the same markup, but on a different
  // URL.
  observed.push_back(second_form);
  manager()->OnPasswordFormsParsed(observed);
  manager()->OnPasswordFormsRendered(observed);

  // Now submit this form
  OnPasswordFormSubmitted(second_form);

  // Navigation after form submit.
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_, PromptUserToSavePassword(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));
  observed.clear();
  manager()->OnPasswordFormsParsed(observed);
  manager()->OnPasswordFormsRendered(observed);

  // Make sure that the saved form matches the second form, not the first.
  ASSERT_TRUE(form_to_save.get());
  EXPECT_CALL(*store_.get(), AddLogin(FormMatches(second_form)));

  // Simulate saving the form, as if the info bar was accepted.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, FormSubmitFailedLogin) {
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillRepeatedly(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  manager()->ProvisionallySavePassword(form);

  // The form reappears, and is visible in the layout:
  // No expected calls to the PasswordStore...
  manager()->OnPasswordFormsParsed(observed);
  manager()->OnPasswordFormsRendered(observed);
}

TEST_F(PasswordManagerTest, FormSubmitInvisibleLogin) {
  // Tests fix of issue 28911: if the login form reappears on the subsequent
  // page, but is invisible, it shouldn't count as a failed login.
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillRepeatedly(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  manager()->ProvisionallySavePassword(form);

  // Expect info bar to appear:
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_, PromptUserToSavePassword(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // The form reappears, but is not visible in the layout:
  manager()->OnPasswordFormsParsed(observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(observed);

  ASSERT_TRUE(form_to_save.get());
  EXPECT_CALL(*store_.get(), AddLogin(FormMatches(form)));

  // Simulate saving the form.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, InitiallyInvisibleForm) {
  // Make sure an invisible login form still gets autofilled.
  std::vector<PasswordForm*> result;
  PasswordForm* existing = new PasswordForm(MakeSimpleForm());
  result.push_back(existing);
  EXPECT_CALL(driver_, FillPasswordForm(_));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillRepeatedly(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  observed.clear();
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  manager()->OnPasswordFormsParsed(observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(observed);  // The post-navigation layout.
}

TEST_F(PasswordManagerTest, SavingDependsOnManagerEnabledPreference) {
  // Test that saving passwords depends on the password manager enabled
  // preference.
  TestingPrefServiceSyncable* prefService = profile()->GetTestingPrefService();
  prefService->SetUserPref(prefs::kPasswordManagerEnabled,
                           base::Value::CreateBooleanValue(true));
  EXPECT_TRUE(manager()->IsSavingEnabled());
  prefService->SetUserPref(prefs::kPasswordManagerEnabled,
                           base::Value::CreateBooleanValue(false));
  EXPECT_FALSE(manager()->IsSavingEnabled());
}

TEST_F(PasswordManagerTest, FillPasswordsOnDisabledManager) {
  // Test fix for issue 158296: Passwords must be filled even if the password
  // manager is disabled.
  std::vector<PasswordForm*> result;
  PasswordForm* existing = new PasswordForm(MakeSimpleForm());
  result.push_back(existing);
  TestingPrefServiceSyncable* prefService = profile()->GetTestingPrefService();
  prefService->SetUserPref(prefs::kPasswordManagerEnabled,
                           base::Value::CreateBooleanValue(false));
  EXPECT_CALL(driver_, FillPasswordForm(_));
  EXPECT_CALL(*store_.get(),
              GetLogins(_, testing::Eq(PasswordStore::DISALLOW_PROMPT), _))
      .WillRepeatedly(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);
}

TEST_F(PasswordManagerTest, FormSavedWithAutocompleteOff) {
  // Test password form with non-generated password will be saved even if
  // autocomplete=off.
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillOnce(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  form.password_autocomplete_set = false;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  // And the form submit contract is to call ProvisionallySavePassword.
  manager()->ProvisionallySavePassword(form);

  // Password form should be saved.
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_, PromptUserToSavePassword(_)).Times(Exactly(1)).WillOnce(
      WithArg<0>(SaveToScopedPtr(&form_to_save)));
  EXPECT_CALL(*store_.get(), AddLogin(FormMatches(form))).Times(Exactly(0));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(observed);  // The post-navigation layout.

  ASSERT_TRUE(form_to_save.get());
}

TEST_F(PasswordManagerTest, GeneratedPasswordFormSavedAutocompleteOff) {
  // Test password form with generated password will still be saved if
  // autocomplete=off.
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillOnce(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  form.password_autocomplete_set = false;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  // Simulate the user generating the password and submitting the form.
  manager()->SetFormHasGeneratedPassword(form);
  manager()->ProvisionallySavePassword(form);

  // The user should not be presented with an infobar as they have already given
  // consent by using the generated password. The form should be saved once
  // navigation occurs.
  EXPECT_CALL(client_, PromptUserToSavePassword(_)).Times(Exactly(0));
  EXPECT_CALL(*store_.get(), AddLogin(FormMatches(form)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(observed);  // The post-navigation layout.
}

TEST_F(PasswordManagerTest, SubmissionCallbackTest) {
  manager()->AddSubmissionCallback(SubmissionCallback());
  PasswordForm form = MakeSimpleForm();
  OnPasswordFormSubmitted(form);
  EXPECT_TRUE(FormsAreEqual(form, submitted_form_));
}

TEST_F(PasswordManagerTest, PasswordFormReappearance) {
  // Test the heuristic to know if a password form reappears.
  // We assume that if we send our credentials and there
  // is at least one visible password form in the next page that
  // means that our previous login attempt failed.
  std::vector<PasswordForm*> result;  // Empty password store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  EXPECT_CALL(*store_.get(), GetLogins(_, _, _))
      .WillRepeatedly(DoAll(WithArg<2>(InvokeConsumer(result)), Return()));
  std::vector<PasswordForm> observed;
  PasswordForm login_form(MakeTwitterLoginForm());
  observed.push_back(login_form);
  manager()->OnPasswordFormsParsed(observed);  // The initial load.
  manager()->OnPasswordFormsRendered(observed);  // The initial layout.

  manager()->ProvisionallySavePassword(login_form);

  PasswordForm failed_login_form(MakeTwitterFailedLoginForm());
  observed.clear();
  observed.push_back(failed_login_form);
  // A PasswordForm appears, and is visible in the layout:
  // No expected calls to the PasswordStore...
  manager()->OnPasswordFormsParsed(observed);
  manager()->OnPasswordFormsRendered(observed);
}
