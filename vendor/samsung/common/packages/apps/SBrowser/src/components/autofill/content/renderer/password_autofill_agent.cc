// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebPasswordFormData.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(S_OPEN_SOURCE_266793003_PATCH)
#include "content/public/common/page_transition_types.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#endif

namespace autofill {
namespace {

// The size above which we stop triggering autocomplete.
static const size_t kMaximumTextSizeForAutocomplete = 1000;

// Maps element names to the actual elements to simplify form filling.
typedef std::map<base::string16, blink::WebInputElement>
    FormInputElementMap;

// Utility struct for form lookup and autofill. When we parse the DOM to look up
// a form, in addition to action and origin URL's we have to compare all
// necessary form elements. To avoid having to look these up again when we want
// to fill the form, the FindFormElements function stores the pointers
// in a FormElements* result, referenced to ensure they are safe to use.
struct FormElements {
  blink::WebFormElement form_element;
  FormInputElementMap input_elements;
};

typedef std::vector<FormElements*> FormElementsList;

// Helper to search the given form element for the specified input elements
// in |data|, and add results to |result|.
static bool FindFormInputElements(blink::WebFormElement* fe,
                                  const FormData& data,
                                  FormElements* result) {
  // Loop through the list of elements we need to find on the form in order to
  // autofill it. If we don't find any one of them, abort processing this
  // form; it can't be the right one.
  for (size_t j = 0; j < data.fields.size(); j++) {
    blink::WebVector<blink::WebNode> temp_elements;
    fe->getNamedElements(data.fields[j].name, temp_elements);

    // Match the first input element, if any.
    // |getNamedElements| may return non-input elements where the names match,
    // so the results are filtered for input elements.
    // If more than one match is made, then we have ambiguity (due to misuse
    // of "name" attribute) so is it considered not found.
    bool found_input = false;
    for (size_t i = 0; i < temp_elements.size(); ++i) {
      if (temp_elements[i].to<blink::WebElement>().hasTagName("input")) {
        // Check for a non-unique match.
        if (found_input) {
          found_input = false;
          break;
        }

        // Only fill saved passwords into password fields and usernames into
        // text fields.
        blink::WebInputElement input_element =
            temp_elements[i].to<blink::WebInputElement>();
        if (input_element.isPasswordField() !=
            (data.fields[j].form_control_type == "password"))
          continue;

        // This element matched, add it to our temporary result. It's possible
        // there are multiple matches, but for purposes of identifying the form
        // one suffices and if some function needs to deal with multiple
        // matching elements it can get at them through the FormElement*.
        // Note: This assignment adds a reference to the InputElement.
        result->input_elements[data.fields[j].name] = input_element;
        found_input = true;
      }
    }

    // A required element was not found. This is not the right form.
    // Make sure no input elements from a partially matched form in this
    // iteration remain in the result set.
    // Note: clear will remove a reference from each InputElement.
    if (!found_input) {
      result->input_elements.clear();
      return false;
    }
  }
  return true;
}

// Helper to locate form elements identified by |data|.
void FindFormElements(blink::WebView* view,
                      const FormData& data,
                      FormElementsList* results) {
  DCHECK(view);
  DCHECK(results);
  blink::WebFrame* main_frame = view->mainFrame();
  if (!main_frame)
    return;

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();

  // Loop through each frame.
  for (blink::WebFrame* f = main_frame; f; f = f->traverseNext(false)) {
    blink::WebDocument doc = f->document();
    if (!doc.isHTMLDocument())
      continue;

    GURL full_origin(doc.url());
    if (data.origin != full_origin.ReplaceComponents(rep))
      continue;

    blink::WebVector<blink::WebFormElement> forms;
    doc.forms(forms);

    for (size_t i = 0; i < forms.size(); ++i) {
      blink::WebFormElement fe = forms[i];

#if defined(S_FP_MSSITES_AUTOFILL_FIX)
      blink::WebString action = fe.action();
      if (action.isNull()){
          action = "";
      }
      GURL full_action(f->document().completeURL(action));
#else
      GURL full_action(f->document().completeURL(fe.action()));
#endif
      if (full_action.is_empty()) {
        // The default action URL is the form's origin.
        full_action = full_origin;
      }

      // Action URL must match.
      if (data.action != full_action.ReplaceComponents(rep))
        continue;

      scoped_ptr<FormElements> curr_elements(new FormElements);
      if (!FindFormInputElements(&fe, data, curr_elements.get()))
        continue;

      // We found the right element.
      // Note: this assignment adds a reference to |fe|.
      curr_elements->form_element = fe;
      results->push_back(curr_elements.release());
    }
  }
}

bool IsElementEditable(const blink::WebInputElement& element) {
  return element.isEnabled() && !element.isReadOnly();
}

void SetElementAutofilled(blink::WebInputElement* element, bool autofilled) {
  if (element->isAutofilled() == autofilled)
    return;
  element->setAutofilled(autofilled);
  // Notify any changeEvent listeners.
  element->dispatchFormControlChangeEvent();
}

bool DoUsernamesMatch(const base::string16& username1,
                      const base::string16& username2,
                      bool exact_match) {

  if (exact_match){
  	#if !defined(S_FP_INVALID_EMAIL_USERNAME_FIX)
	return username1 == username2;
	#else
	bool full_match = (username1 == username2);
	LOG(INFO)<<"FP DoUsernamesMatch full"<<full_match<<" "<<username1<<" "<<username2;
	if(full_match)
           return true;
  	bool partial_match = false;
  	std::string currUsername = base::UTF16ToUTF8(username2);
	std::size_t found = currUsername.find("@");
	if(found==std::string::npos)
		return false;
	std::string s;
	s.assign(currUsername, 0, found);
	base::string16 username_stripped_value = base::UTF8ToUTF16(s);
	partial_match = (username_stripped_value == username1);
	LOG(INFO)<<"FP DoUsernamesMatch partial"<<partial_match;
	return partial_match;
	#endif
  }
  return StartsWith(username1, username2, true);
}

// Returns |true| if the given element is both editable and has permission to be
// autocompleted. The latter can be either because there is no
// autocomplete='off' set for the element, or because the flag is set to ignore
// autocomplete='off'. Otherwise, returns |false|.
#if !defined(S_AUTOCOMPLETE_IGNORE)
bool IsElementAutocompletable(const blink::WebInputElement& element) {
  return IsElementEditable(element) &&
         (ShouldIgnoreAutocompleteOffForPasswordFields() ||
          element.autoComplete());
}
#endif

// Returns true if the password specified in |form| is a default value.
bool PasswordValueIsDefault(const PasswordForm& form,
                            blink::WebFormElement form_element) {
  blink::WebVector<blink::WebNode> temp_elements;
  form_element.getNamedElements(form.password_element, temp_elements);

  // We are loose in our definition here and will return true if any of the
  // appropriately named elements match the element to be saved. Currently
  // we ignore filling passwords where naming is ambigious anyway.
  for (size_t i = 0; i < temp_elements.size(); ++i) {
    if (temp_elements[i].to<blink::WebElement>().getAttribute("value") ==
        form.password_value)
      return true;
  }
  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, public:

PasswordAutofillAgent::PasswordAutofillAgent(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      usernames_usage_(NOTHING_TO_AUTOFILL),
      web_view_(render_view->GetWebView()),
      gesture_handler_(new AutofillWebUserGestureHandler(this)),
      user_gesture_occurred_(false),
#if defined(S_FP_MSSITES_AUTOFILL_FIX)
      will_send_submit_(false),
#endif
      weak_ptr_factory_(this) {
  blink::WebUserGestureIndicator::setHandler(gesture_handler_.get());
}

PasswordAutofillAgent::~PasswordAutofillAgent() {
  DCHECK(gesture_handler_.get());
  blink::WebUserGestureIndicator::setHandler(NULL);
}

#if defined(S_AUTOCOMPLETE_IGNORE)
bool PasswordAutofillAgent::IsElementAutocompletable(const blink::WebInputElement& element) {
  return IsElementEditable(element) &&
         (web_view_->settings()->autocompleteIgnore() ||
          element.autoComplete());
}
#endif

bool PasswordAutofillAgent::TextFieldDidEndEditing(
    const blink::WebInputElement& element) {
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  const PasswordFormFillData& fill_data =
      iter->second.fill_data;

  // If wait_for_username is false, we should have filled when the text changed.
  if (!fill_data.wait_for_username)
    return false;

  blink::WebInputElement password = iter->second.password_field;
  if (!IsElementEditable(password))
    return false;

  blink::WebInputElement username = element;  // We need a non-const.

  // Do not set selection when ending an editing session, otherwise it can
  // mess with focus.
  FillUserNameAndPassword(&username, &password, fill_data,
                          true /* exact_username_match */,
                          false /* set_selection */);
  return true;
}

#if defined(S_FP_HIDDEN_FORM_FIX)
void PasswordAutofillAgent::CheckFormVisibilityAndAutofill(){

     LOG(INFO)<<"FP CheckFormsVisibilityAndDoAutofil";
     LoginToPasswordInfoMap::const_iterator iter;
     for(iter = login_to_password_info_.begin(); iter!=login_to_password_info_.end();++iter){
        LOG(INFO)<<"FP login_to_password_info";
        //Check If the Password field is Focusable.
        if(iter->second.password_field.isFocusable()){
           LOG(INFO)<<"FP Form is visible now So, Do Autofill";
           //If User has opted for Extra Authentication, Send Request to launch FingerPrint Activity
           if(iter->second.fill_data.authentication_required)
              Send(new AutofillHostMsg_HiddenFormAutofill(routing_id(), iter->second.fill_data));
           else if(!iter->second.fill_data.manual_autofill){
              blink::WebElement element = iter->first;
              blink::WebInputElement username = element.to<blink::WebInputElement>();
              if(IsElementAutocompletable(username))
                 username.setValue(iter->second.fill_data.basic_data.fields[0].value, true);
              blink::WebInputElement password = iter->second.password_field;
              FillUserNameAndPassword(&username, &password, iter->second.fill_data,
                                         true /* exact_username_match */,
                                         false /* set_selection */);
           }
           break;
        }
     }
}
#endif

bool PasswordAutofillAgent::TextDidChangeInTextField(
    const blink::WebInputElement& element) {
#if defined(S_FP_COPY_OVER_PASSWORD_EXTENDED_FIX)
  if(element.isPasswordField() && !element.form().isNull()){
        blink::WebFrame* frame = element.document().frame();
        form_ID_attr_[frame] = GetFormIdentifier(element.form());
        scoped_ptr<PasswordForm> password_form(CreatePasswordForm(element.form()));
        if (password_form)
            provisionally_saved_forms_[frame].reset(password_form.release());
  }
#endif
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  // The input text is being changed, so any autofilled password is now
  // outdated.
  blink::WebInputElement username = element;  // We need a non-const.
  blink::WebInputElement password = iter->second.password_field;
  SetElementAutofilled(&username, false);
  if (password.isAutofilled()) {
    password.setValue(base::string16());
    SetElementAutofilled(&password, false);
  }

  // If wait_for_username is true we will fill when the username loses focus.
  if (iter->second.fill_data.wait_for_username)
    return false;

  if (!element.isText() || !IsElementAutocompletable(element) ||
      !IsElementAutocompletable(password)) {
    return false;
  }

  // Don't inline autocomplete if the user is deleting, that would be confusing.
  // But refresh the popup.  Note, since this is ours, return true to signal
  // no further processing is required.
  if (iter->second.backspace_pressed_last) {
    ShowSuggestionPopup(iter->second.fill_data, username);
    return true;
  }

  blink::WebString name = element.nameForAutofill();
  if (name.isEmpty())
    return false;  // If the field has no name, then we won't have values.

  // Don't attempt to autofill with values that are too large.
  if (element.value().length() > kMaximumTextSizeForAutocomplete)
    return false;

  // The caret position should have already been updated.
  PerformInlineAutocomplete(element, password, iter->second.fill_data);
  return true;
}

bool PasswordAutofillAgent::TextFieldHandlingKeyDown(
    const blink::WebInputElement& element,
    const blink::WebKeyboardEvent& event) {
  // If using the new Autofill UI that lives in the browser, it will handle
  // keypresses before this function. This is not currently an issue but if
  // the keys handled there or here change, this issue may appear.

  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  int win_key_code = event.windowsKeyCode;
  iter->second.backspace_pressed_last =
      (win_key_code == ui::VKEY_BACK || win_key_code == ui::VKEY_DELETE);
  return true;
}

bool PasswordAutofillAgent::DidAcceptAutofillSuggestion(
    const blink::WebNode& node,
    const blink::WebString& username) {
  blink::WebInputElement input;
  PasswordInfo password;
  if (!FindLoginInfo(node, &input, &password))
    return false;

  // Set the incoming |username| in the text field and |FillUserNameAndPassword|
  // will do the rest.
  input.setValue(username, true);
  return FillUserNameAndPassword(&input, &password.password_field,
                                 password.fill_data,
                                 true /* exact_username_match */,
                                 true /* set_selection */);
}

bool PasswordAutofillAgent::DidClearAutofillSelection(
    const blink::WebNode& node) {
  blink::WebInputElement input;
  PasswordInfo password;
  return FindLoginInfo(node, &input, &password);
}

bool PasswordAutofillAgent::ShowSuggestions(
    const blink::WebInputElement& element) {
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  // If autocomplete='off' is set on the form elements, no suggestion dialog
  // should be shown. However, return |true| to indicate that this is a known
  // password form and that the request to show suggestions has been handled (as
  // a no-op).
  if (!IsElementAutocompletable(element) ||
      !IsElementAutocompletable(iter->second.password_field))
    return true;

  return ShowSuggestionPopup(iter->second.fill_data, element);
}

bool PasswordAutofillAgent::OriginCanAccessPasswordManager(
    const blink::WebSecurityOrigin& origin) {
  return origin.canAccessPasswordManager();
}

void PasswordAutofillAgent::OnDynamicFormsSeen(blink::WebFrame* frame) {

#if defined(S_FP_MSSITES_AUTOFILL_FIX)
LOG(INFO)<<"FP:PasswordAutofillAgent::SendPasswordForms OnDynamicFormsSeen will_send_submit_ ="<<will_send_submit_;
  if(!will_send_submit_)
#endif
  SendPasswordForms(frame, false /* only_visible */);
}

void PasswordAutofillAgent::SendPasswordForms(blink::WebFrame* frame,
                                              bool only_visible) {
  LOG(INFO)<<"FP:PasswordAutofillAgent::SendPasswordForms only_visible ="<<only_visible;
  // Make sure that this security origin is allowed to use password manager.
  blink::WebSecurityOrigin origin = frame->document().securityOrigin();
  if (!OriginCanAccessPasswordManager(origin))
    return;

  // Checks whether the webpage is a redirect page or an empty page.
  if (IsWebpageEmpty(frame)){
    LOG(INFO)<<"FP:PasswordAutofillAgent::SendPasswordForms : Web page Empty return";
    return;
  	}

  blink::WebVector<blink::WebFormElement> forms;
  frame->document().forms(forms);
  LOG(INFO)<<"FP:PasswordAutofillAgent::SendPasswordForms forms SIZE = "<<forms.size();
  std::vector<PasswordForm> password_forms;
  for (size_t i = 0; i < forms.size(); ++i) {
    const blink::WebFormElement& form = forms[i];

    // If requested, ignore non-rendered forms, e.g. those styled with
    // display:none.
    if (only_visible && !IsWebNodeVisible(form))
      continue;

    #if defined(S_FP_AVOID_SCREEN_AFTER_AUTOLOGIN)
    // Some time After Submitting the same form is sent over 
    // which unnecessarily invokes Autofill again.
    // So, don't send those Form which was already submitted.
    if(!only_visible && form.wasWebLoginSubmitted()){
       LOG(INFO)<<"FP:PasswordAutofillAgent::SendPasswordForms form wasUserSubmitted ";
	//If the previous page, a form was already submitted in the frame, 
	// and after submit again the frame is sent with multiple forms matching the forms saved in DB
	// Since only for one it will skip AutoFill but for others it will go ahead.
	// So, It's better to return from here only.
	// This is temporary change, If Any Regression observed, please change return -> continue.
	return;
	//continue;
    }
    #endif
    
    scoped_ptr<PasswordForm> password_form(CreatePasswordForm(form));
    if (password_form.get()){
      #if defined(S_FP_DEFAULT_USERNAME_FIX)	 	
      // This is strange to see the same form is being sent because of DynamicFormsSeen which user has just filled.
      // It can cause problem if we already have a credential saved, because then it will autofill.
      // So, Its better to avoid such form to send to browser process while parsing.
      // To Do: Need to check why the WebCore::Timer<WebCore::Document>::fired() gets fired 
      // when (https://www.web-odakyu.com/mb/index.jsp) login form's submit button gets clicked.
      if(provisionally_saved_forms_[frame].get()){
          if(!only_visible && provisionally_saved_forms_[frame]->action == password_form.get()->action &&
		  	provisionally_saved_forms_[frame]->password_value == password_form.get()->password_value)
	       continue;
      }
      #endif	
      #if defined(S_FP_HIDDEN_FORM_FIX)
      password_form.get()->is_hidden = form.hasRenderer();
      #endif
      password_forms.push_back(*password_form);
    }
  }

  if (password_forms.empty() && !only_visible) {
    // We need to send the PasswordFormsRendered message regardless of whether
    // there are any forms visible, as this is also the code path that triggers
    // showing the infobar.
    LOG(INFO)<<"FP:PasswordAutofillAgent::SendPasswordForms password_forms EMPTY return ";
    return;
  }

  if (only_visible) {
    Send(new AutofillHostMsg_PasswordFormsRendered(routing_id(),
                                                   password_forms));
  } else {
    Send(new AutofillHostMsg_PasswordFormsParsed(routing_id(), password_forms));
  }
}

bool PasswordAutofillAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordAutofillAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_FillPasswordForm, OnFillPasswordForm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordAutofillAgent::DidStartLoading() {
  if (usernames_usage_ != NOTHING_TO_AUTOFILL) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.OtherPossibleUsernamesUsage",
                              usernames_usage_, OTHER_POSSIBLE_USERNAMES_MAX);
    usernames_usage_ = NOTHING_TO_AUTOFILL;
  }
}

void PasswordAutofillAgent::DidFinishDocumentLoad(blink::WebFrame* frame) {
  // The |frame| contents have been parsed, but not yet rendered.  Let the
  // PasswordManager know that forms are loaded, even though we can't yet tell
  // whether they're visible.
  SendPasswordForms(frame, false);
}

void PasswordAutofillAgent::DidFinishLoad(blink::WebFrame* frame) {
  // The |frame| contents have been rendered.  Let the PasswordManager know
  // which of the loaded frames are actually visible to the user.  This also
  // triggers the "Save password?" infobar if the user just submitted a password
  // form.
  SendPasswordForms(frame, true);
}

void PasswordAutofillAgent::FrameDetached(blink::WebFrame* frame) {
  FrameClosing(frame);
}

void PasswordAutofillAgent::FrameWillClose(blink::WebFrame* frame) {
  FrameClosing(frame);
}

#if defined(S_FP_NEW_TAB_FIX)
void PasswordAutofillAgent::CheckforRPPBeforeTabClose() {
   Send(new AutofillHostMsg_RPPCheckBeforeTabClose(routing_id()));
}
#endif

void PasswordAutofillAgent::WillSendSubmitEvent(
    blink::WebFrame* frame,
    const blink::WebFormElement& form) {
  // Some login forms have onSubmit handlers that put a hash of the password
  // into a hidden field and then clear the password (http://crbug.com/28910).
  // This method gets called before any of those handlers run, so save away
  // a copy of the password in case it gets lost.
  //LOG(INFO)<<"FP: PasswordAutofillAgent::WillSendSubmitEvent";
  scoped_ptr<PasswordForm> password_form(CreatePasswordForm(form));
#if defined(S_FP_COPY_OVER_PASSWORD_FIX)
  form_ID_attr_[frame] = blink::WebString();
#endif
  if (password_form){
    provisionally_saved_forms_[frame].reset(password_form.release());
#if defined(S_FP_COPY_OVER_PASSWORD_FIX)
    form_ID_attr_[frame] = GetFormIdentifier(form);
#endif
#if defined(S_FP_MSSITES_AUTOFILL_FIX)
    will_send_submit_ = true;
#endif
  }
}

void PasswordAutofillAgent::WillSubmitForm(blink::WebFrame* frame,
                                           const blink::WebFormElement& form) {

  //LOG(INFO)<<"FP: PasswordAutofillAgent::WillSubmitForm";
  scoped_ptr<PasswordForm> submitted_form = CreatePasswordForm(form);
#if defined(S_FP_MSSITES_AUTOFILL_FIX)
  will_send_submit_ = false;
#endif
  // If there is a provisionally saved password, copy over the previous
  // password value so we get the user's typed password, not the value that
  // may have been transformed for submit.
  // TODO(gcasto): Do we need to have this action equality check? Is it trying
  // to prevent accidentally copying over passwords from a different form?
  if (submitted_form) {
#if defined(S_FP_COPY_OVER_PASSWORD_FIX)
    if (provisionally_saved_forms_[frame].get() &&
        ((submitted_form->origin.is_valid() && submitted_form->password_value.empty()
        && (!form_ID_attr_[frame].isEmpty() && GetFormIdentifier(form)==form_ID_attr_[frame]))
         || (submitted_form->action == provisionally_saved_forms_[frame]->action)))
#else
    if (provisionally_saved_forms_[frame].get() &&
        submitted_form->action == provisionally_saved_forms_[frame]->action)
#endif
    {
      submitted_form->password_value =
          provisionally_saved_forms_[frame]->password_value;
#if defined(S_FP_COPY_OVER_PASSWORD_FIX)
      form_ID_attr_[frame] = blink::WebString();
#endif
    }
#if defined(S_FP_COPY_OVER_USERNAME_FIX)
    if (provisionally_saved_forms_[frame].get() && 
        submitted_form->action == provisionally_saved_forms_[frame]->action 
        && submitted_form->username_value.empty()){
      submitted_form->username_value =
          provisionally_saved_forms_[frame]->username_value;
    }
#endif
    // Some observers depend on sending this information now instead of when
    // the frame starts loading. If there are redirects that cause a new
    // RenderView to be instantiated (such as redirects to the WebStore)
    // we will never get to finish the load.
    //LOG(INFO)<<"FP: PasswordAutofillAgent::WillSendSubmitEvent sending IPC";

    Send(new AutofillHostMsg_PasswordFormSubmitted(routing_id(),
                                                   *submitted_form));
    // Remove reference since we have already submitted this form.
    provisionally_saved_forms_.erase(frame);
  }
}

blink::WebFrame* PasswordAutofillAgent::CurrentOrChildFrameWithSavedForms(
    const blink::WebFrame* current_frame) {
#if defined(S_PLM_P140903_00442)
	if(current_frame == NULL)
		return NULL;
#endif
  for (FrameToPasswordFormMap::const_iterator it =
           provisionally_saved_forms_.begin();
       it != provisionally_saved_forms_.end();
       ++it) {
    blink::WebFrame* form_frame = it->first;
	#if defined(S_PLM_P141126_02106)
	if(form_frame == NULL)
		return NULL;
   #endif
    // The check that the returned frame is related to |current_frame| is mainly
    // for double-checking. There should not be any unrelated frames in
    // |provisionally_saved_forms_|, because the map is cleared after
    // navigation. If there are reasons to remove this check in the future and
    // keep just the first frame found, it might be a good idea to add a UMA
    // statistic or a similar check on how many frames are here to choose from.
    if (current_frame == form_frame ||
        current_frame->findChildByName(form_frame->uniqueName())) {
      return form_frame;
    }
  }
  return NULL;
}

void PasswordAutofillAgent::DidStartProvisionalLoad(blink::WebFrame* frame) {

  #if defined(S_FP_MSSITES_AUTOFILL_FIX)
  will_send_submit_ = false;
  #endif

  if (!frame->parent()) {
    // If the navigation is not triggered by a user gesture, e.g. by some ajax
    // callback, then inherit the submitted password form from the previous
    // state. This fixes the no password save issue for ajax login, tracked in
    // [http://crbug/43219]. Note that this still fails for sites that use
    // synchonous XHR as isProcessingUserGesture() will return true.
    blink::WebFrame* form_frame = CurrentOrChildFrameWithSavedForms(frame);

    #if defined(S_OPEN_SOURCE_266793003_PATCH)
    // Bug fix for crbug.com/368690. isProcessingUserGesture() is false when
    // the user is performing actions outside the page (e.g. typed url,
    // history navigation). We don't want to trigger saving in these cases.
    content::DocumentState* document_state =
                     content::DocumentState::FromDataSource(frame->provisionalDataSource());
    content::NavigationState* navigation_state = document_state->navigation_state();

    //Added additional transition check as there seems to be some problem on WebApps with PageTransitionIsWebTriggerable
    bool transition_is_fwdBack = (navigation_state->transition_type() & content::PAGE_TRANSITION_FORWARD_BACK);
    if (content::PageTransitionIsWebTriggerable(navigation_state->transition_type()) && !transition_is_fwdBack
	     &&  !blink::WebUserGestureIndicator::isProcessingUserGesture())
    #else
    if(!blink::WebUserGestureIndicator::isProcessingUserGesture())
    #endif
    {
       LOG(INFO)<<"FP: PasswordAutofillAgent::DidStartProvisionalLoad";
      // If onsubmit has been called, try and save that form.
      if (provisionally_saved_forms_[form_frame].get()) {
        Send(new AutofillHostMsg_PasswordFormSubmitted(
            routing_id(),
            *provisionally_saved_forms_[form_frame]));
        provisionally_saved_forms_.erase(form_frame);
      } else {
        // Loop through the forms on the page looking for one that has been
        // filled out. If one exists, try and save the credentials.
        blink::WebVector<blink::WebFormElement> forms;
        frame->document().forms(forms);

        for (size_t i = 0; i < forms.size(); ++i) {
          blink::WebFormElement form_element= forms[i];
          scoped_ptr<PasswordForm> password_form(
              CreatePasswordForm(form_element));
          if (password_form.get() &&
              !password_form->username_value.empty() &&
              !password_form->password_value.empty() &&
              !PasswordValueIsDefault(*password_form, form_element)) {
            Send(new AutofillHostMsg_PasswordFormSubmitted(
                routing_id(), *password_form));
          }
        }
      }
    }
    // Clear the whole map during main frame navigation.
    provisionally_saved_forms_.clear();

    // We are navigating, se we need to wait for a new user gesture before
    // filling in passwords.
    user_gesture_occurred_ = false;
    gesture_handler_->clearElements();
  }
}

void PasswordAutofillAgent::OnFillPasswordForm(
    const PasswordFormFillData& form_data) {
  if (usernames_usage_ == NOTHING_TO_AUTOFILL) {
    if (form_data.other_possible_usernames.size())
      usernames_usage_ = OTHER_POSSIBLE_USERNAMES_PRESENT;
    else if (usernames_usage_ == NOTHING_TO_AUTOFILL)
      usernames_usage_ = OTHER_POSSIBLE_USERNAMES_ABSENT;
  }
  LOG(INFO)<<"FP: PasswordAutofillAgent::OnFillPasswordForm";
  FormElementsList forms;
  // We own the FormElements* in forms.
  FindFormElements(render_view()->GetWebView(), form_data.basic_data, &forms);
  FormElementsList::iterator iter;
  for (iter = forms.begin(); iter != forms.end(); ++iter) {
    scoped_ptr<FormElements> form_elements(*iter);

    // Attach autocomplete listener to enable selecting alternate logins.
    // First, get pointers to username element.
    blink::WebInputElement username_element =
        form_elements->input_elements[form_data.basic_data.fields[0].name];

    // Get pointer to password element. (We currently only support single
    // password forms).
    blink::WebInputElement password_element =
        form_elements->input_elements[form_data.basic_data.fields[1].name];

    // If wait_for_username is true, we don't want to initially fill the form
    // until the user types in a valid username.
    //Samsung: As we have already taken care of action url changes and incognito mode
    // We are here just avoiding wait_for_username fully now.
    // Need to revisit this if we want to alter this only for FP registered accounts.
    #if defined(S_FP_WAIT_FOR_USERNAME_FIX)
    #if defined(S_FP_HIDDEN_FORM_FIX)
    if(!form_data.form_is_hidden && !form_data.manual_autofill)
    #endif
    #else
    if (!form_data.wait_for_username
        #if defined(S_FP_HIDDEN_FORM_FIX) 
        && !form_data.form_is_hidden
        #endif
        )
    #endif
      FillFormOnPasswordRecieved(form_data, username_element, password_element);
		
    // We might have already filled this form if there are two <form> elements
    // with identical markup.
    if (login_to_password_info_.find(username_element) !=
        login_to_password_info_.end())
      continue;

    PasswordInfo password_info;
    password_info.fill_data = form_data;
    password_info.password_field = password_element;
    login_to_password_info_[username_element] = password_info;

    FormData form;
    FormFieldData field;
    FindFormAndFieldForInputElement(
        username_element, &form, &field, REQUIRE_NONE);
    Send(new AutofillHostMsg_AddPasswordFormMapping(
        routing_id(),
        field,
        form_data));

    #if defined(S_FP_HIDDEN_FORM_FIX)
    // Some time It happens while sending the form there is no renderer but when we come here Renderer is created.
    // In Such cases, Do Autofill.
    if(form_data.form_is_hidden)
   	CheckFormVisibilityAndAutofill();
    #endif
  }
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, private:

void PasswordAutofillAgent::GetSuggestions(
    const PasswordFormFillData& fill_data,
    const base::string16& input,
    std::vector<base::string16>* suggestions,
    std::vector<base::string16>* realms) {
  if (StartsWith(fill_data.basic_data.fields[0].value, input, false)) {
    suggestions->push_back(fill_data.basic_data.fields[0].value);
    realms->push_back(base::UTF8ToUTF16(fill_data.preferred_realm));
  }

  for (PasswordFormFillData::LoginCollection::const_iterator iter =
           fill_data.additional_logins.begin();
       iter != fill_data.additional_logins.end(); ++iter) {
    if (StartsWith(iter->first, input, false)) {
      suggestions->push_back(iter->first);
      realms->push_back(base::UTF8ToUTF16(iter->second.realm));
    }
  }

  for (PasswordFormFillData::UsernamesCollection::const_iterator iter =
           fill_data.other_possible_usernames.begin();
       iter != fill_data.other_possible_usernames.end(); ++iter) {
    for (size_t i = 0; i < iter->second.size(); ++i) {
      if (StartsWith(iter->second[i], input, false)) {
        usernames_usage_ = OTHER_POSSIBLE_USERNAME_SHOWN;
        suggestions->push_back(iter->second[i]);
        realms->push_back(base::UTF8ToUTF16(iter->first.realm));
      }
    }
  }
}

bool PasswordAutofillAgent::ShowSuggestionPopup(
    const PasswordFormFillData& fill_data,
    const blink::WebInputElement& user_input) {
  blink::WebFrame* frame = user_input.document().frame();
  if (!frame)
    return false;

  blink::WebView* webview = frame->view();
  if (!webview)
    return false;

  std::vector<base::string16> suggestions;
  std::vector<base::string16> realms;
  GetSuggestions(fill_data, user_input.value(), &suggestions, &realms);
  DCHECK_EQ(suggestions.size(), realms.size());

  FormData form;
  FormFieldData field;
  FindFormAndFieldForInputElement(
      user_input, &form, &field, REQUIRE_NONE);

  blink::WebInputElement selected_element = user_input;
  gfx::Rect bounding_box(selected_element.boundsInViewportSpace());

  float scale = web_view_->pageScaleFactor();
  gfx::RectF bounding_box_scaled(bounding_box.x() * scale,
                                 bounding_box.y() * scale,
                                 bounding_box.width() * scale,
                                 bounding_box.height() * scale);
  Send(new AutofillHostMsg_ShowPasswordSuggestions(routing_id(),
                                                   field,
                                                   bounding_box_scaled,
                                                   suggestions,
                                                   realms));
  return !suggestions.empty();
}

void PasswordAutofillAgent::FillFormOnPasswordRecieved(
    const PasswordFormFillData& fill_data,
    blink::WebInputElement username_element,
    blink::WebInputElement password_element) {
   //LOG(INFO)<<"FP: PasswordAutofillAgent::FillFormOnPasswordRecieved";
  // Do not fill if the password field is in an iframe.
  DCHECK(password_element.document().frame());
  if (password_element.document().frame()->parent()){  	 
     #if !defined(S_FP_IFRAME_AUTOFILL_FIX)
	    LOG(INFO)<<"FP: PasswordAutofillAgent::FillFormOnPasswordRecieved : IFRAME return";
        return;
     #endif
  }
  
  #if defined(S_AUTOCOMPLETE_IGNORE)
  if (!web_view_->settings()->autocompleteIgnore() &&
      !username_element.form().autoComplete()){
      LOG(INFO)<<"FP: PasswordAutofillAgent::FillFormOnPasswordRecieved : AUTOCOMPLETE_IGNORE return";
	  return;
  	}
  #else
  if (!ShouldIgnoreAutocompleteOffForPasswordFields() &&
      !username_element.form().autoComplete()){
      LOG(INFO)<<"FP: PasswordAutofillAgent::FillFormOnPasswordRecieved : AUTOCOMPLETE_IGNORE return";
      return;
  	}
  #endif

  // If we can't modify the password, don't try to set the username
  if (!IsElementAutocompletable(password_element)){
    LOG(INFO)<<"FP: PasswordAutofillAgent::FillFormOnPasswordRecieved : !IsElementAutocompletable return";
    return;
  	}

  // Try to set the username to the preferred name, but only if the field
  // can be set and isn't prefilled.
  #if defined(S_FP_DEFAULT_USERNAME_FIX)
     if (IsElementAutocompletable(username_element)){
	if(fill_data.selectedUser.empty())
        	username_element.setValue(fill_data.basic_data.fields[0].value, true);
	else 
		username_element.setValue(fill_data.selectedUser, true);
	}
  #else
     if (IsElementAutocompletable(username_element)
  	    && username_element.value().isEmpty()){

        if(fill_data.selectedUser.empty())
        	username_element.setValue(fill_data.basic_data.fields[0].value, true);
        else{
	        // TODO(tkent): Check maxlength and pattern.
			username_element.setValue(fill_data.selectedUser, true);
        }
     }
  #endif

  // Fill if we have an exact match for the username. Note that this sets
  // username to autofilled.
  FillUserNameAndPassword(&username_element, &password_element, fill_data,
                          true /* exact_username_match */,
                          false /* set_selection */);
}

bool PasswordAutofillAgent::FillUserNameAndPassword(
    blink::WebInputElement* username_element,
    blink::WebInputElement* password_element,
    const PasswordFormFillData& fill_data,
    bool exact_username_match,
    bool set_selection) {
  LOG(INFO)<<"FP: PasswordAutofillAgent::FillUserNameAndPassword";
  base::string16 current_username = username_element->value();
  // username and password will contain the match found if any.
  base::string16 username;
  base::string16 password;

  // Look for any suitable matches to current field text.
  if (DoUsernamesMatch(fill_data.basic_data.fields[0].value, current_username,
                       exact_username_match)) {
    username = fill_data.basic_data.fields[0].value;
    password = fill_data.basic_data.fields[1].value;
  } else {
    // Scan additional logins for a match.
    PasswordFormFillData::LoginCollection::const_iterator iter;
    for (iter = fill_data.additional_logins.begin();
         iter != fill_data.additional_logins.end(); ++iter) {
      if (DoUsernamesMatch(iter->first, current_username,
                           exact_username_match)) {
        username = iter->first;
        password = iter->second.password;
        break;
      }
    }

    // Check possible usernames.
    if (username.empty() && password.empty()) {
      for (PasswordFormFillData::UsernamesCollection::const_iterator iter =
               fill_data.other_possible_usernames.begin();
           iter != fill_data.other_possible_usernames.end(); ++iter) {
        for (size_t i = 0; i < iter->second.size(); ++i) {
          if (DoUsernamesMatch(iter->second[i], current_username,
                               exact_username_match)) {
            usernames_usage_ = OTHER_POSSIBLE_USERNAME_SELECTED;
            username = iter->second[i];
            password = iter->first.password;
            break;
          }
        }
        if (!username.empty() && !password.empty())
          break;
      }
    }
  }
  if (password.empty()){
    LOG(INFO)<<"FP: PasswordAutofillAgent::FillUserNameAndPassword return false for no match found";
    return false;  // No match was found.
    }

  // TODO(tkent): Check maxlength and pattern for both username and password
  // fields.

  // Don't fill username if password can't be set.
  if (!IsElementAutocompletable(*password_element)) {
    return false;
  }

  // Input matches the username, fill in required values.
  if (IsElementAutocompletable(*username_element)) {
    username_element->setValue(username, true);
    SetElementAutofilled(username_element, true);

    if (set_selection) {
      username_element->setSelectionRange(current_username.length(),
                                          username.length());
    }
  } else if (current_username !=username
  #if defined(S_FP_INVALID_EMAIL_USERNAME_FIX)
   && !DoUsernamesMatch(username, current_username,
                               exact_username_match)
    #endif
    ) {
    // If the username can't be filled and it doesn't match a saved password
    // as is, don't autofill a password.
    return false;
  }

  #if defined(S_FP_SUPPORT) && defined(S_FP_AUTOLOGIN_SUPPORT)
  // Set Focus On the Password Field to generate Enter Key on it.
  if(fill_data.authentication_required)
        web_view_->setFocusOnPasswordField(*password_element);
  #endif

  // If a user gesture has not occurred, we setup a handler to listen for the
  // next user gesture, at which point we then fill in the password. This is to
  // make sure that we do not fill in the DOM with a password until we believe
  // the user is intentionally interacting with the page.
  //Samsung: As User authenticate himself using his Fingerprint,
  // consider it as user interaction with the page and update the value in DOM
  if (!user_gesture_occurred_ 
      #if defined(S_FP_SUPPORT) && defined(S_FP_AUTOLOGIN_SUPPORT)
      && !fill_data.authentication_required
      #endif
      ) {
    gesture_handler_->addElement(*password_element);
    password_element->setSuggestedValue(password);
  } else {
    password_element->setValue(password, true);
  }
  // Note: Don't call SetElementAutofilled() here, as that dispatches an
  // onChange event in JavaScript, which is not appropriate for the password
  // element if a user gesture has not yet occured.
  password_element->setAutofilled(true);

  #if defined(S_FP_AVOID_PASSWORD_SELECTION)
  // Avoid password selection while AutoLogin.
  // Do it only in the case of WebLogin
  if(fill_data.authentication_required)
      password_element->setSelectionRange(password.length(), password.length());
  #endif
  
  // Generate Enter Event after Filling the form.
  // AutoLogin is only supported when additional authentication is done
  #if defined(S_FP_SUPPORT) && defined(S_FP_AUTOLOGIN_SUPPORT)
  LOG(INFO)<<"FP: PasswordAutofillAgent::FillUserNameAndPassword : UserName and PWD autofilled.Initiate Autologin fill_data.authentication_required ="<<fill_data.authentication_required;
  if(fill_data.authentication_required){
      #if defined(S_FP_DELAY_FORMSUBMIT)
      //Delay is added to support some sites whose submission 
      // becomes active some time after the password field is filled.
      if (submit_button_input_timer_.IsRunning()) {
             submit_button_input_timer_.Reset();
      }else{
             submit_button_input_timer_.Start(FROM_HERE, 
			 	base::TimeDelta::FromMilliseconds(500),
			 	base::Bind(&PasswordAutofillAgent::SubmitOnTimer,
			 	base::Unretained(this), *password_element/*, password*/));
      }
      #else
      web_view_->generateEnterEvent(*password_element/*, password*/);
      #endif
  }
  #endif
  
  return true;
}

#if defined(S_FP_DELAY_FORMSUBMIT)
void PasswordAutofillAgent::SubmitOnTimer(const blink::WebInputElement &pelement/*, base::string16 pwd*/)
{
   web_view_->generateEnterEvent(pelement/*, pwd*/);
}
#endif

void PasswordAutofillAgent::PerformInlineAutocomplete(
    const blink::WebInputElement& username_input,
    const blink::WebInputElement& password_input,
    const PasswordFormFillData& fill_data) {
  DCHECK(!fill_data.wait_for_username);

  // We need non-const versions of the username and password inputs.
  blink::WebInputElement username = username_input;
  blink::WebInputElement password = password_input;

  // Don't inline autocomplete if the caret is not at the end.
  // TODO(jcivelli): is there a better way to test the caret location?
  if (username.selectionStart() != username.selectionEnd() ||
      username.selectionEnd() != static_cast<int>(username.value().length())) {
    return;
  }

  // Show the popup with the list of available usernames.
  ShowSuggestionPopup(fill_data, username);


#if !defined(OS_ANDROID)
  // Fill the user and password field with the most relevant match. Android
  // only fills in the fields after the user clicks on the suggestion popup.
  FillUserNameAndPassword(&username, &password, fill_data,
                          false /* exact_username_match */,
                          true /* set_selection */);
#endif
}

void PasswordAutofillAgent::FrameClosing(const blink::WebFrame* frame) {
  for (LoginToPasswordInfoMap::iterator iter = login_to_password_info_.begin();
       iter != login_to_password_info_.end();) {
    if (iter->first.document().frame() == frame)
      login_to_password_info_.erase(iter++);
    else
      ++iter;
  }
  for (FrameToPasswordFormMap::iterator iter =
           provisionally_saved_forms_.begin();
       iter != provisionally_saved_forms_.end();) {
    if (iter->first == frame)
      provisionally_saved_forms_.erase(iter++);
    else
      ++iter;
  }
}

bool PasswordAutofillAgent::FindLoginInfo(const blink::WebNode& node,
                                          blink::WebInputElement* found_input,
                                          PasswordInfo* found_password) {
  if (!node.isElementNode())
    return false;

  blink::WebElement element = node.toConst<blink::WebElement>();
  if (!element.hasTagName("input"))
    return false;

  blink::WebInputElement input = element.to<blink::WebInputElement>();
  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(input);
  if (iter == login_to_password_info_.end())
    return false;

  *found_input = input;
  *found_password = iter->second;
  return true;
}

void PasswordAutofillAgent::AutofillWebUserGestureHandler::onGesture() {
  agent_->set_user_gesture_occurred(true);

  std::vector<blink::WebInputElement>::iterator iter;
  for (iter = elements_.begin(); iter != elements_.end(); ++iter) {
    if (!iter->isNull() && !iter->suggestedValue().isNull())
      iter->setValue(iter->suggestedValue(), true);
  }

  elements_.clear();
}

PasswordAutofillAgent::AutofillWebUserGestureHandler::
    AutofillWebUserGestureHandler(PasswordAutofillAgent* agent)
    : agent_(agent) {}

PasswordAutofillAgent::AutofillWebUserGestureHandler::
    ~AutofillWebUserGestureHandler() {}

}  // namespace autofill
