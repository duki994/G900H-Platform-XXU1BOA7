/*
 * Copyright (C) 2013 Samsung Electronics Co. Ltd.
 * Copyright (C) 2010 Tobias Brunner
 * Hochschule fuer Technik Rapperswil
 *
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

#include <unistd.h>
#include <cutils/sockets.h>
#include <cutils/properties.h>
#include <signal.h>
#include <stdint.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>


#include "android_service.h"
#include "android_config.h"
#include "android_ikev1_utils.h"
#include "android_ikev2_utils.h"

#include <daemon.h>
#include <threading/thread.h>
#include <processing/jobs/callback_job.h>

typedef struct private_android_service_t private_android_service_t;

/**
 * private data of Android service
 */
struct private_android_service_t {

	/**
	 * public interface
	 */
	android_service_t public;

	/**
	 * current IKE_SA
	 */
	ike_sa_t *ike_sa;

	/**
	 * android credentials
	 */
	android_creds_t *creds;

	/**
	 * Socket waits tear down
	 * signal from the framework.
	 */  
        int control_socket;

	/**
	 * Our internal address
	 */
	char vip[INET6_ADDRSTRLEN];

        /**
         * DNS server address from the SGW
         */
        char dns[INET6_ADDRSTRLEN];

};

/**
 * Some of the error codes defined in VpnManager.java
 */
typedef enum {
	/** Error code to indicate an error from authentication. */
	VPN_ERROR_AUTH = 51,
	/** Error code to indicate the connection attempt failed. */
	VPN_ERROR_CONNECTION_FAILED = 101,
	/** Error code to indicate an error of remote server hanging up. */
	VPN_ERROR_REMOTE_HUNG_UP = 7,
	/** Error code to indicate an error of losing connectivity. */
	VPN_ERROR_CONNECTION_LOST = 103,
} android_vpn_errors_t;

/**
 * Callback used to shutdown the daemon
 */
static job_requeue_t shutdown_callback(void *data)
{
	kill(0, SIGTERM);
	return JOB_REQUEUE_NONE;
}

/**
 * Callback used to listen shutdown signal
 */
static job_requeue_t wait_for_teardown_callback(void *data)
{
        private_android_service_t *this = 
	  (private_android_service_t *)data;
	unsigned char c = '\0';

	/* What ever happens in the socket,
	   we interpret that as a shutdown signal. */
	read(this->control_socket, &c, sizeof(c));
	DBG3(DBG_CFG, "Teardown signal received. strongSwan going down");

	kill(0, SIGTERM);
	return JOB_REQUEUE_NONE;
}


static bool wait_for_teardown_cancel(void *data)
{
  /* Ask to be violently terminated... */
  return FALSE;
}

/**
 * send a status code back to the Android app
 */
static void send_status(private_android_service_t *this, bool success)
{
	DBG2(DBG_CFG, "status of Android plugin changed: %s", 
	     (success)?"SUCCESS":"FAILURE");

	if (success)
	{
	        //Try to get the TUN device name
		char *tun_device_name = ""; //not owned
		tun_device_t *tun_device = 
		  (tun_device_t *)lib->get(lib, "kernel-libipsec-tun");
		
		if (tun_device != NULL)
		  {
		    tun_device_name = tun_device->get_name(tun_device);
		  }


		pid_t process_id = fork();
		switch(process_id)
	  	{
	  	case -1: /* failure */
	    		DBG1(DBG_CFG, "Fork failed cannot signal Android framework");
			success = FALSE;
	    		break;
	  	case 0: /* child */
	    	{
			const char *app_path = "/etc/ppp/ip-up-vpn";
			int err = 0;
			err |= setenv("INTERFACE", tun_device_name, TRUE);
			err |= setenv("INTERNAL_ADDR4", this->vip, TRUE);
			err |= setenv("INTERNAL_CIDR4", "0", TRUE);
			err |= setenv("ROUTES", "0.0.0.0/0", TRUE); /* Routes already set */
			err |= setenv("INTERNAL_DNS4_LIST", this->dns, TRUE);
			err |= setenv("DEFAULT_DOMAIN", "", TRUE); /* Default domain not supported */
			if (err == 0)
			{
				execl(app_path, app_path, NULL);	      
				DBG1(DBG_CFG, "Failed to exec %s: %s", app_path,  strerror(errno));
			}
			exit(-1); /* This line should not be reached.*/
		}
			break;
		default: /* parent */	    
		  /* do nothing */
			break;
		}
	}

	if (!success)
	{
		  /* non-recovable exception */
		  callback_job_t *job = callback_job_create(shutdown_callback, NULL, NULL, NULL);
		  if (job != NULL) 
		  { 
		  	lib->scheduler->schedule_job(lib->scheduler, (job_t*)job, 1); 
		  }
	}
}

METHOD(listener_t, alert, bool, 
       private_android_service_t *this, ike_sa_t *ike_sa, alert_t alert, va_list args)
{
  char* alert_map[] = { 
	"ALERT_RADIUS_NOT_RESPONDING",
	"ALERT_SHUTDOWN_SIGNAL",
	"ALERT_LOCAL_AUTH_FAILED",
	"ALERT_PEER_AUTH_FAILED",
	"ALERT_PEER_ADDR_FAILED",
	"ALERT_PEER_INIT_UNREACHABLE",
	"ALERT_INVALID_IKE_SPI",
	"ALERT_PARSE_ERROR_HEADER",
	"ALERT_PARSE_ERROR_BODY",
	"ALERT_RETRANSMIT_SEND",
	"ALERT_RETRANSMIT_SEND_TIMEOUT",
	"ALERT_RETRANSMIT_RECEIVE",
	"ALERT_HALF_OPEN_TIMEOUT",
	"ALERT_PROPOSAL_MISMATCH_IKE",
	"ALERT_PROPOSAL_MISMATCH_CHILD",
	"ALERT_TS_MISMATCH",
	"ALERT_TS_NARROWED",
	"ALERT_INSTALL_CHILD_SA_FAILED",
	"ALERT_INSTALL_CHILD_POLICY_FAILED",
	"ALERT_UNIQUE_REPLACE",
	"ALERT_UNIQUE_KEEP",
	"ALERT_KEEP_ON_CHILD_SA_FAILURE",
	"ALERT_VIP_FAILURE",
	"ALERT_AUTHORIZATION_FAILED",
	"ALERT_IKE_SA_EXPIRED",
	"ALERT_CERT_EXPIRED",
	"ALERT_CERT_REVOKED",
	"ALERT_CERT_VALIDATION_FAILED",
	"ALERT_CERT_NO_ISSUER",
	"ALERT_CERT_UNTRUSTED_ROOT",
	"ALERT_CERT_EXCEEDED_PATH_LEN",
	"ALERT_CERT_POLICY_VIOLATION" 
  };


  DBG2(DBG_CFG, "received alert %s", alert_map[alert]);

  switch(alert)
    {
    case ALERT_LOCAL_AUTH_FAILED: /* falls through */
    case ALERT_PEER_AUTH_FAILED:  /* falls through */
    case ALERT_PEER_ADDR_FAILED:  /* falls through */
    case ALERT_PEER_INIT_UNREACHABLE:  /* falls through */
    case ALERT_HALF_OPEN_TIMEOUT:  /* falls through */
    case ALERT_PROPOSAL_MISMATCH_IKE: /* falls through */
    case ALERT_PROPOSAL_MISMATCH_CHILD: /* falls through */
    case ALERT_TS_MISMATCH: /* falls through */
    case ALERT_INSTALL_CHILD_SA_FAILED: /* falls through */
    case ALERT_INSTALL_CHILD_POLICY_FAILED: /* falls through */
    case ALERT_VIP_FAILURE: /* falls through */
    case ALERT_AUTHORIZATION_FAILED:
      {
	send_status(this, FALSE);
	return FALSE; /*we are not interested to get any more alarms.*/
      }
      break;
    default:
      /* do nothing */
      break;
    }

  return TRUE;
}


METHOD(listener_t, ike_state_change, bool, 
       private_android_service_t *this,  ike_sa_t *ike_sa, ike_sa_state_t state)
{
  char* ike_state_map[] = { "IKE_CREATED",
			    "IKE_CONNECTING",
			    "IKE_ESTABLISHED",
			    "IKE_PASSIVE",
			    "IKE_REKEYING",
			    "IKE_DELETING",
			    "IKE_DESTROYING" };

  DBG2(DBG_CFG, "IKE SA state changed to %s", ike_state_map[state]);

  return TRUE;
}

METHOD(listener_t, ike_updown, bool,
	   private_android_service_t *this, ike_sa_t *ike_sa, bool up)
{
	/* this callback is only registered during initiation, so if the IKE_SA
	 * goes down we assume an authentication error */
	if (this->ike_sa == ike_sa && !up)
	{
	        DBG2(DBG_CFG, "IKE SA down");
		send_status(this, FALSE);
		return FALSE;
	}
	return TRUE;
}

METHOD(listener_t, child_state_change, bool,
	   private_android_service_t *this, ike_sa_t *ike_sa, child_sa_t *child_sa,
	   child_sa_state_t state)
{
	/* this callback is only registered during initiation, so we still have
	 * the control socket open */
	if (this->ike_sa == ike_sa && state == CHILD_DESTROYING)
	{
		send_status(this, FALSE);
		return FALSE;
	}
	return TRUE;
}

METHOD(listener_t, child_updown, bool,
	   private_android_service_t *this, ike_sa_t *ike_sa, child_sa_t *child_sa,
	   bool up)
{
	if (this->ike_sa == ike_sa)
	{
		if (up)
		{
			enumerator_t* vip_enumerator = NULL;
			host_t *vip = NULL;

			memset(this->vip, '\0', sizeof(this->vip));
			
			vip_enumerator = 
			  ike_sa->create_virtual_ip_enumerator(ike_sa, TRUE);
			
			while(vip == NULL && 
			      vip_enumerator->enumerate(vip_enumerator, &vip))
			{	
				snprintf(this->vip, sizeof(this->vip), "%H", vip);
			}
			

			vip_enumerator->destroy(vip_enumerator);
			vip_enumerator = NULL;

			DBG2(DBG_CFG, "Child sa ready");
			/* disable the hooks registered to catch initiation failures */
			this->public.listener.ike_updown = NULL;
			this->public.listener.child_state_change = NULL;
			property_set("vpn.status", "ok");
			send_status(this, TRUE);
		}
		else
		{
			callback_job_t *job;
			/* daemon proxy only checks for terminated daemons to
			 * detect lost connections, so... */
			DBG2(DBG_CFG, "connection lost, raising delayed SIGTERM");
			send_status(this, FALSE);
			return FALSE;
		}
	}
	return TRUE;
}

METHOD(listener_t, ike_rekey, bool,
	   private_android_service_t *this, ike_sa_t *old, ike_sa_t *new)
{
	if (this->ike_sa == old)
	{
		this->ike_sa = new;
	}
	return TRUE;
}

int do_initiate(private_android_service_t *this)
{
  
	int socket = -1, fd = -1, i = 1, err = 0;

	/* parameters from VPN.java */
	android_config_t *configs = NULL;

	peer_cfg_t *peer_cfg = NULL;
	child_cfg_t *child_cfg = NULL;

	ike_sa_t *ike_sa = NULL;
	callback_job_t *job = NULL;

	DBG3(DBG_CFG, "Executing Android specific worker thread");

	/* Create control socket.
	   If the creation fails, we should probably initiate delayed
	   shutdown, because Android system is not going to shut us down */
	socket = android_get_control_socket("charon");
	if (socket == -1)
	{
		DBG1(DBG_CFG, "failed to get Android control socket");
		return -1;
	}

	if (listen(socket, 1) < 0)
	{
		DBG1(DBG_CFG, "failed to listen on Android control socket: %s",
			 strerror(errno));
		close(socket);
		return -1;
	}

	fd = accept(socket, NULL, 0);
	/* the original control socket is not used anymore */
	close(socket);
	socket = -1;
	if (fd < 0)
	{
		DBG1(DBG_CFG, "accept on Android control socket failed: %s",
			 strerror(errno));

		return -1;
	}

	configs = read_android_config(fd);
	if (configs == NULL)
	{
		DBG1(DBG_CFG, "Failed to read configs from control socket");
		close(fd);
		return -1;
	}

	/* We are done with reading arguments. Save the socket descriptor
	   for listening shutdown. */
	this->control_socket = fd;
	fd = -1;
      
	job = callback_job_create(wait_for_teardown_callback, this, 
				  NULL, wait_for_teardown_cancel);
	if (job != NULL) 
	{ 
		lib->scheduler->schedule_job(lib->scheduler, (job_t*)job, 1); 
	}

	if (configs->auth_method == AUTH_XAUTH_INIT_PSK ||
		configs->auth_method == AUTH_XAUTH_INIT_RSA )
	{
		this->creds->set_username_password(this->creds, 
											configs->xauth_username, 
											configs->xauth_password, 
											TRUE);
		DBG2(DBG_CFG, "XAUTH Username & password stored.");
	}
	
	if (configs->auth_method == AUTH_XAUTH_INIT_RSA ||
	    configs->auth_method == AUTH_RSA)
	{
		if (strlen(configs->rsa.private_key) > 0 &&
			!this->creds->add_private_key(this->creds, configs->rsa.private_key, configs->auth_method == AUTH_RSA))
		{
			configs->destroy(configs);
			return -1;
		}

		if (strlen(configs->rsa.user_cert) > 0)
		{
			this->creds->add_certificate(this->creds, configs->rsa.user_cert);
		}
		else 
		{
			/* User cert is mandatory */
			configs->destroy(configs);
			return -1;
		}

		if (strlen(configs->rsa.ca_cert) > 0)
		{
			this->creds->add_certificate(this->creds, configs->rsa.ca_cert);
		}

		if (strlen(configs->rsa.server_cert) > 0)
		{
			this->creds->add_certificate(this->creds, configs->rsa.server_cert);
		}	 

		if (configs->rsa.ocsp_server_url != NULL &&
			strlen(configs->rsa.ocsp_server_url) > 0)
		{
			this->creds->add_ocsp_url(this->creds, configs->rsa.ocsp_server_url);
		}
	}
	
	if (configs->auth_method == AUTH_XAUTH_INIT_PSK ||
		configs->auth_method == AUTH_PSK)
	{
		this->creds->set_username_password(this->creds, 
											configs->psk.ipsec_identifier->clone(configs->psk.ipsec_identifier), 
											configs->psk.ipsec_secret, FALSE);

	}

	switch(configs->auth_method)
	{
	case AUTH_XAUTH_INIT_PSK:
		ikev1_psk_configs_create(configs, &peer_cfg, &child_cfg);
		break;
	case AUTH_XAUTH_INIT_RSA:
		ikev1_rsa_configs_create(configs, &peer_cfg, &child_cfg);
		break;
	case AUTH_PSK:
		ikev2_psk_configs_create(configs, &peer_cfg, &child_cfg);
		break;
	case AUTH_RSA:
		ikev2_rsa_configs_create(configs, &peer_cfg, &child_cfg);
		break;
	default:
		//By this point we have checked the connection
		//type many time already. Hence, if the type is still
		//unknown value, something is seriously wrong. 
		//-->So just commit a suicide.
		DBG1(DBG_CFG, "Oops unknown connection type.");
		assert(false);
		break;
	}

	if (peer_cfg == NULL || child_cfg == NULL)
	{
		DBG1(DBG_CFG, "Failed to create configs");
		configs->destroy(configs);
		return -1;
	}
	DBG3(DBG_CFG, "Configurations created. Initiating SA");

	/* get us an IKE_SA */
	ike_sa = charon->ike_sa_manager->checkout_by_config(charon->ike_sa_manager,
							    peer_cfg);
	if (!ike_sa)
	{
		peer_cfg->destroy(peer_cfg);
		return -1;
	}

	if (!ike_sa->get_peer_cfg(ike_sa))
	{
		ike_sa->set_peer_cfg(ike_sa, peer_cfg);
	}
	peer_cfg->destroy(peer_cfg);

	/* store the IKE_SA so we can track its progress */
	this->ike_sa = ike_sa;

	/* get an additional reference because initiate consumes one */
	if (ike_sa->initiate(ike_sa, child_cfg, 0, NULL, NULL) != SUCCESS)
	{
		DBG1(DBG_CFG, "failed to initiate tunnel");
		charon->ike_sa_manager->checkin_and_destroy(charon->ike_sa_manager,
							    ike_sa);
		configs->destroy(configs);
		return -1;
	}
	charon->ike_sa_manager->checkin(charon->ike_sa_manager, ike_sa);
	configs->destroy(configs);
	return 0;
}

/**
 * handle the request received from the Android control socket
 */
static job_requeue_t initiate(private_android_service_t *this)
{
  if ( do_initiate(this) != 0 )
    {
      /* Initiation failed schedule shutdown */
      send_status(this, FALSE);
    }
  return JOB_REQUEUE_NONE;
}

METHOD(android_service_t, destroy, void,
	   private_android_service_t *this)
{
	charon->bus->remove_listener(charon->bus, &this->public.listener);
	if (this->control_socket) { close(this->control_socket); }
	free(this);
}

METHOD(android_service_t, set_dns, void,
       private_android_service_t *this, host_t *dns)
{
  snprintf(this->dns, sizeof(this->dns), "%H", dns);
  DBG1(DBG_CFG, "DNS %s saved", this->dns);
}

/**
 * See header
 */
android_service_t *android_service_create(android_creds_t *creds)
{
	private_android_service_t *this;

	INIT(this,
		.public = {
			.listener = {
				.ike_updown = _ike_updown,
				.alert = _alert,
				.ike_state_change = _ike_state_change,
				.child_state_change = _child_state_change,
				.child_updown = _child_updown,
				.ike_rekey = _ike_rekey,
			},
			.destroy = _destroy,
			.set_dns = _set_dns,
		},
		.creds = creds,
	);

	charon->bus->add_listener(charon->bus, &this->public.listener);
	lib->processor->queue_job(lib->processor,
		(job_t*)callback_job_create((callback_job_cb_t)initiate, this,
									NULL, NULL));

	memset(this->vip, '\0', sizeof(this->vip));
	memset(this->dns, '\0', sizeof(this->dns));
	return &this->public;
}

