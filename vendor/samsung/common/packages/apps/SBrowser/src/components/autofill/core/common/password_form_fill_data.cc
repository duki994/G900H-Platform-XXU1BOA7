// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_fill_data.h"

#include "base/logging.h"
#if defined(S_FP_MIXED_CASE_USERNAME_FIX)
#include "base/i18n/case_conversion.h"
#endif
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

UsernamesCollectionKey::UsernamesCollectionKey() {}

UsernamesCollectionKey::~UsernamesCollectionKey() {}

bool UsernamesCollectionKey::operator<(
    const UsernamesCollectionKey& other) const {
  if (username != other.username)
    return username < other.username;
  if (password != other.password)
    return password < other.password;
  return realm < other.realm;
}

PasswordFormFillData::PasswordFormFillData() : wait_for_username(false) {
}

PasswordFormFillData::~PasswordFormFillData() {
}

void InitPasswordFormFillData(
    const PasswordForm& form_on_page,
    const PasswordFormMap& matches,
    const PasswordForm* const preferred_match,
    bool wait_for_username_before_autofill,
    bool manual_autofill,
    bool enable_other_possible_usernames,
    PasswordFormFillData* result) {
  // Note that many of the |FormFieldData| members are not initialized for
  // |username_field| and |password_field| because they are currently not used
  // by the password autocomplete code.
  result->selectedUser = base::UTF8ToUTF16("");

  // When Manual Autofill is set We need to consider only Non FP Account.
  const PasswordForm *preferred_match_temp = preferred_match;

  if(manual_autofill && preferred_match->use_additional_authentication){
  	 PasswordFormMap::const_iterator iter;
        for (iter = matches.begin(); iter != matches.end(); iter++){
	      if(!iter->second->use_additional_authentication){
		  	preferred_match_temp = iter->second;
			break;
	      	}
        }
  }

  FormFieldData username_field;
  	
  #if defined(S_FP_EMPTY_USERNAME_FIX)
  username_field.name = preferred_match_temp->username_element;
  #else
  username_field.name = form_on_page.username_element;
  #endif

  #if defined(S_FP_MIXED_CASE_USERNAME_FIX)
  if(form_on_page.username_element_readonly)
      username_field.value = base::i18n::ToLower(preferred_match_temp->username_value);
  else
  #endif
  username_field.value = preferred_match_temp->username_value;
	      
  FormFieldData password_field;
  password_field.name = form_on_page.password_element;
  password_field.value = preferred_match_temp->password_value;
  password_field.form_control_type = "password";

  result->manual_autofill = manual_autofill;
  result->username_element_readonly = form_on_page.username_element_readonly;
  // Fill basic form data.
  result->basic_data.origin = form_on_page.origin;
  result->basic_data.action = form_on_page.action;
  result->basic_data.fields.push_back(username_field);
  if(preferred_match_temp->use_additional_authentication)
    result->username_list.push_back(username_field.value);

  result->basic_data.fields.push_back(password_field);
  result->wait_for_username = wait_for_username_before_autofill;

  result->preferred_realm = preferred_match_temp->original_signon_realm;

  #if defined(S_FP_HIDDEN_FORM_FIX)
  result->form_is_hidden = form_on_page.is_hidden;
  #endif

  // Copy additional username/value pairs.
  PasswordFormMap::const_iterator iter;
  for (iter = matches.begin(); iter != matches.end(); iter++) {
    if (iter->second != preferred_match_temp) {
      PasswordAndRealm value;
      if(manual_autofill){
          if(iter->second->use_additional_authentication == false){
               value.password = iter->second->password_value;
               value.realm = iter->second->original_signon_realm;
               result->additional_logins[iter->first] = value;
          }
      }else{
          value.password = iter->second->password_value;
          value.realm = iter->second->original_signon_realm;
          result->additional_logins[iter->first] = value;
          if(iter->second->use_additional_authentication == true)
             result->username_list.push_back(iter->second->username_value);
      }
    }
    if (enable_other_possible_usernames &&
        !iter->second->other_possible_usernames.empty()) {
      // Note that there may be overlap between other_possible_usernames and
      // other saved usernames or with other other_possible_usernames. For now
      // we will ignore this overlap as it should be a rare occurence. We may
      // want to revisit this in the future.
      UsernamesCollectionKey key;
      key.username = iter->first;
      key.password = iter->second->password_value;
      key.realm = iter->second->original_signon_realm;
      result->other_possible_usernames[key] =
          iter->second->other_possible_usernames;
    }
  }
}

#if defined(S_FP_HIDDEN_FORM_FIX)
void InitHiddenFormFillData(const PasswordFormFillData &h_fill_data, PasswordFormFillData* result){

   result->selectedUser = h_fill_data.selectedUser;
   result->basic_data = h_fill_data.basic_data;
   result->preferred_realm = h_fill_data.preferred_realm;
   result->additional_logins = h_fill_data.additional_logins;
   result->other_possible_usernames = h_fill_data.other_possible_usernames;
   result->wait_for_username = h_fill_data.wait_for_username;
   result->username_element_readonly = h_fill_data.username_element_readonly;
   result->authentication_required = h_fill_data.authentication_required;
   result->form_is_hidden = false;
   result->manual_autofill = h_fill_data.manual_autofill;
   result->username_list = h_fill_data.username_list;
}
#endif

}  // namespace autofill
