/*
 * Copyright (C) 2013 Samsung Electronics
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
 * @defgroup android_config android_config
 * @{ @ingroup android
 */

#ifndef ANDROID_CONFIG_H_
#define ANDROID_CONFIG_H_

#include <sa/authenticator.h>
#include <utils/identification.h>

typedef struct android_rsa_config_t android_rsa_config_t;
struct android_rsa_config_t {
  char *private_key;
  char *user_cert;
  char *ca_cert;
  char *server_cert;
  char *ocsp_server_url;
};

typedef struct android_psk_config_t android_psk_config_t;
struct android_psk_config_t {
  identification_t *ipsec_identifier;
  char *ipsec_secret;
};

typedef struct android_config_t android_config_t;
struct android_config_t {

  auth_method_t auth_method; /* supports 
				AUTH_XAUTH_INIT_PSK, 
				AUTH_XAUTH_INIT_RSA and
			        AUTH_PSK 
			        AUTH_RSA */

  char *sgw;

  identification_t *xauth_username;
  char *xauth_password;

  union
  {
    android_psk_config_t psk;
    android_rsa_config_t rsa;
  };

  void (*destroy)(android_config_t *this);
};

/**
 * Create an Android condfig instance.
 *
 */
android_config_t *android_config_create();

android_config_t *read_android_config(int control_socket_fd);

#endif /* ANDROID_CONFIG_H_ */

