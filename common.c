/**
 * \file
 * \brief     Common includes, functions and variables.
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
#include "common.h"
#include "config.h"
#include "prop.h"
#include "propdb.h"
#include "repo.h"

#ifdef DAVRODS_ENABLE_PROVIDER_LOCALLOCK
#include "lock_local.h"
#endif /* DAVRODS_ENABLE_PROVIDER_LOCALLOCK */

#include <stdlib.h>

#include <http_request.h>
#include <http_protocol.h>
#include <ap_provider.h>

#include <irods/rodsClient.h>

APLOG_USE_MODULE(davrods);

// Common utility functions {{{

const char *get_rods_error_msg(int rods_error_code) {
    char *submsg = NULL;
    return rodsErrorName(rods_error_code, &submsg);
}

/**
 * \brief Extract the davrods pool from a request, as set by the rods_auth component.
 *
 * \param r an apache request record.
 *
 * \return the davrods memory pool
 */
apr_pool_t *get_davrods_pool_from_req(request_rec *r) {
    // TODO: Remove function, move apr call to the single caller.
    apr_pool_t *pool = NULL;
    int status = apr_pool_userdata_get((void**)&pool, "davrods_pool", r->connection->pool);
    assert(status == 0);
    return pool;
}

// }}}
// DAV provider definition and registration {{{

#ifdef DAVRODS_ENABLE_PROVIDER_NOLOCKS

// The no-locking provider limits the DAV protocol to version 1. This
// can cause a slight increase in performance, but may prevent certain
// clients from connecting in read/write mode (e.g. Apple OS X).
const dav_provider davrods_dav_provider_nolocks = {
    &davrods_hooks_repository,
    &davrods_hooks_propdb,
    NULL, // locks   - disabled.
    NULL, // vsn     - unimplemented.
    NULL, // binding - unimplemented.
    NULL, // search  - unimplemented.

    NULL  // context - not needed.
};

#endif /* DAVRODS_ENABLE_PROVIDER_NOLOCKS */

#ifdef DAVRODS_ENABLE_PROVIDER_LOCALLOCK

const dav_provider davrods_dav_provider_locallock = {
    &davrods_hooks_repository,
    &davrods_hooks_propdb,
    &davrods_hooks_locallock,
    NULL, // vsn     - unimplemented.
    NULL, // binding - unimplemented.
    NULL, // search  - unimplemented.

    NULL  // context - not needed.
};

#endif /* DAVRODS_ENABLE_PROVIDER_LOCALLOCK */

void davrods_dav_register(apr_pool_t *p) {

    // Register the namespace URIs.
    dav_register_liveprop_group(p, &davrods_liveprop_group);

    // Register the DAV providers.

#ifdef DAVRODS_ENABLE_PROVIDER_NOLOCKS
    dav_register_provider(p, DAVRODS_PROVIDER_NAME "-nolocks",   &davrods_dav_provider_nolocks);
#endif /* DAVRODS_ENABLE_PROVIDER_NOLOCKS */

#ifdef DAVRODS_ENABLE_PROVIDER_LOCALLOCK
    dav_register_provider(p, DAVRODS_PROVIDER_NAME "-locallock", &davrods_dav_provider_locallock);
#endif /* DAVRODS_ENABLE_PROVIDER_LOCALLOCK */

#if !defined(DAVRODS_ENABLE_PROVIDER_NOLOCKS)   \
 && !defined(DAVRODS_ENABLE_PROVIDER_LOCALLOCK)
    #error No DAV provider enabled. Please define one of the DAVRODS_ENABLE_PROVIDER_.* switches.
#endif
}
