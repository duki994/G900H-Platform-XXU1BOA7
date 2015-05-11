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

#include <errno.h>
#include <assert.h>

#include "android_config.h"

#define FREE_IF(x) if (x) free(x);
#define ZERO_FREE_STRING_IF(x) if (x) {zero_string_buffer(x); free(x);}

static const char EOT = 0x04;

typedef struct private_android_config_t private_android_config_t;
struct private_android_config_t
{
  android_config_t public;
};

/**
 * Read a string argument from the Android control socket
 */
static char *read_argument(int fd, uint16_t length)
{
	int offset = 0;
	char *data = malloc(length + 1);
	if (data)
	  {
	    while (offset < length)
	      {
		int n = recv(fd, &data[offset], length - offset, 0);
		if (n < 0)
		  {
		    DBG1(DBG_CFG, "failed to read argument from Android"
			 " control socket: %s", strerror(errno));
		    free(data);
		    return NULL;
		  }
		offset += n;
	      }
	    data[length] = '\0';
	  }
	else
	  {
	    DBG1(DBG_CFG, "Failed to allocate memory for argument");
	  }
	return data;
}

/**
 * Read a string argument from the Android control socket
 */
static int read_argument_string(int fd, char **argument)
{       
	uint16_t length = 0;
	unsigned char c = 0;
	int err = 0;
	char *a = NULL;

	err = recv(fd, &c, sizeof(c), 0);
	if ( err != sizeof(c) )
	{
		return err;
	}

	length = c;
	length = length << 8;

	err = recv(fd, &c, sizeof(c), 0);
	if ( err != sizeof(c) )
	{
		return err;
	}
	err = 0;
	length |= c;
	/* DBG1(DBG_CFG, "Reading argument: length %d", length); */

	switch(length)
	  {
	  case 0xffff:
		err = EOT;
	    break;
	  case 0:	  	
	        *argument = malloc(1);
		if (*argument != NULL) 
		{
			memset(*argument, 0, 1);
		}
		else
		{
			err = ENOMEM;
		}
	    break;
	  default:
		*argument = read_argument(fd, length);
		if (*argument == NULL)
		  {
		    err = ENOMEM;
		  }
		/* DBG4(DBG_CFG, "Read argument %s", *argument); */
	    break;
	  }

	return err;
}

int read_xauth_arguments(private_android_config_t *this, int fd)
{
  char *string = NULL;
  int err = read_argument_string(fd, &string);
  if (err != 0) goto error;  
  this->public.xauth_username = identification_create_from_string(string);
  free(string);
  string = NULL;

  err = read_argument_string(fd, &(this->public.xauth_password));
  if (err != 0) goto error;  

 error:
  FREE_IF(string);
  return err;
}

int read_arguments(private_android_config_t *this, int fd)
{
  int err = 0;
  char *string = NULL;

  err = read_argument_string(fd, &(this->public.sgw));
  if (err != 0) goto error;

  err = read_argument_string(fd, &string);
  if (err != 0) goto error;

  if (strcmp(string,"xauthrsa") == 0)
    {
      this->public.auth_method = AUTH_XAUTH_INIT_RSA;
    }
  else if (strcmp(string,"xauthpsk") == 0)
    {
      this->public.auth_method = AUTH_XAUTH_INIT_PSK;
    }
  else if (strcmp(string,"ikev2psk") == 0)
    {
      this->public.auth_method = AUTH_PSK;
    }
  else if (strcmp(string,"ikev2rsa") == 0)
    {
      this->public.auth_method = AUTH_RSA;
    }
  else
    {
      DBG1(DBG_CFG, "Read unkown connection type %s", string);
 

      free(string);
      string = NULL;

      /* Config type not supported */
      err = EINVAL; 
      goto error;
    }
  free(string);
  string = NULL;

  switch(this->public.auth_method)
    {
    case AUTH_XAUTH_INIT_RSA:
      err = read_argument_string(fd, &(this->public.rsa.private_key));
      if (err != 0) goto error;
      err = read_argument_string(fd, &(this->public.rsa.user_cert));
      if (err != 0) goto error;
      err = read_argument_string(fd, &(this->public.rsa.ca_cert));
      if (err != 0) goto error;
      err = read_argument_string(fd, &(this->public.rsa.server_cert));
      if (err != 0) goto error;
      err = read_xauth_arguments(this, fd);
      if (err != 0) goto error;
      break;
    case AUTH_XAUTH_INIT_PSK:
      err = read_argument_string(fd, &string);
      if (err != 0) goto error;

      if (strlen(string) > 0)
	{
	  this->public.psk.ipsec_identifier = identification_create_from_string(string);
	}
      else
	{
	  //ipsec identifier not set. We should use our own IP.
	  this->public.psk.ipsec_identifier = identification_create_from_string("%any");
	}
      free(string);
      string = NULL;

      err = read_argument_string(fd, &(this->public.psk.ipsec_secret));
      if (err != 0) goto error;
   
      err = read_xauth_arguments(this, fd);
      if (err != 0) goto error;   

      break;
    case AUTH_PSK:
      err = read_argument_string(fd, &string);
      if (err != 0) goto error;

      if (strlen(string) > 0)
	{
	  this->public.psk.ipsec_identifier = identification_create_from_string(string);
	}
      else
	{
	  //ipsec identifier not set. We should use our own IP.
	  this->public.psk.ipsec_identifier = identification_create_from_string("%any");
	}
      free(string);
      string = NULL;

      err = read_argument_string(fd, &(this->public.psk.ipsec_secret));
      if (err != 0) goto error;            
      break;
    case AUTH_RSA:
      err = read_argument_string(fd, &(this->public.rsa.private_key));
      if (err != 0) goto error;
      err = read_argument_string(fd, &(this->public.rsa.user_cert));
      if (err != 0) goto error;
      err = read_argument_string(fd, &(this->public.rsa.ca_cert));
      if (err != 0) goto error;
      err = read_argument_string(fd, &(this->public.rsa.server_cert));
      if (err != 0) goto error;
      err = read_argument_string(fd, &(this->public.rsa.ocsp_server_url));
      if (err != 0) goto error;
      break;
    default:
      //Unknown connection type. 
      //This should have been checked already.
      //Something has gone wrong...
      assert(false);
      break;
    }

  /* We have read all arguments. This read should be  EOT mark */
  err = read_argument_string(fd, &string);
  if (err == 0)
    {
      /* Unexpected configuration parameter received */
      err = EINVAL;
    }
  else if (err == EOT)
    {
      /* Got EOT mark as expected */
      err = 0;
    }

error:
    FREE_IF(string);
    return err;
}


void zero_string_buffer(char *string_buffer)
{
  while(*string_buffer != '\0')
  {
    *string_buffer = '\0';
    string_buffer++;
  } 
}

METHOD(android_config_t, destroy, void,
	   private_android_config_t *this)
{
  FREE_IF(this->public.sgw);

  DESTROY_IF(this->public.xauth_username);

  ZERO_FREE_STRING_IF(this->public.xauth_password);

  if (this->public.auth_method == AUTH_XAUTH_INIT_RSA ||
      this->public.auth_method == AUTH_RSA)
    {
      ZERO_FREE_STRING_IF(this->public.rsa.private_key);
      FREE_IF(this->public.rsa.user_cert);
      FREE_IF(this->public.rsa.ca_cert);
      FREE_IF(this->public.rsa.server_cert);
      FREE_IF(this->public.rsa.ocsp_server_url);
    }
  else if (this->public.auth_method == AUTH_XAUTH_INIT_PSK ||
           this->public.auth_method == AUTH_PSK)
    {
      DESTROY_IF(this->public.psk.ipsec_identifier);
      ZERO_FREE_STRING_IF(this->public.psk.ipsec_secret);
    }

  free(this);
}

android_config_t *android_config_create()
{
  private_android_config_t *this;
  
  INIT(this,
       .public = {
	   .destroy = _destroy,
	   },
       );
  
  return &this->public;
}

android_config_t *read_android_config(int control_socket_fd)
{
  android_config_t *this = android_config_create();
  if (this)
    {
      int err = read_arguments((private_android_config_t*)this, control_socket_fd);
      if (err != 0)
	{	 
	  DBG1(DBG_CFG, "Failed to read connection config");
	  this->destroy(this);
	  this = NULL;
	}
    }

  return this;
}
