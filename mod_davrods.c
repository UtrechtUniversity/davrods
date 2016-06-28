/**
 * \file
 * \brief     Davrods main module file.
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
#include "mod_davrods.h"
#include "config.h"
#include "auth.h"
#include "common.h"

APLOG_USE_MODULE(davrods);

static void register_hooks(apr_pool_t *p) {
    davrods_auth_register(p);
    davrods_dav_register(p);
}

module AP_MODULE_DECLARE_DATA davrods_module = {
    STANDARD20_MODULE_STUFF,
    davrods_create_dir_config, // Directory config setup.
    davrods_merge_dir_config,  //    ..       ..   merge function.
    NULL,                      // Server config setup.
    NULL,                      //   ..     ..   merge function.
    davrods_directives,        // Command table.
    register_hooks,            // Hook setup.
};
