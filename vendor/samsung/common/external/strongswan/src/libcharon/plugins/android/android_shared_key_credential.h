/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd 
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
 * @defgroup android_shared_key_credential android_shared_key_credential
 * @{ @ingroup android
 */
#ifndef ANDROID_SHARED_KEY_CREDENTIAL_H_
#define ANDROID_SHARED_KEY_CREDENTIAL_H_

#include <utils/identification.h>
#include <credentials/keys/shared_key.h>

typedef struct shared_key_credential_t shared_key_credential_t;
struct shared_key_credential_t
{
  identification_t* id;
  shared_key_t* key;
  shared_key_type_t key_type;
  
  void (*destroy)(shared_key_credential_t* this);
};

shared_key_credential_t* shared_key_credential_t_create(identification_t *id, 
							shared_key_t *shared_key, 
							shared_key_type_t key_type);


#endif // ANDROID_SHARED_KEY_CREDENTIAL_H_
