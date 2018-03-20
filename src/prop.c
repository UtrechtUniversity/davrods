/**
 * \file
 * \brief     Davrods DAV property support.
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
#include "prop.h"

APLOG_USE_MODULE(davrods);

static dav_prop_insert prop_insert_prop(
    const dav_resource *resource,
   int propid,
   dav_prop_insert what,
   apr_text_header *phdr
) {
    return DAV_PROP_INSERT_NOTSUPP;
}

static int prop_is_writable(const dav_resource *resource, int propid) {
    // We have no writable properties.

    return 0;
}

const char *const davrods_namespace_uris[] = {
    "DAV:",
    NULL // Sentinel.
};
enum {
    DAVRODS_URI_DAV = 0, // The DAV: namespace URI.
};

static dav_error *prop_patch_validate(
    const dav_resource *resource,
    const apr_xml_elem *elem,
    int operation,
    void **context,
    int *defer_to_dead
) {
    return dav_new_error(
        resource->pool, HTTP_METHOD_NOT_ALLOWED, 0, 0,
        "Property manipulation is not supported by this server."
    );
}

static dav_error *prop_patch_exec(
    const dav_resource *resource,
    const apr_xml_elem *elem,
    int operation,
    void *context,
    dav_liveprop_rollback **rollback_ctx
) {
    return dav_new_error(
        resource->pool, HTTP_METHOD_NOT_ALLOWED, 0, 0,
        "Property manipulation is not supported by this server."
    );
}

static void prop_patch_commit(
    const dav_resource *resource,
    int operation,
    void *context,
    dav_liveprop_rollback *rollback_ctx
) {
}

static dav_error *prop_patch_rollback(
    const dav_resource *resource,
    int operation,
    void *context,
    dav_liveprop_rollback *rollback_ctx
) {
    return dav_new_error(
        resource->pool, HTTP_METHOD_NOT_ALLOWED, 0, 0,
        "Property manipulation is not supported by this server."
    );
}

const dav_hooks_liveprop davrods_hooks_liveprop = {
    prop_insert_prop,
    prop_is_writable,
    davrods_namespace_uris,
    prop_patch_validate,
    prop_patch_exec,
    prop_patch_commit,
    prop_patch_rollback
};

const dav_liveprop_spec davrods_props[] = {
    // Standard DAV properties.
    {
        DAVRODS_URI_DAV,
        "creationdate",
        DAV_PROPID_creationdate,
        0
    },
    {
        DAVRODS_URI_DAV,
        "getcontentlength",
        DAV_PROPID_getcontentlength,
        0
    },
    {
        DAVRODS_URI_DAV,
        "getetag",
        DAV_PROPID_getetag,
        0
    },
    {
        DAVRODS_URI_DAV,
        "getlastmodified",
        DAV_PROPID_getlastmodified,
        0
    },

    { 0 } // Sentinel.
};

//#define DAVRODS_PROP_COUNT (sizeof(davrods_props) / sizeof(dav_liveprop_spec) - 1)
const size_t DAVRODS_PROP_COUNT = sizeof(davrods_props) / sizeof(dav_liveprop_spec) - 1;

const dav_liveprop_group davrods_liveprop_group = {
    davrods_props,
    davrods_namespace_uris,
    &davrods_hooks_liveprop
};
