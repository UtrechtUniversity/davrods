/**
 * \file
 * \brief     HTTP Basic auth provider for iRODS.
 * \author    Chris Smeele
 * \copyright Copyright (c) 2016, Utrecht University
 *
 * This file is part of Davrods.
 *
 * Davrods is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Davrods is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Davrods.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _DAVRODS_AUTH_H
#define _DAVRODS_AUTH_H

#include "mod_davrods.h"
#include <mod_auth.h>

authn_status check_rods(request_rec *r,
                        const char *username,
                        const char *password,
                        bool is_basic_auth);

bool davrods_user_can_reuse_connection(request_rec *r,
                                       const char *username,
                                       const char *password);

void davrods_auth_register(apr_pool_t *p);

#endif /* _DAVRODS_AUTH_H */
