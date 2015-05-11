/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is Mozilla Password Manager.
*
* The Initial Developer of the Original Code is
* Brian Ryner.
* Portions created by the Initial Developer are Copyright (C) 2003
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*  Brian Ryner <bryner@brianryner.com>
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

// Helper to WebPasswordFormData to do the locating of username/password
// fields.
// This method based on Firefox2 code in
//   toolkit/components/passwordmgr/base/nsPasswordManager.cpp

#include "config.h"
#include "WebPasswordFormUtils.h"

#include "HTMLNames.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "platform/weborigin/KURL.h"

using namespace WebCore;

namespace blink {

// Maximum number of password fields we will observe before throwing our
// hands in the air and giving up with a given form.
static const size_t maxPasswords = 3;

/*Note: S_WRONG_PASSWORD_FACEBOOKPOPUPFIX
The fix under this flag is to avoid Remember popup dialog in facebook on entering wrong credentials.
IN facebook mobile page, on entering wrong passwrod and submit -> content changes and password field input type changes to "text" from "password".Due to this,as there is no password field, createpasswordform call fails.
To avoid this,this workaround is made under the assumption of having a form with one input "text", one input "password" and one input "submit" button under form element.
And the first field will be always username/text field.so, if the next element is also "text" then the content is similar to facebook and still to go ahead with form creation.
*/
void findPasswordFormFields(HTMLFormElement* form, PasswordFormFields* fields)
{
    ASSERT(form);
    ASSERT(fields);

    HTMLInputElement* latestInputElement = 0;

    #if defined(S_FP_EMPTY_USERNAME_FIX)
    HTMLInputElement* latestFilledInputElement = 0;
    #endif
	
    #if defined(S_WRONG_PASSWORD_FACEBOOKPOPUPFIX)
    bool username_already_found = false;
    #endif
    const Vector<FormAssociatedElement*>& formElements = form->associatedElements();
    for (size_t i = 0; i < formElements.size(); i++) {
        if (!formElements[i]->isFormControlElement())
            continue;
        HTMLFormControlElement* control = toHTMLFormControlElement(formElements[i]);
        if (control->isActivatedSubmit())
            fields->submit = control;

        if (!control->hasTagName(HTMLNames::inputTag))
            continue;

        HTMLInputElement* inputElement = toHTMLInputElement(control);
        if (inputElement->isDisabledFormControl())
            continue;

        if ((fields->passwords.size() < maxPasswords)
            && (inputElement->isPasswordField()
            #if defined(S_WRONG_PASSWORD_FACEBOOKPOPUPFIX)
            || (form->document().url().host().contains("m.facebook") && username_already_found == true && inputElement->isTextField())
            #endif
	     )) {
            // We assume that the username is the input element before the
            // first password element.
            if (fields->passwords.isEmpty() && latestInputElement) {
		  #if defined(S_FP_EMPTY_USERNAME_FIX)
		  //If Password is filled It means, We are here after Submitting the Form.
		  // So, Its better to consider last filled text field as username element.
		  if(!inputElement->value().isEmpty() && latestFilledInputElement)
		       fields->userName = latestFilledInputElement;
		  else
		  #endif
		  	fields->userName = latestInputElement;
                // Remove the selected username from alternateUserNames.
                if (!fields->alternateUserNames.isEmpty() && !latestInputElement->value().isEmpty())
                    fields->alternateUserNames.removeLast();
            }
            fields->passwords.append(inputElement);
        }

        // Various input types such as text, url, email can be a username field.
        // Samsung - It doesn't make any sense to consider any text field as username field
        // As In our case the scheme of the Form is always SCHEME_HTML and it should always have username and password element set.
        if (inputElement->isTextField() && !inputElement->isPasswordField() && !inputElement->nameForAutofill().isEmpty()) {
            latestInputElement = inputElement;
            // We ignore elements that have no value. Unlike userName, alternateUserNames
            // is used only for autofill, not for form identification, and blank autofill
            // entries are not useful.
            if (!inputElement->value().isEmpty()){
                #if defined(S_FP_EMPTY_USERNAME_FIX)
                latestFilledInputElement = inputElement;
                #endif
		  
                fields->alternateUserNames.append(inputElement->value());
		    #if defined(S_WRONG_PASSWORD_FACEBOOKPOPUPFIX)
		    if(form->document().url().host().contains("m.facebook"))
                      username_already_found = true;
                #endif
            }
        }
    }
}

} // namespace blink
