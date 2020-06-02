/**
 * \file
 * \brief     Davrods DAV repository.
 * \author    Chris Smeele
 * \copyright Copyright (c) 2016-2020, Utrecht University
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
#ifndef _DAVRODS_REPO_H
#define _DAVRODS_REPO_H

#include "common.h"
#include "config.h"

#include <irods/rodsClient.h>
#include <irods/rods.h>

/**
 * \brief Private implementation-specific resource information.
 *
 * This is property `info` of the dav_resource struct.
 *
 * We mostly use it to access iRODS connection state and iRODS specific (dav)
 * resource information.
 */
struct dav_resource_private {
    // Information specific to the HTTP request {{{

    request_rec        *r;
    apr_pool_t         *davrods_pool;
    davrods_dir_conf_t *conf;
    rcComm_t           *rods_conn;
    rodsEnv            *rods_env;
    const char         *rods_root;

    // }}}
    // Information specific to the DAV resource {{{

    // This is `MAX_NAME_LEN` as specified by iRODS.
    char rods_path[MAX_NAME_LEN]; // Currently 1024 + 64.

    // relative_uri is resource->uri with the root_dir chopped off.
    // i.e. with a Davrods in <Location /abc/def/>,
    // resource->uri may be /abc/def/some_file.txt,
    // while relative_uri will be just /some_file.txt.
    const char *relative_uri;

    rodsObjStat_t *stat;
    const char *root_dir;

    // }}}
};

extern const dav_hooks_repository davrods_hooks_repository;

const char *davrods_get_basename(const char *path);

#endif /* _DAVRODS_REPO_H */
