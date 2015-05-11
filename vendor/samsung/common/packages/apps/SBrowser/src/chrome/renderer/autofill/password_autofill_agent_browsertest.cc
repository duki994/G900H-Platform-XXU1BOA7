// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_autofill_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/keycodes/keyboard_codes.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebString;
using blink::WebView;

namespace {

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kPasswordName[] = "password";

const char kAliceUsername[] = "alice";
const char kAlicePassword[] = "password";
const char kBobUsername[] = "bob";
const char kBobPassword[] = "secret";
const char kCarolUsername[] = "Carol";
const char kCarolPassword[] = "test";
const char kCarolAlternateUsername[] = "RealCarolUsername";


const char kFormHTML[] =
    "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kVisibleFormHTML[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body>"
    "  <form>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kEmptyFormHTML[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body> <form> </form> </body>";

const char kNonVisibleFormHTML[] =
    "<head> <style> form {display: none;} </style> </head>"
    "<body>"
    "  <form>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kEmptyWebpage[] =
    "<html>"
    "   <head>"
    "   </head>"
    "   <body>"
    "   </body>"
    "</html>";

const char kRedirectionWebpage[] =
    "<html>"
    "   <head>"
    "       <meta http-equiv='Content-Type' content='text/html'>"
    "       <title>Redirection page</title>"
    "       <script></script>"
    "   </head>"
    "   <body>"
    "       <script type='text/javascript'>"
    "         function test(){}"
    "       </script>"
    "   </body>"
    "</html>";

const char kSimpleWebpage[] =
    "<html>"
    "   <head>"
    "       <meta charset='utf-8' />"
    "       <title>Title</title>"
    "   </head>"
    "   <body>"
    "       <form name='LoginTestForm'>"
    "           <input type='text' id='username'/>"
    "           <input type='password' id='password'/>"
    "           <input type='submit' value='Login'/>"
    "       </form>"
    "   </body>"
    "</html>";

const char kWebpageWithDynamicContent[] =
    "<html>"
    "   <head>"
    "       <meta charset='utf-8' />"
    "       <title>Title</title>"
    "   </head>"
    "   <body>"
    "       <script type='text/javascript'>"
    "           function addParagraph() {"
    "             var p = document.createElement('p');"
    "             document.body.appendChild(p);"
    "            }"
    "           window.onload = addParagraph;"
    "       </script>"
    "   </body>"
    "</html>";

const char kJavaScriptClick[] =
    "var event = new MouseEvent('click', {"
    "   'view': window,"
    "   'bubbles': true,"
    "   'cancelable': true"
    "});"
    "var form = document.getElementById('myform1');"
    "form.dispatchEvent(event);"
    "console.log('clicked!');";

const char kOnChangeDetectionScript[] =
    "<script>"
    "  usernameOnchangeCalled = false;"
    "  passwordOnchangeCalled = false;"
    "  document.getElementById('username').onchange = function() {"
    "    usernameOnchangeCalled = true;"
    "  };"
    "  document.getElementById('password').onchange = function() {"
    "    passwordOnchangeCalled = true;"
    "  };"
    "</script>";

}  // namespace

namespace autofill {

class PasswordAutofillAgentTest : public ChromeRenderViewTest {
 public:
  PasswordAutofillAgentTest() {
  }

  // Simulates the fill password form message being sent to the renderer.
  // We use that so we don't have to make RenderView::OnFillPasswordForm()
  // protected.
  void SimulateOnFillPasswordForm(
      const PasswordFormFillData& fill_data) {
    AutofillMsg_FillPasswordForm msg(0, fill_data);
    password_autofill_->OnMessageReceived(msg);
  }

  virtual void SetUp() {
    ChromeRenderViewTest::SetUp();

    // Add a preferred login and an additional login to the FillData.
    username1_ = ASCIIToUTF16(kAliceUsername);
    password1_ = ASCIIToUTF16(kAlicePassword);
    username2_ = ASCIIToUTF16(kBobUsername);
    password2_ = ASCIIToUTF16(kBobPassword);
    username3_ = ASCIIToUTF16(kCarolUsername);
    password3_ = ASCIIToUTF16(kCarolPassword);
    alternate_username3_ = ASCIIToUTF16(kCarolAlternateUsername);

    FormFieldData username_field;
    username_field.name = ASCIIToUTF16(kUsernameName);
    username_field.value = username1_;
    fill_data_.basic_data.fields.push_back(username_field);

    FormFieldData password_field;
    password_field.name = ASCIIToUTF16(kPasswordName);
    password_field.value = password1_;
    password_field.form_control_type = "password";
    fill_data_.basic_data.fields.push_back(password_field);

    PasswordAndRealm password2;
    password2.password = password2_;
    fill_data_.additional_logins[username2_] = password2;
    PasswordAndRealm password3;
    password3.password = password3_;
    fill_data_.additional_logins[username3_] = password3;

    UsernamesCollectionKey key;
    key.username = username3_;
    key.password = password3_;
    key.realm = "google.com";
    fill_data_.other_possible_usernames[key].push_back(alternate_username3_);

    // We need to set the origin so it matches the frame URL and the action so
    // it matches the form action, otherwise we won't autocomplete.
    UpdateOriginForHTML(kFormHTML);
    fill_data_.basic_data.action = GURL("http://www.bidule.com");

    LoadHTML(kFormHTML);

    // Now retrieve the input elements so the test can access them.
    UpdateUsernameAndPasswordElements();
  }

  virtual void TearDown() {
    username_element_.reset();
    password_element_.reset();
    ChromeRenderViewTest::TearDown();
  }

  void UpdateOriginForHTML(const std::string& html) {
    std::string origin = "data:text/html;charset=utf-8," + html;
    fill_data_.basic_data.origin = GURL(origin);
  }

  void UpdateUsernameAndPasswordElements() {
    WebDocument document = GetMainFrame()->document();
    WebElement element =
        document.getElementById(WebString::fromUTF8(kUsernameName));
    ASSERT_FALSE(element.isNull());
    username_element_ = element.to<blink::WebInputElement>();
    element = document.getElementById(WebString::fromUTF8(kPasswordName));
    ASSERT_FALSE(element.isNull());
    password_element_ = element.to<blink::WebInputElement>();
  }

  void ClearUsernameAndPasswordFields() {
    username_element_.setValue("");
    username_element_.setAutofilled(false);
    password_element_.setValue("");
    password_element_.setAutofilled(false);
  }

  void SimulateUsernameChangeForElement(const std::string& username,
                                        bool move_caret_to_end,
                                        WebFrame* input_frame,
                                        WebInputElement& username_input) {
    username_input.setValue(WebString::fromUTF8(username));
    // The field must have focus or AutofillAgent will think the
    // change should be ignored.
    while (!username_input.focused())
      input_frame->document().frame()->view()->advanceFocus(false);
    if (move_caret_to_end)
      username_input.setSelectionRange(username.length(), username.length());
    autofill_agent_->textFieldDidChange(username_input);
    // Processing is delayed because of a Blink bug:
    // https://bugs.webkit.org/show_bug.cgi?id=16976
    // See PasswordAutofillAgent::TextDidChangeInTextField() for details.

    // Autocomplete will trigger a style recalculation when we put up the next
    // frame, but we don't want to wait that long. Instead, trigger a style
    // recalcuation manually after TextFieldDidChangeImpl runs.
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &PasswordAutofillAgentTest::LayoutMainFrame, base::Unretained(this)));

    base::MessageLoop::current()->RunUntilIdle();
  }

  void LayoutMainFrame() {
    GetMainFrame()->view()->layout();
  }

  void SimulateUsernameChange(const std::string& username,
                              bool move_caret_to_end) {
    SimulateUsernameChangeForElement(username, move_caret_to_end,
                                     GetMainFrame(), username_element_);
  }

  // Tests that no suggestion popup is generated when the username_element_ is
  // edited.
  void ExpectNoSuggestionsPopup() {
    // The first test below ensures that the suggestions have been handled by
    // the password_autofill_agent, even though autocomplete='off' is set. The
    // second check ensures that, although handled, no "show suggestions" IPC to
    // the browser was generated.
    //
    // This is interesting in the specific case of an autocomplete='off' form
    // that also has a remembered username and password
    // (http://crbug.com/326679). To fix the DCHECK that this case used to hit,
    // |true| is returned from ShowSuggestions for all forms with valid
    // usersnames that are autocomplete='off', prentending that a selection box
    // has been shown to the user. Of course, it hasn't, so a message is never
    // sent to the browser on acceptance, and the DCHECK isn't hit (and nothing
    // is filled).
    //
    // These tests only make sense in the context of not ignoring
    // autocomplete='off', so only test them if the disable autocomplete='off'
    // flag is not enabled.
    // TODO(jww): Remove this function and callers once autocomplete='off' is
    // permanently ignored.
    if (!ShouldIgnoreAutocompleteOffForPasswordFields()) {
      EXPECT_TRUE(autofill_agent_->password_autofill_agent_->ShowSuggestions(
          username_element_));

      EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
          AutofillHostMsg_ShowPasswordSuggestions::ID));
    }
  }

  void SimulateKeyDownEvent(const WebInputElement& element,
                            ui::KeyboardCode key_code) {
    blink::WebKeyboardEvent key_event;
    key_event.windowsKeyCode = key_code;
    autofill_agent_->textFieldDidReceiveKeyDown(element, key_event);
  }

  void CheckTextFieldsStateForElements(const WebInputElement& username_element,
                                       const std::string& username,
                                       bool username_autofilled,
                                       const WebInputElement& password_element,
                                       const std::string& password,
                                       bool password_autofilled,
                                       bool checkSuggestedValue = true) {
    EXPECT_EQ(username,
              static_cast<std::string>(username_element.value().utf8()));
    EXPECT_EQ(username_autofilled, username_element.isAutofilled());
    EXPECT_EQ(password,
              static_cast<std::string>(
                  checkSuggestedValue ? password_element.suggestedValue().utf8()
                                      : password_element.value().utf8()));
    EXPECT_EQ(password_autofilled, password_element.isAutofilled());
  }

  // Checks the DOM-accessible value of the username element and the
  // *suggested* value of the password element.
  void CheckTextFieldsState(const std::string& username,
                            bool username_autofilled,
                            const std::string& password,
                            bool password_autofilled) {
    CheckTextFieldsStateForElements(username_element_, username,
                                    username_autofilled, password_element_,
                                    password, password_autofilled);
  }

  // Checks the DOM-accessible value of the username element and the
  // DOM-accessible value of the password element.
  void CheckTextFieldsDOMState(const std::string& username,
                               bool username_autofilled,
                               const std::string& password,
                               bool password_autofilled) {
    CheckTextFieldsStateForElements(username_element_,
                                    username,
                                    username_autofilled,
                                    password_element_,
                                    password,
                                    password_autofilled,
                                    false);
  }

  void CheckUsernameSelection(int start, int end) {
    EXPECT_EQ(start, username_element_.selectionStart());
    EXPECT_EQ(end, username_element_.selectionEnd());
  }

  base::string16 username1_;
  base::string16 username2_;
  base::string16 username3_;
  base::string16 password1_;
  base::string16 password2_;
  base::string16 password3_;
  base::string16 alternate_username3_;
  PasswordFormFillData fill_data_;

  WebInputElement username_element_;
  WebInputElement password_element_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillAgentTest);
};

// Tests that the password login is autocompleted as expected when the browser
// sends back the password info.
TEST_F(PasswordAutofillAgentTest, InitialAutocomplete) {
  /*
   * Right now we are not sending the message to the browser because we are
   * loading a data URL and the security origin canAccessPasswordManager()
   * returns false.  May be we should mock URL loading to cirmcuvent this?
   TODO(jcivelli): find a way to make the security origin not deny access to the
                   password manager and then reenable this code.

  // The form has been loaded, we should have sent the browser a message about
  // the form.
  const IPC::Message* msg = render_thread_.sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsParsed::ID);
  ASSERT_TRUE(msg != NULL);

  Tuple1<std::vector<PasswordForm> > forms;
  AutofillHostMsg_PasswordFormsParsed::Read(msg, &forms);
  ASSERT_EQ(1U, forms.a.size());
  PasswordForm password_form = forms.a[0];
  EXPECT_EQ(PasswordForm::SCHEME_HTML, password_form.scheme);
  EXPECT_EQ(ASCIIToUTF16(kUsernameName), password_form.username_element);
  EXPECT_EQ(ASCIIToUTF16(kPasswordName), password_form.password_element);
  */

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that we correctly fill forms having an empty 'action' attribute.
TEST_F(PasswordAutofillAgentTest, InitialAutocompleteForEmptyAction) {
  const char kEmptyActionFormHTML[] =
      "<FORM name='LoginTestForm'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kEmptyActionFormHTML);

  // Retrieve the input elements so the test can access them.
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kUsernameName));
  ASSERT_FALSE(element.isNull());
  username_element_ = element.to<blink::WebInputElement>();
  element = document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Set the expected form origin and action URLs.
  UpdateOriginForHTML(kEmptyActionFormHTML);
  fill_data_.basic_data.action = fill_data_.basic_data.origin;

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that if a password is marked as readonly, neither field is autofilled
// on page load.
TEST_F(PasswordAutofillAgentTest, NoInitialAutocompleteForReadOnlyPassword) {
  password_element_.setAttribute(WebString::fromUTF8("readonly"),
                                 WebString::fromUTF8("true"));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState(std::string(), false, std::string(), false);
}

// Can still fill a password field if the username is set to a value that
// matches.
TEST_F(PasswordAutofillAgentTest,
       AutocompletePasswordForReadonlyUsernameMatched) {
  username_element_.setValue(username3_);
  username_element_.setAttribute(WebString::fromUTF8("readonly"),
                                 WebString::fromUTF8("true"));

  // Filled even though username is not the preferred match.
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsState(UTF16ToUTF8(username3_), false,
                       UTF16ToUTF8(password3_), true);
}

// If a username field is empty and readonly, don't autofill.
TEST_F(PasswordAutofillAgentTest,
       NoAutocompletePasswordForReadonlyUsernameUnmatched) {
  username_element_.setValue(WebString::fromUTF8(""));
  username_element_.setAttribute(WebString::fromUTF8("readonly"),
                                 WebString::fromUTF8("true"));

  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

// Tests that having a non-matching username precludes the autocomplete.
TEST_F(PasswordAutofillAgentTest, NoAutocompleteForFilledFieldUnmatched) {
  username_element_.setValue(WebString::fromUTF8("bogus"));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsState("bogus", false, std::string(), false);
}

// Don't try to complete a prefilled value even if it's a partial match
// to a username.
TEST_F(PasswordAutofillAgentTest, NoPartialMatchForPrefilledUsername) {
  username_element_.setValue(WebString::fromUTF8("ali"));

  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState("ali", false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, InputWithNoForms) {
  const char kNoFormInputs[] =
      "<input type='text' id='username'/>"
      "<input type='password' id='password'/>";
  LoadHTML(kNoFormInputs);

  SimulateOnFillPasswordForm(fill_data_);

  // Input elements that aren't in a <form> won't autofill.
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, NoAutocompleteForTextFieldPasswords) {
  const char kTextFieldPasswordFormHTML[] =
      "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='text' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kTextFieldPasswordFormHTML);

  // Retrieve the input elements so the test can access them.
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kUsernameName));
  ASSERT_FALSE(element.isNull());
  username_element_ = element.to<blink::WebInputElement>();
  element = document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Set the expected form origin URL.
  UpdateOriginForHTML(kTextFieldPasswordFormHTML);

  SimulateOnFillPasswordForm(fill_data_);

  // Fields should still be empty.
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, NoAutocompleteForPasswordFieldUsernames) {
  const char kPasswordFieldUsernameFormHTML[] =
      "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='password' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kPasswordFieldUsernameFormHTML);

  // Retrieve the input elements so the test can access them.
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kUsernameName));
  ASSERT_FALSE(element.isNull());
  username_element_ = element.to<blink::WebInputElement>();
  element = document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Set the expected form origin URL.
  UpdateOriginForHTML(kPasswordFieldUsernameFormHTML);

  SimulateOnFillPasswordForm(fill_data_);

  // Fields should still be empty.
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

// Tests that having a matching username does not preclude the autocomplete.
TEST_F(PasswordAutofillAgentTest, InitialAutocompleteForMatchingFilledField) {
  username_element_.setValue(WebString::fromUTF8(kAliceUsername));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that editing the password clears the autocompleted password field.
TEST_F(PasswordAutofillAgentTest, PasswordClearOnEdit) {
  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate the user changing the username to some unknown username.
  SimulateUsernameChange("alicia", true);

  // The password should have been cleared.
  CheckTextFieldsState("alicia", false, std::string(), false);
}

// Tests that we only autocomplete on focus lost and with a full username match
// when |wait_for_username| is true.
TEST_F(PasswordAutofillAgentTest, WaitUsername) {
  // Simulate the browser sending back the login info.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // No auto-fill should have taken place.
  CheckTextFieldsState(std::string(), false, std::string(), false);

  // No autocomplete should happen when text is entered in the username.
  SimulateUsernameChange("a", true);
  CheckTextFieldsState("a", false, std::string(), false);
  SimulateUsernameChange("al", true);
  CheckTextFieldsState("al", false, std::string(), false);
  SimulateUsernameChange(kAliceUsername, true);
  CheckTextFieldsState(kAliceUsername, false, std::string(), false);

  // Autocomplete should happen only when the username textfield is blurred with
  // a full match.
  username_element_.setValue("a");
  autofill_agent_->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState("a", false, std::string(), false);
  username_element_.setValue("al");
  autofill_agent_->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState("al", false, std::string(), false);
  username_element_.setValue("alices");
  autofill_agent_->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState("alices", false, std::string(), false);
  username_element_.setValue(ASCIIToUTF16(kAliceUsername));
  autofill_agent_->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that inline autocompletion works properly.
TEST_F(PasswordAutofillAgentTest, InlineAutocomplete) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Simulate the user typing in the first letter of 'alice', a stored username.
  SimulateUsernameChange("a", true);
  // Both the username and password text fields should reflect selection of the
  // stored login.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  // And the selection should have been set to 'lice', the last 4 letters.
  CheckUsernameSelection(1, 5);

  // Now the user types the next letter of the same username, 'l'.
  SimulateUsernameChange("al", true);
  // Now the fields should have the same value, but the selection should have a
  // different start value.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  CheckUsernameSelection(2, 5);

  // Test that deleting does not trigger autocomplete.
  SimulateKeyDownEvent(username_element_, ui::VKEY_BACK);
  SimulateUsernameChange("alic", true);
  CheckTextFieldsState("alic", false, std::string(), false);
  CheckUsernameSelection(4, 4);  // No selection.
  // Reset the last pressed key to something other than backspace.
  SimulateKeyDownEvent(username_element_, ui::VKEY_A);

  // Now lets say the user goes astray from the stored username and types the
  // letter 'f', spelling 'alf'.  We don't know alf (that's just sad), so in
  // practice the username should no longer be 'alice' and the selected range
  // should be empty.
  SimulateUsernameChange("alf", true);
  CheckTextFieldsState("alf", false, std::string(), false);
  CheckUsernameSelection(3, 3);  // No selection.

  // Ok, so now the user removes all the text and enters the letter 'b'.
  SimulateUsernameChange("b", true);
  // The username and password fields should match the 'bob' entry.
  CheckTextFieldsState(kBobUsername, true, kBobPassword, true);
  CheckUsernameSelection(1, 3);

  // Then, the user again removes all the text and types an uppercase 'C'.
  SimulateUsernameChange("C", true);
  // The username and password fields should match the 'Carol' entry.
  CheckTextFieldsState(kCarolUsername, true, kCarolPassword, true);
  CheckUsernameSelection(1, 5);
  // The user removes all the text and types a lowercase 'c'.  We only
  // want case-sensitive autocompletion, so the username and the selected range
  // should be empty.
  SimulateUsernameChange("c", true);
  CheckTextFieldsState("c", false, std::string(), false);
  CheckUsernameSelection(1, 1);

  // Check that we complete other_possible_usernames as well.
  SimulateUsernameChange("R", true);
  CheckTextFieldsState(kCarolAlternateUsername, true, kCarolPassword, true);
  CheckUsernameSelection(1, 17);
}

TEST_F(PasswordAutofillAgentTest, IsWebNodeVisibleTest) {
  blink::WebVector<blink::WebFormElement> forms1, forms2, forms3;
  blink::WebFrame* frame;

  LoadHTML(kVisibleFormHTML);
  frame = GetMainFrame();
  frame->document().forms(forms1);
  ASSERT_EQ(1u, forms1.size());
  EXPECT_TRUE(IsWebNodeVisible(forms1[0]));

  LoadHTML(kEmptyFormHTML);
  frame = GetMainFrame();
  frame->document().forms(forms2);
  ASSERT_EQ(1u, forms2.size());
  EXPECT_FALSE(IsWebNodeVisible(forms2[0]));

  LoadHTML(kNonVisibleFormHTML);
  frame = GetMainFrame();
  frame->document().forms(forms3);
  ASSERT_EQ(1u, forms3.size());
  EXPECT_FALSE(IsWebNodeVisible(forms3[0]));
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest) {
  render_thread_->sink().ClearMessages();
  LoadHTML(kVisibleFormHTML);
  const IPC::Message* message = render_thread_->sink()
      .GetFirstMessageMatching(AutofillHostMsg_PasswordFormsRendered::ID);
  EXPECT_TRUE(message);
  Tuple1<std::vector<autofill::PasswordForm> > param;
  AutofillHostMsg_PasswordFormsRendered::Read(message, &param);
  EXPECT_TRUE(param.a.size());

  render_thread_->sink().ClearMessages();
  LoadHTML(kEmptyFormHTML);
  message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID);
  EXPECT_TRUE(message);
  AutofillHostMsg_PasswordFormsRendered::Read(message, &param);
  EXPECT_FALSE(param.a.size());

  render_thread_->sink().ClearMessages();
  LoadHTML(kNonVisibleFormHTML);
  message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID);
  EXPECT_TRUE(message);
  AutofillHostMsg_PasswordFormsRendered::Read(message, &param);
  EXPECT_FALSE(param.a.size());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_Redirection) {
  render_thread_->sink().ClearMessages();
  LoadHTML(kEmptyWebpage);
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID));

  render_thread_->sink().ClearMessages();
  LoadHTML(kRedirectionWebpage);
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID));

  render_thread_->sink().ClearMessages();
  LoadHTML(kSimpleWebpage);
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID));

  render_thread_->sink().ClearMessages();
  LoadHTML(kWebpageWithDynamicContent);
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID));
}

// Tests that a password form in an iframe will not be filled in until a user
// interaction with the form.
TEST_F(PasswordAutofillAgentTest, IframeNoFillTest) {
  const char kIframeName[] = "iframe";
  const char kWebpageWithIframeStart[] =
      "<html>"
      "   <head>"
      "       <meta charset='utf-8' />"
      "       <title>Title</title>"
      "   </head>"
      "   <body>"
      "       <iframe id='iframe' src=\"";
  const char kWebpageWithIframeEnd[] =
      "\"></iframe>"
      "   </body>"
      "</html>";

  std::string origin("data:text/html;charset=utf-8,");
  origin += kSimpleWebpage;

  std::string page_html(kWebpageWithIframeStart);
  page_html += origin;
  page_html += kWebpageWithIframeEnd;

  LoadHTML(page_html.c_str());

  // Set the expected form origin and action URLs.
  fill_data_.basic_data.origin = GURL(origin);
  fill_data_.basic_data.action = GURL(origin);

  SimulateOnFillPasswordForm(fill_data_);

  // Retrieve the input elements from the iframe since that is where we want to
  // test the autofill.
  WebFrame* iframe = GetMainFrame()->findChildByName(kIframeName);
  ASSERT_TRUE(iframe);
  WebDocument document = iframe->document();

  WebElement username_element = document.getElementById(kUsernameName);
  WebElement password_element = document.getElementById(kPasswordName);
  ASSERT_FALSE(username_element.isNull());
  ASSERT_FALSE(password_element.isNull());

  WebInputElement username_input = username_element.to<WebInputElement>();
  WebInputElement password_input = password_element.to<WebInputElement>();
  ASSERT_FALSE(username_element.isNull());

  CheckTextFieldsStateForElements(username_input, "", false,
                                  password_input, "", false);

  // Simulate the user typing in the username in the iframe, which should cause
  // an autofill.
  SimulateUsernameChangeForElement(kAliceUsername, true,
                                   iframe, username_input);

  CheckTextFieldsStateForElements(username_input, kAliceUsername, true,
                                  password_input, kAlicePassword, true);
}

// Tests that a password will only be filled as a suggested and will not be
// accessible by the DOM until a user gesture has occurred.
TEST_F(PasswordAutofillAgentTest, GestureRequiredTest) {
  // Trigger the initial autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);

  // However, it should only have completed with the suggested value, as tested
  // above, and it should not have completed into the DOM accessible value for
  // the password field.
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), true);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Verfies that a DOM-activated UI event will not cause an autofill.
TEST_F(PasswordAutofillAgentTest, NoDOMActivationTest) {
  // Trigger the initial autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  ExecuteJavaScript(kJavaScriptClick);
  CheckTextFieldsDOMState(kAliceUsername, true, "", true);
}

// Regression test for http://crbug.com/326679
TEST_F(PasswordAutofillAgentTest, SelectUsernameWithUsernameAutofillOff) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Set the username element to autocomplete='off'
  username_element_.setAttribute(WebString::fromUTF8("autocomplete"),
                                 WebString::fromUTF8("off"));

  // Simulate the user changing the username to some known username.
  SimulateUsernameChange(kAliceUsername, true);

  ExpectNoSuggestionsPopup();
}

// Regression test for http://crbug.com/326679
TEST_F(PasswordAutofillAgentTest,
       SelectUnknownUsernameWithUsernameAutofillOff) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Set the username element to autocomplete='off'
  username_element_.setAttribute(WebString::fromUTF8("autocomplete"),
                                 WebString::fromUTF8("off"));

  // Simulate the user changing the username to some unknown username.
  SimulateUsernameChange("foo", true);

  ExpectNoSuggestionsPopup();
}

// Regression test for http://crbug.com/326679
TEST_F(PasswordAutofillAgentTest, SelectUsernameWithPasswordAutofillOff) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Set the main password element to autocomplete='off'
  password_element_.setAttribute(WebString::fromUTF8("autocomplete"),
                                 WebString::fromUTF8("off"));

 // Simulate the user changing the username to some known username.
  SimulateUsernameChange(kAliceUsername, true);

  ExpectNoSuggestionsPopup();
}

// Regression test for http://crbug.com/326679
TEST_F(PasswordAutofillAgentTest,
       SelectUnknownUsernameWithPasswordAutofillOff) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Set the main password element to autocomplete='off'
  password_element_.setAttribute(WebString::fromUTF8("autocomplete"),
                                 WebString::fromUTF8("off"));

  // Simulate the user changing the username to some unknown username.
  SimulateUsernameChange("foo", true);

  ExpectNoSuggestionsPopup();
}

// Verifies that password autofill triggers onChange events in JavaScript for
// forms that are filled on page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsOnLoad) {
  std::string html = std::string(kFormHTML) + kOnChangeDetectionScript;
  LoadHTML(html.c_str());
  UpdateOriginForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted...
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  // ... but since there hasn't been a user gesture yet, the autocompleted
  // password should only be visible to the user.
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), true);

  // A JavaScript onChange event should have been triggered for the username,
  // but not yet for the password.
  int username_onchange_called = -1;
  int password_onchange_called = -1;
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("usernameOnchangeCalled ? 1 : 0"),
          &username_onchange_called));
  EXPECT_EQ(1, username_onchange_called);
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("passwordOnchangeCalled ? 1 : 0"),
          &password_onchange_called));
  // TODO(isherman): Re-enable this check once http://crbug.com/333144 is fixed.
  // EXPECT_EQ(0, password_onchange_called);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // Now, a JavaScript onChange event should have been triggered for the
  // password as well.
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("passwordOnchangeCalled ? 1 : 0"),
          &password_onchange_called));
  EXPECT_EQ(1, password_onchange_called);
}

// Verifies that password autofill triggers onChange events in JavaScript for
// forms that are filled after page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsWaitForUsername) {
  std::string html = std::string(kFormHTML) + kOnChangeDetectionScript;
  LoadHTML(html.c_str());
  UpdateOriginForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should not yet have been autocompleted.
  CheckTextFieldsState(std::string(), false, std::string(), false);

  // Simulate a click just to force a user gesture, since the username value is
  // set directly.
  SimulateElementClick(kUsernameName);

  // Simulate the user entering her username.
  username_element_.setValue(ASCIIToUTF16(kAliceUsername), true);
  autofill_agent_->textFieldDidEndEditing(username_element_);

  // The username and password should now have been autocompleted.
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // JavaScript onChange events should have been triggered both for the username
  // and for the password.
  int username_onchange_called = -1;
  int password_onchange_called = -1;
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("usernameOnchangeCalled ? 1 : 0"),
          &username_onchange_called));
  EXPECT_EQ(1, username_onchange_called);
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("passwordOnchangeCalled ? 1 : 0"),
          &password_onchange_called));
  EXPECT_EQ(1, password_onchange_called);
}

}  // namespace autofill
