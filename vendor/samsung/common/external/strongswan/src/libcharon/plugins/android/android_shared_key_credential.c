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

#include "android_shared_key_credential.h"

METHOD(shared_key_credential_t, 
       shared_key_credential_destroy, void,
       shared_key_credential_t *this)
{
  if (this->id != NULL)
    {
      this->id->destroy(this->id);
      this->id = NULL;
    }

  if (this->key != NULL)
    {
      this->key->destroy(this->key);
      this->key = NULL;
    }

  free(this);
}

shared_key_credential_t* shared_key_credential_t_create(identification_t *id, 
                                                          shared_key_t *shared_key, 
                                                          shared_key_type_t key_type)
{
  shared_key_credential_t *this;

  INIT(this,
       .id = id,
       .key = shared_key,
       .key_type = key_type,
       .destroy = _shared_key_credential_destroy
       );
  return this;
}

