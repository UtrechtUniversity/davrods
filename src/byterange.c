/**
 * \file
 * \brief     Davrods GET+Range request support.
 * \author    Chris Smeele
 * \copyright Copyright (c) 2018, Utrecht University
 *
 * Copyright (c) 2018, Utrecht University
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   Apache HTTP Server
 *   Copyright 2013 The Apache Software Foundation.
 *
 *   This product includes software developed at
 *   The Apache Software Foundation (http://www.apache.org/).
 *
 *   Portions of this software were developed at the National Center
 *   for Supercomputing Applications (NCSA) at the University of
 *   Illinois at Urbana-Champaign.
 *
 *   This software contains code derived from the RSA Data Security
 *   Inc. MD5 Message-Digest Algorithm, including various
 *   modifications by Spyglass Inc., Carnegie Mellon University, and
 *   Bell Communications Research, Inc (Bellcore).
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements.  See the NOTICE file distributed with
 *   this work for additional information regarding copyright ownership.
 *   The ASF licenses this file to You under the Apache License, Version 2.0
 *   (the "License"); you may not use this file except in compliance with
 *   the License.  You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/* This code is based on:
 * - Apache's byterange_filter.c: For supporting the HTTP Range header.
 * - Earlier Davrods' repo.c: For reading and sending iRODS file contents.
 */

#include "apr.h"

#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_lib.h"
#include "apr_signal.h"

#define APR_WANT_STDIO          /* for sscanf */
#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "apr_want.h"

#include "util_filter.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_request.h"
#include "http_vhost.h"
#include "http_log.h"           /* For errors detected in basic auth common
                                 * support code... */
#include "apr_date.h"           /* For apr_date_parse_http and APR_DATE_BAD */
#include "util_charset.h"
#include "util_ebcdic.h"
#include "util_time.h"

#include "mod_core.h"

#if APR_HAVE_STDARG_H
#include <stdarg.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "byterange.h"
#include "repo.h"

#ifndef AP_DEFAULT_MAX_RANGES
#define AP_DEFAULT_MAX_RANGES 200
#endif
#ifndef AP_DEFAULT_MAX_OVERLAPS
#define AP_DEFAULT_MAX_OVERLAPS 20
#endif
#ifndef AP_DEFAULT_MAX_REVERSALS
#define AP_DEFAULT_MAX_REVERSALS 20
#endif

#define MAX_PREALLOC_RANGES 100

#ifdef APLOG_USE_MODULE
APLOG_USE_MODULE(davrods);
#endif

typedef struct indexes_t {
    apr_off_t start;
    apr_off_t end;
} indexes_t;

/**
 * \brief Parse a range request, joining ranges where possible.
 *
 * This is unmodified from Apache's byterange filter code.
 *
 * \return number of ranges (merged) or -1 for no-good
 */
static int ap_set_byterange(request_rec *r, apr_off_t clength,
                            apr_array_header_t **indexes,
                            int *overlaps, int *reversals)
{
    const char *range;
    const char *ct;
    char *cur;
    apr_array_header_t *merged;
    int num_ranges = 0, unsatisfiable = 0;
    apr_off_t ostart = 0, oend = 0, sum_lengths = 0;
    int in_merge = 0;
    indexes_t *idx;
    int ranges = 1;
    int i;
    const char *it;

    *overlaps = 0;
    *reversals = 0;

    if (r->assbackwards) {
        return 0;
    }

    /*
     * Check for Range request-header (HTTP/1.1) or Request-Range for
     * backwards-compatibility with second-draft Luotonen/Franks
     * byte-ranges (e.g. Netscape Navigator 2-3).
     *
     * We support this form, with Request-Range, and (farther down) we
     * send multipart/x-byteranges instead of multipart/byteranges for
     * Request-Range based requests to work around a bug in Netscape
     * Navigator 2-3 and MSIE 3.
     */

    if (!(range = apr_table_get(r->headers_in, "Range"))) {
        range = apr_table_get(r->headers_in, "Request-Range");
    }

    if (!range || strncasecmp(range, "bytes=", 6) || r->status != HTTP_OK) {
        return 0;
    }

    /* is content already a single range? */
    if (apr_table_get(r->headers_out, "Content-Range")) {
        return 0;
    }

    /* is content already a multiple range? */
    if ((ct = apr_table_get(r->headers_out, "Content-Type"))
        && (!strncasecmp(ct, "multipart/byteranges", 20)
            || !strncasecmp(ct, "multipart/x-byteranges", 22))) {
            return 0;
        }

    /*
     * Check the If-Range header for Etag or Date.
     */
    if (AP_CONDITION_NOMATCH == ap_condition_if_range(r, r->headers_out)) {
        return 0;
    }

    range += 6;
    it = range;
    while (*it) {
        if (*it++ == ',') {
            ranges++;
        }
    }
    it = range;
    if (ranges > MAX_PREALLOC_RANGES) {
        ranges = MAX_PREALLOC_RANGES;
    }
    *indexes = apr_array_make(r->pool, ranges, sizeof(indexes_t));
    while ((cur = ap_getword(r->pool, &range, ','))) {
        char *dash;
        char *errp;
        apr_off_t number, start, end;

        if (!*cur)
            break;

        /*
         * Per RFC 2616 14.35.1: If there is at least one syntactically invalid
         * byte-range-spec, we must ignore the whole header.
         */

        if (!(dash = strchr(cur, '-'))) {
            return 0;
        }

        if (dash == cur) {
            /* In the form "-5" */
            if (apr_strtoff(&number, dash+1, &errp, 10) || *errp) {
                return 0;
            }
            if (number < 1) {
                return 0;
            }
            start = clength - number;
            end = clength - 1;
        }
        else {
            *dash++ = '\0';
            if (apr_strtoff(&number, cur, &errp, 10) || *errp) {
                return 0;
            }
            start = number;
            if (*dash) {
                if (apr_strtoff(&number, dash, &errp, 10) || *errp) {
                    return 0;
                }
                end = number;
                if (start > end) {
                    return 0;
                }
            }
            else {                  /* "5-" */
                end = clength - 1;
                /*
                 * special case: 0-
                 *   ignore all other ranges provided
                 *   return as a single range: 0-
                 */
                if (start == 0) {
                    num_ranges = 0;
                    sum_lengths = 0;
                    in_merge = 1;
                    oend = end;
                    ostart = start;
                    apr_array_clear(*indexes);
                    break;
                }
            }
        }

        if (start < 0) {
            start = 0;
        }
        if (start >= clength) {
            unsatisfiable = 1;
            continue;
        }
        if (end >= clength) {
            end = clength - 1;
        }

        if (!in_merge) {
            /* new set */
            ostart = start;
            oend = end;
            in_merge = 1;
            continue;
        }
        in_merge = 0;

        if (start >= ostart && end <= oend) {
            in_merge = 1;
        }

        if (start < ostart && end >= ostart-1) {
            ostart = start;
            ++*reversals;
            in_merge = 1;
        }
        if (end >= oend && start <= oend+1 ) {
            oend = end;
            in_merge = 1;
        }

        if (in_merge) {
            ++*overlaps;
            continue;
        } else {
            idx = (indexes_t *)apr_array_push(*indexes);
            idx->start = ostart;
            idx->end = oend;
            sum_lengths += oend - ostart + 1;
            /* new set again */
            in_merge = 1;
            ostart = start;
            oend = end;
            num_ranges++;
        }
    }

    if (in_merge) {
        idx = (indexes_t *)apr_array_push(*indexes);
        idx->start = ostart;
        idx->end = oend;
        sum_lengths += oend - ostart + 1;
        num_ranges++;
    }
    else if (num_ranges == 0 && unsatisfiable) {
        /* If all ranges are unsatisfiable, we should return 416 */
        return -1;
    }
    if (sum_lengths > clength) {
        ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r,
                      "Sum of ranges larger than file, ignoring.");
        return 0;
    }

    /*
     * create the merged table now, now that we know we need it
     */
    merged = apr_array_make(r->pool, num_ranges, sizeof(char *));
    idx = (indexes_t *)(*indexes)->elts;
    for (i = 0; i < (*indexes)->nelts; i++, idx++) {
        char **new = (char **)apr_array_push(merged);
        *new = apr_psprintf(r->pool, "%" APR_OFF_T_FMT "-%" APR_OFF_T_FMT,
                            idx->start, idx->end);
    }

    r->status = HTTP_PARTIAL_CONTENT;
    r->range = apr_array_pstrcat(r->pool, merged, ',');
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01583)
                  "Range: %s | %s (%d : %d : %"APR_OFF_T_FMT")",
                  it, r->range, *overlaps, *reversals, clength);

    return num_ranges;
}

/*
 * Here we try to be compatible with clients that want multipart/x-byteranges
 * instead of multipart/byteranges (also see above), as per HTTP/1.1. We
 * look for the Request-Range header (e.g. Netscape 2 and 3) as an indication
 * that the browser supports an older protocol. We also check User-Agent
 * for Microsoft Internet Explorer 3, which needs this as well.
 */
static int use_range_x(request_rec *r)
{
    const char *ua;
    return (apr_table_get(r->headers_in, "Request-Range")
            || ((ua = apr_table_get(r->headers_in, "User-Agent"))
                && ap_strstr_c(ua, "MSIE 3")));
}

#define BYTERANGE_FMT "%" APR_OFF_T_FMT "-%" APR_OFF_T_FMT "/%" APR_OFF_T_FMT

/**
 * \brief Send a 416 Range not Satisfiable status code.
 *
 * This was modified from Apache's original code to
 * remove filter-specific behavior.
 */
static apr_status_t send_416(
    const dav_resource *resource,
    ap_filter_t        *output,
    apr_bucket_brigade *bb
) {
    apr_bucket *e;
    conn_rec *c = resource->info->r->connection;
    resource->info->r->status = HTTP_OK;
    e = ap_bucket_error_create(HTTP_RANGE_NOT_SATISFIABLE, NULL,
                               resource->info->r->pool, c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, e);
    e = apr_bucket_eos_create(c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, e);
    return ap_pass_brigade(output, bb);
}

/**
 * \brief Seek in an iRODS object.
 */
static dav_error *deliver_seek(
    const dav_resource *resource,
    openedDataObjInp_t *data_obj,
    size_t pos
) {
    openedDataObjInp_t seek_inp = { 0 };
    seek_inp.l1descInx = data_obj->l1descInx;
    seek_inp.offset    = pos;
    seek_inp.whence    = SEEK_SET;

    fileLseekOut_t *seek_out = NULL;
    int status = rcDataObjLseek(resource->info->rods_conn, &seek_inp, &seek_out);

    if (seek_out)
        free(seek_out);

    if (status < 0) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, resource->info->r,
                      "rcDataObjLseek failed: %d = %s", status, get_rods_error_msg(status));
        return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
                             "Could not seek file for range request");
    } else {
        return NULL;
    }
}

/**
 * \brief Read data object bytes from iRODS, send them to the client.
 */
static dav_error *deliver_file_bytes(
    const dav_resource *resource,
    openedDataObjInp_t *data_obj,
    ap_filter_t        *output,
    apr_bucket_brigade *bb,
    size_t              seek_pos,
    size_t              bytes_to_read,
    size_t             *total_read
) {
    apr_pool_t *pool = resource->pool;
    bytesBuf_t read_buffer = { 0 };

    *total_read = 0;

    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, resource->info->r,
                  "Reading data object in %luK chunks",
                  resource->info->conf->rods_rx_buffer_size / 1024);

    // NB:
    // ap_set_byterange joins and truncates requested ranges when necessary,
    // and filters invalid ranges.
    // For this reason we can assume that any error occuring during a seek is
    // not an issue with the original range request, but an issue with the
    // iRODS object or the iRODS connection instead.
    // So errors here will result in a 500, not a 416.
    dav_error *err = deliver_seek(resource, data_obj, seek_pos);
    if (err)
        return err;

    while (*total_read < bytes_to_read) {
        // Have a buffer size of at most rods_rx_buffer_size bytes.
        size_t buffer_size = MIN(bytes_to_read - *total_read,
                                 resource->info->conf->rods_rx_buffer_size);
        // Request to read that amount of bytes.
        data_obj->len = buffer_size;

        int bytes_read = 0;

        // Read the data object.
        bytes_read = rcDataObjRead(resource->info->rods_conn, data_obj, &read_buffer);

        if (bytes_read < 0) {
            if (read_buffer.buf)
                free(read_buffer.buf);

            ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, resource->info->r,
                          "rcDataObjRead failed: %d = %s",
                          bytes_read, get_rods_error_msg(bytes_read));

            return dav_new_error(pool, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
                                 "Could not read from requested resource");
        }

        *total_read += bytes_read;

        if (bytes_read == 0) {
            // No errors, but nothing to read either, EOF.
            free(read_buffer.buf);
            read_buffer.buf = NULL;
            return NULL;
        }

        apr_brigade_write(bb, NULL, NULL, read_buffer.buf, bytes_read);

        free(read_buffer.buf);
        read_buffer.buf = NULL;

        // Flush our output after each buffer_size bytes.
        int status;
        if ((status = ap_pass_brigade(output, bb)) != APR_SUCCESS) {
            return dav_new_error(pool, HTTP_INTERNAL_SERVER_ERROR, 0, status,
                                 "Could not write contents to filter.");
        }
    }

    return NULL;
}

/**
 * \brief Process a GET request with an optional Range header.
 *
 * This is based on ap_byterange_filter.
 * Changes mostly come down to having to deal with an iRODS object
 * as the input instead of a bucket brigade (Davrods is not a filter).
 */
dav_error *davrods_byterange_deliver_file(const dav_resource *resource,
                                          openedDataObjInp_t *data_obj,
                                          ap_filter_t        *output,
                                          apr_bucket_brigade *bb) {

    conn_rec    *c = resource->info->r->connection;
    request_rec *r = resource->info->r;

    // Set up Range limits.

    core_dir_config *core_conf = ap_get_core_module_config(resource->info->r->per_dir_config);

    int max_ranges    = ((core_conf->max_ranges    >= 0 || core_conf->max_ranges    == AP_MAXRANGES_UNLIMITED)
                         ? core_conf->max_ranges    : AP_DEFAULT_MAX_RANGES);
    int max_overlaps  = ((core_conf->max_overlaps  >= 0 || core_conf->max_overlaps  == AP_MAXRANGES_UNLIMITED)
                         ? core_conf->max_overlaps  : AP_DEFAULT_MAX_OVERLAPS );
    int max_reversals = ((core_conf->max_reversals >= 0 || core_conf->max_reversals == AP_MAXRANGES_UNLIMITED)
                         ? core_conf->max_reversals : AP_DEFAULT_MAX_REVERSALS);

    size_t obj_length = resource->info->stat->objSize;

    // Parse a Range header, if it exists.
    apr_array_header_t *indexes;
    int overlaps   = 0;
    int reversals  = 0;
    int num_ranges = ap_set_byterange(resource->info->r,
                                      obj_length,
                                      &indexes,
                                      &overlaps,
                                      &reversals);

    if (num_ranges) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, resource->info->r,
                      "Ranges: %d, overlaps: %d, reversals: %d",
                      num_ranges, overlaps, reversals);
    }

    if (num_ranges == 0
        || (max_ranges    >= 0 && num_ranges > max_ranges)
        || (max_overlaps  >= 0 && overlaps   > max_overlaps)
        || (max_reversals >= 0 && reversals  > max_reversals)) {

        // No Range header or we hit a limit?
        // Deliver the entire file.

        size_t total_read = 0;
        dav_error *err = deliver_file_bytes(resource,
                                            data_obj,
                                            output,
                                            bb,
                                            0,
                                            obj_length,
                                            &total_read);
        return err;

    } else if (num_ranges < 0) {

        // All ranges are unsatisfiable.

        send_416(resource, output, bb);
        return NULL;
    }

    // This is a range request. Deliver each range.

    apr_table_unset(r->headers_out, "Content-Length");

    char *bound_head = NULL;

    if (num_ranges > 1) {
        // Output in multipart format and generate multipart boundaries.
        ap_set_content_type(r,
                            apr_pstrcat(r->pool, "multipart",
                                        use_range_x(r) ? "/x-" : "/",
                                        "byteranges; boundary=",
                                        ap_multipart_boundary, NULL));
        bound_head = apr_pstrcat(r->pool,
                                 CRLF "--", ap_multipart_boundary,
                                 CRLF "Content-range: bytes ",
                                 NULL);
        ap_xlate_proto_to_ascii(bound_head, strlen(bound_head));
    }

    indexes_t *idx = (indexes_t*)indexes->elts;

    // For each range...
    for (int i = 0; i < indexes->nelts; i++, idx++) {
        apr_off_t range_start = idx->start;
        apr_off_t range_end   = idx->end;

        // For single range requests, we must produce Content-Range header.
        // Otherwise, we need to produce the multipart boundaries.
        if (num_ranges == 1) {
            apr_table_setn(r->headers_out, "Content-Range",
                           apr_psprintf(r->pool, "bytes " BYTERANGE_FMT,
                                        range_start, range_end, obj_length));
        } else {
            char *ts;

            apr_bucket *e = apr_bucket_pool_create(bound_head, strlen(bound_head),
                                                   r->pool, c->bucket_alloc);
            APR_BRIGADE_INSERT_TAIL(bb, e);

            ts = apr_psprintf(r->pool, BYTERANGE_FMT CRLF CRLF,
                              range_start, range_end, obj_length);
            ap_xlate_proto_to_ascii(ts, strlen(ts));
            e = apr_bucket_pool_create(ts, strlen(ts), r->pool,
                                       c->bucket_alloc);
            APR_BRIGADE_INSERT_TAIL(bb, e);
        }

        // Now output the content for that range.
        size_t total_read = 0;
        dav_error *err = deliver_file_bytes(resource,
                                            data_obj,
                                            output,
                                            bb,
                                            range_start,
                                            range_end - range_start + 1,
                                            &total_read);
        if (err)
            return err;
    }

    if (num_ranges > 1) {
        char *end;

        // Add the final boundary.
        end = apr_pstrcat(r->pool, CRLF "--", ap_multipart_boundary, "--" CRLF, NULL);
        ap_xlate_proto_to_ascii(end, strlen(end));
        apr_bucket *e = apr_bucket_pool_create(end, strlen(end), r->pool, c->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(bb, e);
    }

    return NULL;
}
