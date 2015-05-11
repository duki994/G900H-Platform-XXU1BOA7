// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <algorithm>
#include <limits>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/common/password_form.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "base/strings/utf_string_conversions.h"

#if defined(SBROWSER_PASSWORD_ENCRYPTION)
#include "components/webdata/encryptor/encryptor.h"

#ifdef  __cplusplus
extern "C" {
#include "wbs.h"
long WBS_Enc (unsigned char *ct, unsigned char *pt, long size, unsigned char *iv);
long WBS_Dec (unsigned char *pt, unsigned char *ct, long size, unsigned char *iv);
}
#endif
#endif //end SBROWSER_PASSWORD_ENCRYPTION
using autofill::PasswordForm;
using base::ASCIIToUTF16;
static const int kCurrentVersionNumber = 5;
static const int kCompatibleVersionNumber = 1;

namespace {

// Convenience enum for interacting with SQL queries that use all the columns.
enum LoginTableColumns {
  COLUMN_ORIGIN_URL = 0,
  COLUMN_ACTION_URL,
  COLUMN_USERNAME_ELEMENT,
  COLUMN_USERNAME_VALUE,
  COLUMN_PASSWORD_ELEMENT,
  COLUMN_PASSWORD_VALUE,
  COLUMN_SUBMIT_ELEMENT,
  COLUMN_SIGNON_REALM,
  COLUMN_SSL_VALID,
  COLUMN_PREFERRED,
  COLUMN_DATE_CREATED,
  COLUMN_BLACKLISTED_BY_USER,
  COLUMN_SCHEME,
  COLUMN_PASSWORD_TYPE,
  COLUMN_POSSIBLE_USERNAMES,
  COLUMN_TIMES_USED,
  COLUMN_FORM_DATA,
#if defined(SBROWSER_PASSWORD_ENCRYPTION)
  COLUMN_USE_ADDITIONAL_AUTH,
  COLUMN_ENCRYPTED_GENERATED_KEY,
  COLUMN_STORED_PLATFORM_IV,
  COLUMN_CT_SIZE
#else
  COLUMN_USE_ADDITIONAL_AUTH
#endif
};

}  // namespace

#if defined(SBROWSER_PASSWORD_ENCRYPTION)
static const char alphanum[] ="0123456789!@#$%^&*ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&(){}[]abcdefghijklmnopqrstuvwxyz";
#endif

LoginDatabase::LoginDatabase() {
}

LoginDatabase::~LoginDatabase() {
}

bool LoginDatabase::Init(const base::FilePath& db_path) {
  // Set pragmas for a small, private database (based on WebDatabase).
  db_.set_page_size(2048);
  db_.set_cache_size(32);
  db_.set_exclusive_locking();
  db_.set_restrict_to_user();
  LOG(INFO)<<"LoginDatabase:: LoginDatabase::Init  db_path= "<<db_path.MaybeAsASCII();
  if (!db_.Open(db_path)) {
    LOG(WARNING) << "Unable to open the password store database.";
    return false;
  }

  sql::Transaction transaction(&db_);
  transaction.Begin();

  // Check the database version.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    db_.Close();
    return false;
  }
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Password store database is too new.";
    db_.Close();
    return false;
  }

  // Initialize the tables.
  if (!InitLoginsTable()) {
    LOG(WARNING) << "Unable to initialize the password store database.";
    db_.Close();
    return false;
  }

  // Save the path for DeleteDatabaseFile().
  db_path_ = db_path;

  // If the file on disk is an older database version, bring it up to date.
  if (!MigrateOldVersionsAsNeeded()) {
    LOG(WARNING) << "Unable to migrate database";
    db_.Close();
    return false;
  }

  if (!transaction.Commit()) {
    db_.Close();
    return false;
  }

  return true;
}

bool LoginDatabase::MigrateOldVersionsAsNeeded() {
 LOG(INFO)<<"LoginDatabase:: LoginDatabase::  LoginDatabase::MigrateOldVersionsAsNeeded::Start version= "<<meta_table_.GetVersionNumber();
  switch (meta_table_.GetVersionNumber()) {
    case 1:
      if (!db_.Execute("ALTER TABLE logins "
                       "ADD COLUMN password_type INTEGER") ||
          !db_.Execute("ALTER TABLE logins "
                       "ADD COLUMN possible_usernames BLOB")) {
        return false;
      }
      meta_table_.SetVersionNumber(2);
      // Fall through.
    case 2:
      if (!db_.Execute("ALTER TABLE logins ADD COLUMN times_used INTEGER")) {
        return false;
      }
      meta_table_.SetVersionNumber(3);
      // Fall through.
    case 3:
      // We need to check if the column exists because of
      // https://crbug.com/295851
      if (!db_.DoesColumnExist("logins", "form_data") &&
          !db_.Execute("ALTER TABLE logins ADD COLUMN form_data BLOB")) {
        return false;
      }
      meta_table_.SetVersionNumber(4);
      // Fall through.
    case 4:
      if (!db_.Execute(
          "ALTER TABLE logins ADD COLUMN use_additional_auth INTEGER")) {
        return false;
      }
      AdditionalAuthDBMigration();
      meta_table_.SetVersionNumber(5);
      // Fall through.
    case kCurrentVersionNumber:
      // Already up to date
      LOG(INFO)<<"LoginDatabase:: LoginDatabase::  LoginDatabase::MigrateOldVersionsAsNeeded::kCurrentVersionNumber"<<kCurrentVersionNumber;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

void LoginDatabase::AdditionalAuthDBMigration(){

if(!((db_.DoesColumnExist("logins", "encrypted_generated_key"))&&(db_.DoesColumnExist("logins", "stored_iv"))&&(db_.DoesColumnExist("logins", "ct_size"))))
{
     if(!db_.Execute(
                "DELETE  FROM logins")){
        LOG(WARNING)<<"LoginDatabase::  unable to reset DB";
		return;
        }

    if(!db_.Execute(
                "ALTER TABLE logins ADD COLUMN encrypted_generated_key BLOB")){
        LOG(WARNING)<<"LoginDatabase:: unable to add column encrypted_generated_key";
        return ;
		}
    if(!db_.Execute(
                "ALTER TABLE logins ADD COLUMN stored_iv VARCHAR")){
        LOG(WARNING)<<"LoginDatabase:: unable to add column stored_iv";
        return ;
		}
    if(!db_.Execute(
                "ALTER TABLE logins ADD COLUMN ct_size INTEGER")){
        LOG(WARNING)<<"LoginDatabase:: unable to add column ct_size";
        return ;
		}
return ;
}
  LOG(INFO)<<"LoginDatabase::  column already exit";
  java_db_path= java_db_path.Append("/data/data/com.sec.android.app.sbrowser/databases/weblogin.db");
   if (!java_db_.Open(java_db_path)) {
  LOG(WARNING) << "LoginDatabase::Unable to open the password store database.";
   return ;
  }
  sql::Statement java_s(java_db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT username , website_action , website_origin, defaultUsername, formid FROM fpauth "));
  if (!java_s.is_valid())
         return ;
  base::string16 java_action_url;
  base::string16 java_username_value;
  base::string16 java_origin_url;
  base::string16 tmp= ASCIIToUTF16("%");
  sql::Statement native_s(db_.GetCachedStatement(SQL_FROM_HERE,
		"UPDATE logins SET "
		"use_additional_auth = ? "));
  if (!native_s.is_valid())
         return ;
     native_s.BindInt(0,0);
  if (!native_s.Run())
	 return ;
  while (java_s.Step()) {
          java_origin_url= tmp;
	  java_action_url=tmp;
          java_origin_url.append( java_s.ColumnString16(2)).append(tmp);
	  java_action_url.append( java_s.ColumnString16(1)).append(tmp);
          java_username_value  = java_s.ColumnString16(0);
      sql::Statement native_s(db_.GetCachedStatement(SQL_FROM_HERE,
		"UPDATE logins SET "
		"use_additional_auth = ? "
                "WHERE  origin_url LIKE ? AND "
                "username_value = ? AND "
                " action_url LIKE ?"));
	 native_s.BindInt(0, 1);
	 native_s.BindString16(1,java_origin_url);
	 native_s.BindString16(2, java_username_value);
	 native_s.BindString16(3,java_action_url);
	 if (!native_s.Run())
        return ;
  }
}

bool LoginDatabase::InitLoginsTable() {
  if (!db_.DoesTableExist("logins")) {
    if (!db_.Execute("CREATE TABLE logins ("
                     "origin_url VARCHAR NOT NULL, "
                     "action_url VARCHAR, "
                     "username_element VARCHAR, "
                     "username_value VARCHAR, "
                     "password_element VARCHAR, "
                     "password_value BLOB, "
                     "submit_element VARCHAR, "
                     "signon_realm VARCHAR NOT NULL,"
                     "ssl_valid INTEGER NOT NULL,"
                     "preferred INTEGER NOT NULL,"
                     "date_created INTEGER NOT NULL,"
                     "blacklisted_by_user INTEGER NOT NULL,"
                     "scheme INTEGER NOT NULL,"
                     "password_type INTEGER,"
                     "possible_usernames BLOB,"
                     "times_used INTEGER,"
                     "form_data BLOB,"
                     "use_additional_auth INTEGER,"
#if defined(SBROWSER_PASSWORD_ENCRYPTION)
	                 "encrypted_generated_key BLOB,"
                     "stored_iv VARCHAR,"
                     "ct_size INTEGER,"
#endif
                     "UNIQUE "
                     "(origin_url, username_element, "
                     "username_value, password_element, "
                     "submit_element, signon_realm))")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX logins_signon ON "
                     "logins (signon_realm)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

void LoginDatabase::ReportMetrics() {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT signon_realm, blacklisted_by_user, COUNT(username_value) "
      "FROM logins GROUP BY signon_realm, blacklisted_by_user"));

  if (!s.is_valid())
    return;

  int total_accounts = 0;
  int blacklisted_sites = 0;
  while (s.Step()) {
    int blacklisted = s.ColumnInt(1);
    int accounts_per_site = s.ColumnInt(2);
    if (blacklisted) {
      ++blacklisted_sites;
    } else {
      total_accounts += accounts_per_site;
      UMA_HISTOGRAM_CUSTOM_COUNTS("PasswordManager.AccountsPerSite",
                                  accounts_per_site, 0, 32, 6);
    }
  }
  UMA_HISTOGRAM_CUSTOM_COUNTS("PasswordManager.TotalAccounts",
                              total_accounts, 0, 32, 6);
  UMA_HISTOGRAM_CUSTOM_COUNTS("PasswordManager.BlacklistedSites",
                              blacklisted_sites, 0, 32, 6);

  sql::Statement usage_statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT password_type, times_used FROM logins"));

  if (!usage_statement.is_valid())
    return;

  while (usage_statement.Step()) {
    PasswordForm::Type type = static_cast<PasswordForm::Type>(
        usage_statement.ColumnInt(0));

    if (type == PasswordForm::TYPE_GENERATED) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PasswordManager.TimesGeneratedPasswordUsed",
          usage_statement.ColumnInt(1), 0, 100, 10);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PasswordManager.TimesPasswordUsed",
          usage_statement.ColumnInt(1), 0, 100, 10);
    }
  }
}

bool LoginDatabase::AddLogin(const PasswordForm& form) {
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
          ENCRYPTION_RESULT_SUCCESS)
    return false;

  // You *must* change LoginTableColumns if this query changes.
#if defined(SBROWSER_PASSWORD_ENCRYPTION)
  long ct_len = 0;
  std::string iv;
  std::string dek_raw_data ;
  unsigned char *ct = NULL;
  unsigned char *pt = NULL;

  unsigned char *IV = GenerateIVForEncDec();
  iv.assign((const  char*)IV);
  
  dek_raw_data = Encryptor::GetKey256(encrypted_password);
  pt = (unsigned char *) dek_raw_data.data();
  
  //fix for the dek_key containing 00 binary data for the entered password generate key using
  //chrome passkey
  if(strlen((const char *)pt )< 32) {
       encrypted_password = "Ekd15zhd";
       dek_raw_data = Encryptor::GetKey256(encrypted_password);
       pt = (unsigned char *) dek_raw_data.data();
  }

  Encryptor::EncryptString16_256(form.password_value, &encrypted_password,dek_raw_data); 

  ct = new  unsigned char [((dek_raw_data.length()/BSIZE)*BSIZE+BSIZE)];
  ct_len = WBS_Enc (ct, pt, strlen((const char *)pt), IV);
  LOG(INFO)<<"WBS: ct_length- "<<ct_len;

  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
	"INSERT OR REPLACE INTO logins "
	"(origin_url, action_url, username_element, username_value, "
	" password_element, password_value, submit_element, "
	" signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
	" scheme, password_type, possible_usernames, times_used, form_data, "
	" use_additional_auth, encrypted_generated_key, stored_iv, ct_size ) VALUES "
	"(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
#else
    sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR REPLACE INTO logins "
      "(origin_url, action_url, username_element, username_value, "
      " password_element, password_value, submit_element, "
      " signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      " scheme, password_type, possible_usernames, times_used, form_data, "
      " use_additional_auth) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
#endif
  s.BindString(COLUMN_ORIGIN_URL, form.origin.spec());
  s.BindString(COLUMN_ACTION_URL, form.action.spec());
  s.BindString16(COLUMN_USERNAME_ELEMENT, form.username_element);
  s.BindString16(COLUMN_USERNAME_VALUE, form.username_value);
  s.BindString16(COLUMN_PASSWORD_ELEMENT, form.password_element);
  //Encryptor::EncryptString16(form.password_value, &encrypted_password);
  s.BindBlob(COLUMN_PASSWORD_VALUE, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s.BindString16(COLUMN_SUBMIT_ELEMENT, form.submit_element);
  s.BindString(COLUMN_SIGNON_REALM, form.signon_realm);
  s.BindInt(COLUMN_SSL_VALID, form.ssl_valid);
  s.BindInt(COLUMN_PREFERRED, form.preferred);
  s.BindInt64(COLUMN_DATE_CREATED, form.date_created.ToTimeT());
  s.BindInt(COLUMN_BLACKLISTED_BY_USER, form.blacklisted_by_user);
  s.BindInt(COLUMN_SCHEME, form.scheme);
  s.BindInt(COLUMN_PASSWORD_TYPE, form.type);
  Pickle usernames_pickle = SerializeVector(form.other_possible_usernames);
  s.BindBlob(COLUMN_POSSIBLE_USERNAMES,
             usernames_pickle.data(),
             usernames_pickle.size());
  s.BindInt(COLUMN_TIMES_USED, form.times_used);
  Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s.BindBlob(COLUMN_FORM_DATA,
             form_data_pickle.data(),
             form_data_pickle.size());
  s.BindInt(COLUMN_USE_ADDITIONAL_AUTH, form.use_additional_authentication);

#if defined(SBROWSER_PASSWORD_ENCRYPTION)
  s.BindBlob(COLUMN_ENCRYPTED_GENERATED_KEY, ct,
                       ct_len);
  s.BindString(COLUMN_STORED_PLATFORM_IV, iv);
  s.BindInt(COLUMN_CT_SIZE,ct_len);
  if(ct){
  	delete []ct;
        ct = NULL;
  }
#endif
  return s.Run();
}

bool LoginDatabase::UpdateLogin(const PasswordForm& form, int* items_changed) {
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
          ENCRYPTION_RESULT_SUCCESS)
    return false;

#if defined(SBROWSER_PASSWORD_ENCRYPTION)
  long ct_size =0;
  std::string iv;
  std::string dek_raw_data ;
  unsigned char *pt = NULL;
  unsigned char *ct = NULL;

  unsigned char *IV = GenerateIVForEncDec();
  iv.assign((const  char*)IV);

  dek_raw_data = Encryptor::GetKey256(encrypted_password);
  pt = (unsigned char *) dek_raw_data.data();

  //fix for the dek_key containing 00 binary data for the entered password generate key using
  //chrome passkey
  if(strlen((const char *)pt )< 32) {
        encrypted_password = "Ekd15zhd";
        dek_raw_data = Encryptor::GetKey256(encrypted_password);
        pt = (unsigned char *) dek_raw_data.data();
   }

  Encryptor::EncryptString16_256(form.password_value, &encrypted_password,dek_raw_data);

   ct = new unsigned char[((dek_raw_data.length()/BSIZE)*BSIZE+BSIZE)];
   ct_size =   WBS_Enc ( ct, pt , strlen((const char *)pt),IV);
   LOG(INFO)<<"WBS: Update ct_size"<<ct_size;
#endif
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE logins SET "
      "action_url = ?, "
      "password_value = ?, "
      "ssl_valid = ?, "
      "preferred = ?, "
      "possible_usernames = ?, "
      "times_used = ?, "
#if defined(SBROWSER_PASSWORD_ENCRYPTION)
      "use_additional_auth = ?, "
      "encrypted_generated_key = ?, "
      "stored_iv = ?, "
      "ct_size = ? "
#else
      "use_additional_auth = ? "
#endif
      "WHERE origin_url = ? AND "
      "username_element = ? AND "
      "username_value = ? AND "
      "password_element = ? AND "
      "signon_realm = ?"));
  //Encryptor::EncryptString16(form.password_value, &encrypted_password);
  s.BindString(0, form.action.spec());
  s.BindBlob(1, encrypted_password.data(),
             static_cast<int>(encrypted_password.length()));
  s.BindInt(2, form.ssl_valid);
  s.BindInt(3, form.preferred);
  Pickle pickle = SerializeVector(form.other_possible_usernames);
  s.BindBlob(4, pickle.data(), pickle.size());
  s.BindInt(5, form.times_used);
  s.BindInt(6, form.use_additional_authentication);
#if defined(SBROWSER_PASSWORD_ENCRYPTION)
  s.BindBlob(7, ct,ct_size);
  s.BindString(8, iv);
  s.BindInt(9,ct_size);
  s.BindString(10, form.origin.spec());
  s.BindString16(11, form.username_element);
  s.BindString16(12, form.username_value);
  s.BindString16(13, form.password_element);
  s.BindString(14, form.signon_realm);
#else
  s.BindString(7, form.origin.spec());
  s.BindString16(8, form.username_element);
  s.BindString16(9, form.username_value);
  s.BindString16(10, form.password_element);
  s.BindString(11, form.signon_realm);
#endif

#if defined(SBROWSER_PASSWORD_ENCRYPTION)
  if(ct){
  	delete []ct;
	ct = NULL;
  }
#endif
  if (!s.Run())
    return false;

  if (items_changed)
    *items_changed = db_.GetLastChangeCount();

  return true;
}

bool LoginDatabase::RemoveLogin(const PasswordForm& form) {
  // Remove a login by UNIQUE-constrained fields.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM logins WHERE "
      "origin_url = ? AND "
      "username_element = ? AND "
      "username_value = ? AND "
      "password_element = ? AND "
      "submit_element = ? AND "
      "signon_realm = ? "));
  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString16(4, form.submit_element);
  s.BindString(5, form.signon_realm);

  return s.Run();
}

bool LoginDatabase::RemoveLoginsCreatedBetween(const base::Time delete_begin,
                                               const base::Time delete_end) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM logins WHERE "
      "date_created >= ? AND date_created < ?"));
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1, delete_end.is_null() ? std::numeric_limits<int64>::max()
                                      : delete_end.ToTimeT());

  return s.Run();
}

LoginDatabase::EncryptionResult LoginDatabase::InitPasswordFormFromStatement(
    PasswordForm* form,
    sql::Statement& s) const {
  std::string encrypted_password;
  base::string16 decrypted_password;
  int encrypted_password_len = s.ColumnByteLength(COLUMN_PASSWORD_VALUE);
  if (encrypted_password_len) {
         encrypted_password.resize(encrypted_password_len);
         memcpy(&encrypted_password[0], s.ColumnBlob(COLUMN_PASSWORD_VALUE), encrypted_password_len);
  }
  EncryptionResult encryption_result =
      DecryptedString(encrypted_password, &decrypted_password);
  if (encryption_result != ENCRYPTION_RESULT_SUCCESS)
    return encryption_result;

  std::string tmp = s.ColumnString(COLUMN_ORIGIN_URL);
  form->origin = GURL(tmp);
  tmp = s.ColumnString(COLUMN_ACTION_URL);
  form->action = GURL(tmp);
  form->username_element = s.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form->username_value = s.ColumnString16(COLUMN_USERNAME_VALUE);
  form->password_element = s.ColumnString16(COLUMN_PASSWORD_ELEMENT);
#if defined(SBROWSER_PASSWORD_ENCRYPTION)
  long pt_len=0;
  long  ct_len;
  std::string iv_value;
  unsigned char *iv;
  std::string encrypt_dek;
  std::string decrypt_dek;
  unsigned char *p_enc_dek = NULL;
  unsigned char *p_decrypted_dek = NULL;

  iv_value = s.ColumnString(COLUMN_STORED_PLATFORM_IV) ;
  iv = (unsigned char *) iv_value.c_str();
  
  s.ColumnBlobAsString(COLUMN_ENCRYPTED_GENERATED_KEY, &encrypt_dek);
  int len = s.ColumnByteLength(COLUMN_ENCRYPTED_GENERATED_KEY);
  if (len) {
       encrypt_dek.resize(len);
       memcpy(&encrypt_dek[0], s.ColumnBlob(COLUMN_ENCRYPTED_GENERATED_KEY), len);
  }else {
      LOG(INFO)<<"WBS: ENC_DEK FAIL length is null";
   }   
  if(len) {
   ct_len = s.ColumnInt(COLUMN_CT_SIZE);
   if(ct_len <= 0) { //need to remove this after testing 
        LOG(INFO)<<"WBS: Return cl_len="<<ct_len;  //return ENCRYPTION_RESULT_SERVICE_FAILURE;
        ct_len = 48; //assigne 48 byte preventive check to avoid crash while allocating memory
   }
  p_enc_dek = (unsigned char *) encrypt_dek.data(); //input  
  p_decrypted_dek = new unsigned char[ct_len];
  pt_len = WBS_Dec (p_decrypted_dek, p_enc_dek, ct_len, iv);
  decrypt_dek .assign((const  char*)p_decrypted_dek); //decrypted DEK should not have any null char
  LOG(INFO)<<"WBS: ct_size read as "<<ct_len<<" : pt_len -> "<<pt_len;

  Encryptor::DecryptString16_256(encrypted_password, &decrypted_password,decrypt_dek);

  if(p_decrypted_dek){
  	delete []p_decrypted_dek;
	p_decrypted_dek = NULL;
  }
  }
#endif
  //Encryptor::DecryptString16(encrypted_password, &decrypted_password);
  form->password_value = decrypted_password;
  form->submit_element = s.ColumnString16(COLUMN_SUBMIT_ELEMENT);
  tmp = s.ColumnString(COLUMN_SIGNON_REALM);
  form->signon_realm = tmp;
  form->ssl_valid = (s.ColumnInt(COLUMN_SSL_VALID) > 0);
  form->preferred = (s.ColumnInt(COLUMN_PREFERRED) > 0);
  form->date_created = base::Time::FromTimeT(
      s.ColumnInt64(COLUMN_DATE_CREATED));
  form->blacklisted_by_user = (s.ColumnInt(COLUMN_BLACKLISTED_BY_USER) > 0);
  int scheme_int = s.ColumnInt(COLUMN_SCHEME);
  DCHECK((scheme_int >= 0) && (scheme_int <= PasswordForm::SCHEME_OTHER));
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
  int type_int = s.ColumnInt(COLUMN_PASSWORD_TYPE);
  DCHECK(type_int >= 0 && type_int <= PasswordForm::TYPE_GENERATED);
  form->type = static_cast<PasswordForm::Type>(type_int);
  Pickle pickle(
      static_cast<const char*>(s.ColumnBlob(COLUMN_POSSIBLE_USERNAMES)),
      s.ColumnByteLength(COLUMN_POSSIBLE_USERNAMES));
  form->other_possible_usernames = DeserializeVector(pickle);
  form->times_used = s.ColumnInt(COLUMN_TIMES_USED);
  Pickle form_data_pickle(
      static_cast<const char*>(s.ColumnBlob(COLUMN_FORM_DATA)),
      s.ColumnByteLength(COLUMN_FORM_DATA));
  PickleIterator form_data_iter(form_data_pickle);
  autofill::DeserializeFormData(&form_data_iter, &form->form_data);
  form->use_additional_authentication =
      (s.ColumnInt(COLUMN_USE_ADDITIONAL_AUTH) > 0);
  return ENCRYPTION_RESULT_SUCCESS;
}

bool LoginDatabase::GetLogins(const PasswordForm& form,
                              std::vector<PasswordForm*>* forms) const {
  DCHECK(forms);
  // You *must* change LoginTableColumns if this query changes.
#if defined(SBROWSER_PASSWORD_ENCRYPTION)
  const std::string sql_query = "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "use_additional_auth, encrypted_generated_key, stored_iv, ct_size FROM logins WHERE signon_realm == ? ";
#else
  const std::string sql_query = "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "use_additional_auth FROM logins WHERE signon_realm == ? ";
#endif
  sql::Statement s;
  const GURL signon_realm(form.signon_realm);
  std::string registered_domain =
      PSLMatchingHelper::GetRegistryControlledDomain(signon_realm);
  PSLMatchingHelper::PSLDomainMatchMetric psl_domain_match_metric =
      PSLMatchingHelper::PSL_DOMAIN_MATCH_NONE;
  if (psl_helper_.ShouldPSLDomainMatchingApply(registered_domain)) {
    // We are extending the original SQL query with one that includes more
    // possible matches based on public suffix domain matching. Using a regexp
    // here is just an optimization to not have to parse all the stored entries
    // in the |logins| table. The result (scheme, domain and port) is verified
    // further down using GURL. See the functions SchemeMatches,
    // RegistryControlledDomainMatches and PortMatches.
    const std::string extended_sql_query =
        sql_query + "OR signon_realm REGEXP ? ";
    // TODO(nyquist) Re-enable usage of GetCachedStatement when
    // http://crbug.com/248608 is fixed.
    s.Assign(db_.GetUniqueStatement(extended_sql_query.c_str()));
    // We need to escape . in the domain. Since the domain has already been
    // sanitized using GURL, we do not need to escape any other characters.
    base::ReplaceChars(registered_domain, ".", "\\.", &registered_domain);
    std::string scheme = signon_realm.scheme();
    // We need to escape . in the scheme. Since the scheme has already been
    // sanitized using GURL, we do not need to escape any other characters.
    // The scheme soap.beep is an example with '.'.
    base::ReplaceChars(scheme, ".", "\\.", &scheme);
    const std::string port = signon_realm.port();
    // For a signon realm such as http://foo.bar/, this regexp will match
    // domains on the form http://foo.bar/, http://www.foo.bar/,
    // http://www.mobile.foo.bar/. It will not match http://notfoo.bar/.
    // The scheme and port has to be the same as the observed form.
    std::string regexp = "^(" + scheme + ":\\/\\/)([\\w-]+\\.)*" +
                         registered_domain + "(:" + port + ")?\\/$";
    s.BindString(0, form.signon_realm);
    s.BindString(1, regexp);
  } else {
    psl_domain_match_metric = PSLMatchingHelper::PSL_DOMAIN_MATCH_DISABLED;
    s.Assign(db_.GetCachedStatement(SQL_FROM_HERE, sql_query.c_str()));
    s.BindString(0, form.signon_realm);
  }

  while (s.Step()) {
    scoped_ptr<PasswordForm> new_form(new PasswordForm());
    EncryptionResult result = InitPasswordFormFromStatement(new_form.get(), s);
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return false;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE)
      continue;
    DCHECK(result == ENCRYPTION_RESULT_SUCCESS);
    if (psl_helper_.IsMatchingEnabled()) {
      if (!PSLMatchingHelper::IsPublicSuffixDomainMatch(new_form->signon_realm,
                                                         form.signon_realm)) {
        // The database returned results that should not match. Skipping result.
        continue;
      }
      if (form.signon_realm != new_form->signon_realm) {
        psl_domain_match_metric = PSLMatchingHelper::PSL_DOMAIN_MATCH_FOUND;
        // This is not a perfect match, so we need to create a new valid result.
        // We do this by copying over origin, signon realm and action from the
        // observed form and setting the original signon realm to what we found
        // in the database. We use the fact that |original_signon_realm| is
        // non-empty to communicate that this match was found using public
        // suffix matching.
        new_form->original_signon_realm = new_form->signon_realm;
        new_form->origin = form.origin;
        new_form->signon_realm = form.signon_realm;
        new_form->action = form.action;
      }
    }
    forms->push_back(new_form.release());
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.PslDomainMatchTriggering",
                            psl_domain_match_metric,
                            PSLMatchingHelper::PSL_DOMAIN_MATCH_COUNT);
  return s.Succeeded();
}

bool LoginDatabase::GetLoginsCreatedBetween(
    const base::Time begin,
    const base::Time end,
    std::vector<autofill::PasswordForm*>* forms) const {
  DCHECK(forms);
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "use_additional_auth FROM logins "
      "WHERE date_created >= ? AND date_created < ?"
      "ORDER BY origin_url"));
  s.BindInt64(0, begin.ToTimeT());
  s.BindInt64(1, end.is_null() ? std::numeric_limits<int64>::max()
                               : end.ToTimeT());

  while (s.Step()) {
    scoped_ptr<PasswordForm> new_form(new PasswordForm());
    EncryptionResult result = InitPasswordFormFromStatement(new_form.get(), s);
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return false;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE)
      continue;
    DCHECK(result == ENCRYPTION_RESULT_SUCCESS);
    forms->push_back(new_form.release());
  }
  return s.Succeeded();
}

bool LoginDatabase::GetAutofillableLogins(
    std::vector<PasswordForm*>* forms) const {
  return GetAllLoginsWithBlacklistSetting(false, forms);
}

bool LoginDatabase::GetBlacklistLogins(
    std::vector<PasswordForm*>* forms) const {
  return GetAllLoginsWithBlacklistSetting(true, forms);
}

bool LoginDatabase::GetAllLoginsWithBlacklistSetting(
    bool blacklisted, std::vector<PasswordForm*>* forms) const {
  DCHECK(forms);
  // You *must* change LoginTableColumns if this query changes.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "use_additional_auth FROM logins WHERE blacklisted_by_user == ? "
      "ORDER BY origin_url"));
  s.BindInt(0, blacklisted ? 1 : 0);

  while (s.Step()) {
    scoped_ptr<PasswordForm> new_form(new PasswordForm());
    EncryptionResult result = InitPasswordFormFromStatement(new_form.get(), s);
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return false;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE)
      continue;
    DCHECK(result == ENCRYPTION_RESULT_SUCCESS);
    forms->push_back(new_form.release());
  }
  return s.Succeeded();
}

bool LoginDatabase::DeleteAndRecreateDatabaseFile() {
  DCHECK(db_.is_open());
  meta_table_.Reset();
  db_.Close();
  sql::Connection::Delete(db_path_);
  return Init(db_path_);
}

Pickle LoginDatabase::SerializeVector(
    const std::vector<base::string16>& vec) const {
  Pickle p;
  for (size_t i = 0; i < vec.size(); ++i) {
    p.WriteString16(vec[i]);
  }
  return p;
}

std::vector<base::string16> LoginDatabase::DeserializeVector(
    const Pickle& p) const {
  std::vector<base::string16> ret;
  base::string16 str;

  PickleIterator iterator(p);
  while (iterator.ReadString16(&str)) {
    ret.push_back(str);
  }
  return ret;
}

#if defined(SBROWSER_PASSWORD_ENCRYPTION) 
unsigned char* LoginDatabase::GenerateIVForEncDec()
{
  srand(time(0));
  unsigned int i = 0;
  int stringRange= sizeof(alphanum) - 1;
  for( ;  i < BSIZE; i++)
  	resultIVkey[i]=alphanum[rand() % stringRange];
  resultIVkey[i] = 0;	//terminating IV with null

  return resultIVkey;
}
#endif

