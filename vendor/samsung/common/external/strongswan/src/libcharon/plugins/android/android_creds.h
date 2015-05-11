/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd 
 * Copyright (C) 2010 Tobias Brunner
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

/**
 * @defgroup android_creds android_creds
 * @{ @ingroup android
 */

#ifndef ANDROID_CREDS_H_
#define ANDROID_CREDS_H_

#include <credentials/credential_set.h>

typedef struct android_creds_t android_creds_t;

/**
 * Android credentials helper.
 */
struct android_creds_t {

	/**
	 * Implements credential_set_t
	 */
	credential_set_t set;

	/**
	 * Add certificate to this set.
	 *
	 * @param cert_pem		certificate in PEM encoding
	 */
	void (*add_certificate)(android_creds_t *this, char *cert_pem);


	/**
	 * Add private key to this set from Android key store
	 *
	 * @param key		private key in PEM encoding
	 */
	bool (*add_private_key)(android_creds_t *this, char *key, bool ikev2);

	/**
	 * Add OCSP url
	 *
	 * @param name		oscp url
	 */
	bool (*add_ocsp_url)(android_creds_t *this, char *url);


	/**
	 * Set the username and password for authentication.
	 *
	 * @param id		ID of the user
	 * @param password	password to use for authentication
	 */
	bool (*set_username_password)(android_creds_t *this, identification_t *id,
				      char *password, bool is_xauth);

	/**
	 * Clear the stored credentials.
	 */
	void (*clear)(android_creds_t *this);

	/**
	 * Destroy a android_creds instance.
	 */
	void (*destroy)(android_creds_t *this);

};

/**
 * Create an android_creds instance.
 */
android_creds_t *android_creds_create();

#endif /** ANDROID_CREDS_H_ @}*/
