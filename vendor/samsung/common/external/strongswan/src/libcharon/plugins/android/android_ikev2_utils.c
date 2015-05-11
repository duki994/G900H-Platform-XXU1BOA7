/*
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
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

#include <utils/debug.h>
#include <assert.h>

#include <daemon.h>
#include "android_ikev2_utils.h"


static const char ikev2_proposal_cbc[] =
    "aes256-aes128"
    "-sha512-sha384-sha256-sha1"
    "-modp2048s256-ecp384-ecp256-modp2048-modp1536";
static const char ikev2_proposal_gcm[] =
    "aes256gcm16-aes128gcm16"
    "-prfsha512-prfsha384-prfsha256-prfsha1"
    "-modp2048s256-ecp384-ecp256-modp2048-modp1536";

static const char child_proposal_gcm[] =
    "aes256gcm16-aes128gcm16";
static const char child_proposal_gcm_dh[] =
    "aes256gcm16-aes128gcm16"
    "-modp2048s256-ecp384-ecp256-modp2048-modp1536";
static const char child_proposal_cbc[] =
    "aes256-aes128"
    "-sha512-sha384-sha256-sha1";
static const char child_proposal_cbc_dh[] =
    "aes256-aes128"
    "-sha512-sha384-sha256-sha1"
    "-modp2048s256-ecp384-ecp256-modp2048-modp1536";

static const int DPD_INTERVAL = 60;


static bool add_child_proposals(child_cfg_t *child_cfg)
{
    proposal_t* proposal = proposal_create_from_string(PROTO_ESP, child_proposal_gcm);
    if (!proposal)
    {
        DBG1(DBG_CFG, "Failed to create child proposal");
        return FALSE;
    }
    child_cfg->add_proposal(child_cfg, proposal);

    proposal = proposal_create_from_string(PROTO_ESP, child_proposal_gcm_dh);
    if (proposal)
    {
        child_cfg->add_proposal(child_cfg, proposal);
    }

    proposal = proposal_create_from_string(PROTO_ESP, child_proposal_cbc);
    if (proposal)
    {
        child_cfg->add_proposal(child_cfg, proposal);
    }

    proposal = proposal_create_from_string(PROTO_ESP, child_proposal_cbc_dh);
    if (proposal)
    {
        child_cfg->add_proposal(child_cfg, proposal);
    }

    return TRUE;
}

static bool add_ike_proposals(ike_cfg_t *ike_cfg)
{
    proposal_t* proposal = proposal_create_from_string(PROTO_IKE, ikev2_proposal_cbc);
    if (!proposal)
    {
        DBG1(DBG_CFG, "Failed to create ike proposal");
        return FALSE;
    }
    ike_cfg->add_proposal(ike_cfg, proposal);

    proposal = proposal_create_from_string(PROTO_IKE, ikev2_proposal_gcm);
    if (proposal)
    {
        ike_cfg->add_proposal(ike_cfg, proposal);
    }

    return TRUE;
}

void ikev2_psk_configs_create(android_config_t *configs,
			      peer_cfg_t **peer_c,
			      child_cfg_t **child_c)
{
  ike_cfg_t *ike_cfg;
  traffic_selector_t *ts;
  peer_cfg_t *peer_cfg;
  child_cfg_t *child_cfg;

  auth_cfg_t *auth;
  lifetime_cfg_t lifetime = {
    .time = {
      .life = 10800, /* 3h */
      .rekey = 10200, /* 2h50min */
      .jitter = 300 /* 5min */
    }
  };

  identification_t *gateway_id = identification_create_from_string("%any");

  *peer_c = NULL;
  *child_c = NULL;

  assert(configs->auth_method == AUTH_PSK);

  ike_cfg = ike_cfg_create(IKEV2, TRUE, FALSE, "0.0.0.0",
			   charon->socket->get_port(charon->socket, FALSE),
			   configs->sgw, IKEV2_UDP_PORT, FRAGMENTATION_NO, 0);

  if (!add_ike_proposals(ike_cfg))
    {
      /* failed */
      gateway_id->destroy(gateway_id);
      return;
    }
  
  peer_cfg = peer_cfg_create("android", ike_cfg, CERT_SEND_IF_ASKED,
			     UNIQUE_REPLACE, 1, /* keyingtries */
			     36000, 0, /* rekey 10h, reauth none */
			     600, 600, /* jitter, over 10min */
			     FALSE, FALSE, /* mobike, aggressive */
			     TRUE, DPD_INTERVAL, 0, /* DPD delay, timeout */
			     FALSE, NULL, NULL); /* mediation */
  peer_cfg->add_virtual_ip(peer_cfg,  host_create_from_string("0.0.0.0", 0));
  
  /* Adds local side PSK authentication */
  auth = auth_cfg_create();
  auth->add(auth, AUTH_RULE_AUTH_CLASS, AUTH_CLASS_PSK);
  auth->add(auth, AUTH_RULE_IDENTITY, 
	    configs->psk.ipsec_identifier->clone(configs->psk.ipsec_identifier));
  auth->add(auth, AUTH_RULE_GROUP, 
	    configs->psk.ipsec_identifier->clone(configs->psk.ipsec_identifier));
  peer_cfg->add_auth_cfg(peer_cfg, auth, TRUE);
    
  /* Add remote PSK side authentication */
  auth = auth_cfg_create();
  auth->add(auth, AUTH_RULE_AUTH_CLASS, AUTH_CLASS_PSK);
  auth->add(auth, AUTH_RULE_IDENTITY, gateway_id);
  peer_cfg->add_auth_cfg(peer_cfg, auth, FALSE);
  
  child_cfg = child_cfg_create("android", &lifetime, NULL, TRUE, MODE_TUNNEL,
			       ACTION_NONE, ACTION_NONE, ACTION_NONE, FALSE,
			       0, 0, NULL, NULL, 0);
  
  if (!add_child_proposals(child_cfg))
    {
      /*failed*/
      return;
    }

  ts = traffic_selector_create_dynamic(0, 0, 65535);
  child_cfg->add_traffic_selector(child_cfg, TRUE, ts);
  ts = traffic_selector_create_from_string(0, TS_IPV4_ADDR_RANGE, "0.0.0.0",
					   0, "255.255.255.255", 65535);
  child_cfg->add_traffic_selector(child_cfg, FALSE, ts);
  
  /*Take extra ref, because add claims ownership. */
  *child_c = child_cfg;
  (*child_c)->get_ref(*child_c);

  peer_cfg->add_child_cfg(peer_cfg, child_cfg);
  
  *peer_c = peer_cfg;
}

void ikev2_rsa_configs_create(android_config_t *configs,
			      peer_cfg_t **peer_c,
			      child_cfg_t **child_c)
{
  ike_cfg_t *ike_cfg;
  traffic_selector_t *ts;
  peer_cfg_t *peer_cfg;
  child_cfg_t *child_cfg;

  chunk_t temp_chunk;
  certificate_t *temp_cert;
  identification_t *temp_id = NULL;

  auth_cfg_t *auth;
  lifetime_cfg_t lifetime = {
    .time = {
      .life = 10800, /* 3h */
      .rekey = 10200, /* 2h50min */
      .jitter = 300 /* 5min */
    }
  };

  /* send cert req if we don't have the sgw cert and if we have
     CA to validate the cert. */
  bool send_cert_req = ( strlen(configs->rsa.server_cert) == 0 );
  
  assert(configs->auth_method == AUTH_RSA);

  *peer_c = NULL;
  *child_c = NULL;

#ifdef FORCE_FRAGMENT_CONF
  DBG1(DBG_CFG, "Set IKEV2 RSA FORCE fragment ON");
  ike_cfg = ike_cfg_create(IKEV2, send_cert_req, FALSE, "0.0.0.0",
			   charon->socket->get_port(charon->socket, FALSE),
			   configs->sgw, IKEV2_UDP_PORT, FRAGMENTATION_FORCE, 0);
#else
  ike_cfg = ike_cfg_create(IKEV2, send_cert_req, FALSE, "0.0.0.0",
			   charon->socket->get_port(charon->socket, FALSE),
			   configs->sgw, IKEV2_UDP_PORT, FRAGMENTATION_YES, 0);
#endif

  if (!add_ike_proposals(ike_cfg))
    {
      /* failed */
      return;
    }

  peer_cfg = peer_cfg_create("android", ike_cfg, CERT_SEND_IF_ASKED,
			     UNIQUE_REPLACE, 1, /* keyingtries */
			     36000, 0, /* rekey 10h, reauth none */
			     600, 600, /* jitter, over 10min */
			     FALSE, FALSE, /* mobike, aggressive */
			     TRUE, DPD_INTERVAL, 0, /* DPD delay, timeout */
			     FALSE, NULL, NULL); /* mediation */
  peer_cfg->add_virtual_ip(peer_cfg,  host_create_from_string("0.0.0.0", 0));
  
  /* Adds local side PUBKEY authentication */
  auth = auth_cfg_create();
  auth->add(auth, AUTH_RULE_AUTH_CLASS, AUTH_CLASS_PUBKEY);

  /* client ID for the auth */
  temp_chunk.ptr = configs->rsa.user_cert;
  temp_chunk.len = strlen(configs->rsa.user_cert);
  temp_cert = lib->creds->create(lib->creds, CRED_CERTIFICATE, CERT_X509,
			    BUILD_BLOB_PEM, temp_chunk, BUILD_END);
  if (temp_cert == NULL)
    {
      DBG1(DBG_CFG, "Failed to create cert from user cert data");
      return;
    }
  
  temp_id = temp_cert->get_subject(temp_cert);
  auth->add(auth, AUTH_RULE_IDENTITY, 
	    temp_id->clone(temp_id));
  temp_id = NULL;

  temp_cert->destroy(temp_cert);
  temp_cert = NULL;

  peer_cfg->add_auth_cfg(peer_cfg, auth, TRUE);
    
  /* Add remote side RSA authentication */
  auth = auth_cfg_create();
  auth->add(auth, AUTH_RULE_AUTH_CLASS, AUTH_CLASS_PUBKEY);

  if (strlen(configs->rsa.server_cert) > 0)
    {      
      temp_chunk.ptr = configs->rsa.server_cert;
      temp_chunk.len = strlen(configs->rsa.server_cert);
      temp_cert = lib->creds->create(lib->creds, CRED_CERTIFICATE, CERT_X509,
				     BUILD_BLOB_PEM, temp_chunk, BUILD_END);
      if (temp_cert == NULL)
	{
	  DBG1(DBG_CFG, "Failed to create cert from user cert data");
	  return;
	}
      temp_id = temp_cert->get_subject(temp_cert);
      temp_id = temp_id->clone(temp_id);
      temp_cert->destroy(temp_cert);
    }
  else
    {
      temp_id = identification_create_from_string("%any");
    }

  auth->add(auth, AUTH_RULE_IDENTITY, temp_id);
  peer_cfg->add_auth_cfg(peer_cfg, auth, FALSE);

  child_cfg = child_cfg_create("android", &lifetime, NULL, TRUE, MODE_TUNNEL,
			       ACTION_NONE, ACTION_NONE, ACTION_NONE, FALSE,
			       0, 0, NULL, NULL, 0);

  if (!add_child_proposals(child_cfg))
    {
      /*failed*/
      return;
    }

  ts = traffic_selector_create_dynamic(0, 0, 65535);
  child_cfg->add_traffic_selector(child_cfg, TRUE, ts);
  ts = traffic_selector_create_from_string(0, TS_IPV4_ADDR_RANGE, "0.0.0.0",
					   0, "255.255.255.255", 65535);
  child_cfg->add_traffic_selector(child_cfg, FALSE, ts);
  
  /*Take extra ref, because add claims ownership. */
  *child_c = child_cfg;
  (*child_c)->get_ref(*child_c);

  peer_cfg->add_child_cfg(peer_cfg, child_cfg);
  
  *peer_c = peer_cfg;

}
