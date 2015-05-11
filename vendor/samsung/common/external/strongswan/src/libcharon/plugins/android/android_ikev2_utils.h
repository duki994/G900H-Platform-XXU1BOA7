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
 * @defgroup android_ikev2_utils android_ikev2_utils
 * @{ @ingroup android
 */

#ifndef ANDROID_IKEV2_UTILS_H_
#define ANDROID_IKEV2_UTILS_H_

#include <config/peer_cfg.h>
#include <config/child_cfg.h>

#include "android_config.h"

void ikev2_psk_configs_create(android_config_t *configs,
			      peer_cfg_t **peer_c,
			      child_cfg_t **child_c);

void ikev2_rsa_configs_create(android_config_t *configs,
			      peer_cfg_t **peer_c,
			      child_cfg_t **child_c);

#endif //ANDROID_IKEV2_UTILS_H_
