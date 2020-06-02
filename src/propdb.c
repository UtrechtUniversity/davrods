/**
 * \file
 * \brief     Davrods property database.
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
#include "propdb.h"
#include "prop.h"
#include "repo.h"

APLOG_USE_MODULE(davrods);

struct dav_db {
    apr_pool_t *pool;
    const dav_resource *resource;
    size_t prop_iter;
};

static dav_error *dav_propdb_open(
    apr_pool_t *pool,
    const dav_resource *resource,
    int ro,
    dav_db **pdb
) {
    // Don't use pcalloc. dav will call propdb_close where we will free().
    dav_db *db = malloc(sizeof(dav_db));
    assert(db);

    db->pool = resource->pool;
    db->resource = resource;

    *pdb = db;

    return NULL;
}

static void dav_propdb_close(
    dav_db *db
) {
    free(db);
}

static dav_error *dav_propdb_define_namespaces(
    dav_db *db,
    dav_xmlns_info *xi
) {
    // TODO: Implementation, if needed.
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, db->resource->info->r,
        "Unimplemented define namespaces request"
    );
    return NULL;
}

static void dav_append_prop(
    apr_pool_t *pool,
    const char *namespace,
    const char *name,
    const char *value,
    apr_text_header *phdr
) {
    const char *s;

    if (strlen(value)) {
        s = apr_psprintf(pool, "<%s%s>%s</%s%s>\n", namespace, name, value, namespace, name);
    } else {
        s = apr_psprintf(pool, "<%s%s/>\n", namespace, name);
    }

    WHISPER("Outputting property XML: %s", s);

    apr_text_append(pool, phdr, s);
}

static dav_error *dav_propdb_output_value(
    dav_db *db,
    const dav_prop_name *name,
    dav_xmlns_info *xi,
    apr_text_header *phdr,
    int *found
) {
    WHISPER("PROP output request for prop name <%s,%s> of resource <%s>\n",
        name->ns,
        name->name,
        db->resource->info->rods_path
    );

    *found = 0;

    if (db->resource->exists) {
        if (strcmp(name->ns, "DAV:") == 0) {
            if (strcmp(name->name, "creationdate") == 0) {
                uint64_t timestamp = atoll(db->resource->info->stat->createTime);
                char date_str[APR_RFC822_DATE_LEN] = { 0 };
                int status = apr_rfc822_date(date_str, timestamp*1000*1000);
                dav_append_prop(
                    db->pool,
                    "D:", name->name,
                    status >= 0
                        ? date_str
                        : "Thu, 01 Jan 1970 00:00:00 GMT",
                    phdr
                );
                *found = 1;
            } else if (strcmp(name->name, "getcontentlength") == 0) {
                if (db->resource->collection) {
                    WHISPER("404-ing Content length request for collection\n");
                } else {
                    dav_append_prop(
                        db->pool,
                        "D:", name->name,
                        apr_psprintf(db->pool, "%" DAVRODS_SIZE_T_FMT, db->resource->info->stat->objSize),
                        phdr
                    );
                    *found = 1;
                }
            } else if (strcmp(name->name, "getetag") == 0) {
                const char *etag = davrods_hooks_repository.getetag(db->resource);
                if (etag && strlen(etag)) {
                    dav_append_prop(db->pool, "D:", name->name, etag, phdr);
                    *found = 1;
                }
            } else if (strcmp(name->name, "getlastmodified") == 0) {
                uint64_t timestamp = atoll(db->resource->info->stat->modifyTime);
                char date_str[APR_RFC822_DATE_LEN] = { 0 };
                int status = apr_rfc822_date(date_str, timestamp*1000*1000);
                dav_append_prop(
                    db->pool,
                    "D:", name->name,
                    status >= 0
                        ? date_str
                        : "Thu, 01 Jan 1970 00:00:00 GMT",
                    phdr
                );
                *found = 1;
            } else if (strcmp(name->name, "checked-in") == 0) {
            } else if (strcmp(name->name, "checked-out") == 0) {
            } else {
                WHISPER("PROP request for unknown DAV: prop <%s>!\n", name->name);
            }
        } else {
            WHISPER("404-ing Prop request for unsupported prop ns <%s>\n", name->ns);
        }
    } else {
        /* WHISPER("404-ing PROP request for non-existent resource\n"); */
        /* return NULL; */

        // Let's assume this is a LOCKNULL resource.
        // TODO: Check whether this is actually the case.

        if (strcmp(name->ns, "DAV:") == 0) {
            if (strcmp(name->name, "creationdate") == 0) {
                dav_append_prop(
                    db->pool,
                    "D:", name->name,
                    "Thu, 01 Jan 1970 00:00:00 GMT",
                    phdr
                );
                *found = 1;
            } else if (strcmp(name->name, "getcontentlength") == 0) {
                dav_append_prop(
                    db->pool,
                    "D:", name->name,
                    apr_psprintf(db->pool, "%" DAVRODS_SIZE_T_FMT, 0),
                    phdr
                );
                *found = 1;
            } else if (strcmp(name->name, "getetag") == 0) {
                const char *etag = davrods_hooks_repository.getetag(db->resource);
                if (etag && strlen(etag)) {
                    dav_append_prop(db->pool, "D:", name->name, etag, phdr);
                    *found = 1;
                }
            } else if (strcmp(name->name, "getlastmodified") == 0) {
                dav_append_prop(
                    db->pool,
                    "D:", name->name,
                    "Thu, 01 Jan 1970 00:00:00 GMT",
                    phdr
                );
                *found = 1;
            } else if (strcmp(name->name, "checked-in") == 0) {
            } else if (strcmp(name->name, "checked-out") == 0) {
            }
        } else {
            return NULL;
        }
    }

    return NULL;
}

static dav_error *dav_propdb_map_namespaces(
    dav_db *db,
    const apr_array_header_t *namespaces,
    dav_namespace_map **mapping
) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, db->resource->info->r,
        "Unimplemented map namespaces request"
    );

    int *pmap;
    const char **puri;
    int i;
    for (i = namespaces->nelts, puri = (const char **)namespaces->elts;
         i-- > 0;
         ++puri, ++pmap) {
        WHISPER("- URI <%s>\n", *puri);

        // TODO: Implementation, if needed.
    }

    return NULL;
}

static dav_error *dav_propdb_store(
    dav_db *db,
    const dav_prop_name *name,
    const apr_xml_elem *elem,
    dav_namespace_map *mapping
) {
    return dav_new_error(
        db->pool, HTTP_METHOD_NOT_ALLOWED, 0, 0,
        "Property manipulation is not supported by this server."
    );
}

static dav_error *dav_propdb_remove(
    dav_db *db,
    const dav_prop_name *name
) {
    return dav_new_error(
        db->pool, HTTP_METHOD_NOT_ALLOWED, 0, 0,
        "Property manipulation is not supported by this server."
    );
}

static int dav_propdb_exists(
    dav_db *db,
    const dav_prop_name *name
) {
    WHISPER("Prop exists <%s%s>? No.\n", name->ns, name->name);

    // TODO: Implementation, if necessary.

    return 0; // 0 = does not exist.
}

static dav_error *dav_propdb_next_name(
    dav_db *db,
    dav_prop_name *pname
) {
    //if (db->resource->exists && db->prop_iter < DAVRODS_PROP_COUNT) {
    // Don't care if a resource does not exist: It's probably a LOCKNULL resource.

    if (db->prop_iter < DAVRODS_PROP_COUNT) {
        if (
               davrods_props[db->prop_iter].propid == DAV_PROPID_getcontentlength
            && db->resource->collection
        ) {
            // This property is not available for collections, skip it.
            db->prop_iter++;
            return dav_propdb_next_name(db, pname);
        } else {
            pname->ns   = davrods_namespace_uris[davrods_props[db->prop_iter].ns];
            pname->name = davrods_props[db->prop_iter].name;
            db->prop_iter++;
        }
    } else {
        // This signifies the end of the property list.
        pname->ns = pname->name = NULL;
    }

    return NULL;
}

static dav_error *dav_propdb_first_name(
    dav_db *db,
    dav_prop_name *pname
) {
    db->prop_iter = 0;
    return dav_propdb_next_name(db, pname);
}

static dav_error *dav_propdb_get_rollback(
    dav_db *db,
    const dav_prop_name *name,
    dav_deadprop_rollback **prollback
) {
    return dav_new_error(
        db->pool, HTTP_METHOD_NOT_ALLOWED, 0, 0,
        "Property manipulation is not supported by this server."
    );
}

static dav_error * dav_propdb_apply_rollback(
    dav_db *db,
    dav_deadprop_rollback *rollback
) {
    return dav_new_error(
        db->pool, HTTP_METHOD_NOT_ALLOWED, 0, 0,
        "Property manipulation is not supported by this server."
    );
}

const dav_hooks_db davrods_hooks_propdb = {
    dav_propdb_open,
    dav_propdb_close,
    dav_propdb_define_namespaces,
    dav_propdb_output_value,
    dav_propdb_map_namespaces,
    dav_propdb_store,
    dav_propdb_remove,
    dav_propdb_exists,
    dav_propdb_first_name,
    dav_propdb_next_name,
    dav_propdb_get_rollback,
    dav_propdb_apply_rollback,

    NULL // An optional private context pointer. We don't need it.
};
