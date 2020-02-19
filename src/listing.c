/**
 * \file
 * \brief     Davrods HTML listing support.
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
#include "listing.h"
#include "repo.h"

/**
 * \brief Encode a path such that it can be safely used in a URI.
 *
 * Used within HTML directory listings.
 * If the input path is safe, no new string is allocated.
 *
 * \param pool A memory pool
 * \param path The path to escape
 *
 * \return An escaped path (may be the same as the input pointer)
 */
static const char *escape_uri_path(
    apr_pool_t *pool,
    const char *path
) {
    // Apache's ap_escape_uri is not sufficient, as it is OS-dependent(!?) and
    // does not encode certain reserved characters that can be problematic in
    // relative URLs.
    //
    // Given the lack of an Apache function that does what we need, we do URL
    // encoding ourselves, as per RFC 1808:
    // https://tools.ietf.org/html/rfc1808 (page 4)
    //
    // We encode everything outside of the 'unreserved' character class, except for '/'.
    // That is, every char not in [a-zA-Z0-9$_.+!*'(),/-].

    static const char escape_table[256] = {
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,0,1,1,0,1,1,0,0,0,0,0,0,0,0,0, //  !"#$%&'()*+,-./
        0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1, // 0123456789:;<=>?
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // @ABCDEFGHIJKLMNO
        0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0, // PQRSTUVWXYZ[\]^_
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // `abcdefghijklmno
        0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1, // pqrstuvwxyz{|}~
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    };

    size_t length_orig    = strlen(path);
    size_t reserved_count = 0;

    for (size_t i = 0; i < length_orig; ++i) {
        if (escape_table[(unsigned char)path[i]])
            ++reserved_count;
    }

    if (!reserved_count)
        return path; // Nothing to escape.

    // Each reserved char will take up 2 extra characters ('&' => '%26').
    size_t length_new = length_orig + reserved_count*2;

    char *new_path = apr_pcalloc(pool, length_new + 1);
    assert(new_path);
    for (size_t i = 0, j = 0;
         i < length_orig && j < length_new;
         ++i) {

        if (escape_table[(unsigned char)path[i]]) {
            sprintf(new_path + j, "%%%.2X", (unsigned char)path[i]);
            j += 3;
        } else {
            new_path[j++] = path[i];
        }
    }

    return new_path;
}

/**
 * \brief Within a HTML directory listing, insert the contents of a local file.
 *
 * \param resource Provides context, pool etc.
 * \param bb       The bucket brigade
 * \param path     The local file path to read (must be accessible by httpd)
 *
 * \return APR_SUCCESS on success, an APR_* error code otherwise
 */
static apr_status_t deliver_directory_try_insert_local_file(
    const dav_resource *resource,
    apr_bucket_brigade *bb,
    const char *path
) {
    if (!strlen(path))
        return APR_SUCCESS;

    apr_file_t *f;
    apr_status_t status = apr_file_open(&f, path, APR_FOPEN_READ, 0, resource->pool);

    if (status == APR_SUCCESS) {
        apr_finfo_t info;
        status = apr_file_info_get(&info, APR_FINFO_SIZE, f);

        if (status == APR_SUCCESS) {
            apr_size_t chunk_size = AP_IOBUFSIZE;
            apr_size_t read_total = 0;

            char *buf = malloc(chunk_size);
            assert(buf);

            // Read the file in chunks and write the contents to the brigade.
            while (read_total < (apr_size_t)info.size) {
                if (info.size - read_total < chunk_size)
                    chunk_size = info.size - read_total;

                apr_size_t read_count = 0;
                status = apr_file_read_full(f, buf, chunk_size, &read_count);

                if (read_count == chunk_size && status == APR_SUCCESS) {
                    apr_brigade_write(bb, NULL, NULL, buf, read_count);
                    read_total += read_count;
                } else {
                    break;
                }
            }

            free(buf);

            if (read_total != (apr_size_t)info.size || status != APR_SUCCESS) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, status, resource->info->r,
                              "Could not read file <%s>", path);
            }
        } else {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, status, resource->info->r,
                          "Could not stat file <%s>", path);
        }
        apr_file_close(f);

    } else {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, status, resource->info->r,
                      "Could not open file <%s> for reading", path);
    }

    return status;
}

dav_error *davrods_deliver_directory_listing(
    const dav_resource *resource,
    ap_filter_t *output
) {
    // Print a basic HTML directory listing.

    // Note: Headers for this response are set in repo.c dav_repo_set_headers

    collInp_t coll_inp = {{ 0 }};
    strcpy(coll_inp.collName, resource->info->rods_path);

    collHandle_t coll_handle = { 0 };

    // Open the collection.
    collEnt_t    coll_entry;
    int status = rclOpenCollection(
        resource->info->rods_conn,
        resource->info->rods_path,
        LONG_METADATA_FG,
        &coll_handle
    );

    if (status < 0) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, resource->info->r,
                      "rcOpenCollection failed: %d = %s", status, get_rods_error_msg(status));

        return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0, status,
                             "Could not open a collection");
    }

    // Make brigade.
    apr_pool_t         *pool = resource->pool;
    apr_bucket_brigade *bb = apr_brigade_create(pool, output->c->bucket_alloc);
    apr_bucket         *bkt;

    // Collection URIs must end with a slash to make relative links work.
    // Normally, web servers redirect clients to `path + '/'` if it's missing,
    // but mod_dav does not expect us to return a redirect status code (it
    // works, but results in mod_dav error messages).
    //
    // As an alternative solution, we supply a HTML <base> tag containing the
    // correct collection path (with appended '/' if necessary). This is then
    // used by the browser as a base URI for all relative links.
    //
    bool uri_ends_with_slash = false;
    { size_t uri_length = strlen(resource->info->relative_uri);
      if (uri_length && resource->info->relative_uri[uri_length-1] == '/')
          uri_ends_with_slash = true; }

    char *root_dir_without_trailing_slash = apr_pstrdup(pool, resource->info->root_dir);
    { size_t len = strlen(root_dir_without_trailing_slash);
      if (len && root_dir_without_trailing_slash[len-1] == '/')
          root_dir_without_trailing_slash[len-1] = '\0'; }

    // Send start of HTML document.
    apr_brigade_printf(bb, NULL, NULL,
                       "<!DOCTYPE html>\n<html>\n<head>\n"
                       "<title>Index of %s%s on %s</title>\n"
                       "<base href=\"%s%s%s\">\n",
                       ap_escape_html(pool, resource->info->relative_uri),
                       uri_ends_with_slash ? "" : "/",
                       ap_escape_html(pool, resource->info->conf->rods_zone),
                       ap_escape_html(pool, escape_uri_path(pool, root_dir_without_trailing_slash)),
                       ap_escape_html(pool, escape_uri_path(pool, resource->info->relative_uri)),
                       uri_ends_with_slash ? "" : "/"); // Append a slash to fix relative links on this page.

    deliver_directory_try_insert_local_file(resource, bb, resource->info->conf->html_head);

    apr_brigade_puts(bb, NULL, NULL, "</head>\n<body>\n");

    deliver_directory_try_insert_local_file(resource, bb, resource->info->conf->html_header);

    apr_brigade_puts(bb, NULL, NULL,
                     "<!-- Warning: Do not parse this directory listing programmatically,\n"
                     "              the format may change without notice!\n"
                     "              If you want to script access to these WebDAV collections,\n"
                     "              please use the PROPFIND method instead. -->\n\n"
                     "<h1>Index of <span class=\"relative-uri\">");

    {
        // Print breadcrumb path.
        size_t uri_len = strlen(resource->info->relative_uri);
        char * const path = apr_pcalloc(pool, uri_len + 2);
        strcpy(path, resource->info->relative_uri);
        if (!uri_ends_with_slash)
            path[uri_len] = '/';

        char *p = path;
        const char *part = p;
        for (; *p; ++p) {
            if (*p == '/') {
                *p = '\0';
                apr_brigade_printf(bb, NULL, NULL,
                                   "<a href=\"%s%s/\">%s</a>%s",
                                   ap_escape_html(pool, escape_uri_path(pool, root_dir_without_trailing_slash)),
                                   ap_escape_html(pool, escape_uri_path(pool, path)),
                                   p == path ? "/" : ap_escape_html(pool, part+1),
                                   p == path ? ""  : "/");
                *p = '/';
                part = p;
            }
        }
    }

    apr_brigade_printf(bb, NULL, NULL, "</span> on <span class=\"zone-name\">%s</span></h1>\n",
                       ap_escape_html(pool, resource->info->conf->rods_zone));

    if (strcmp(resource->info->relative_uri, "/") && resource->info->relative_uri[0])
        apr_brigade_puts(bb, NULL, NULL, "<p><a class=\"parent-link\" href=\"..\">Parent collection</a></p>\n");

    apr_brigade_puts(bb, NULL, NULL,
                     "<table>\n<thead>\n"
                     "  <tr><th class=\"name\">Name</th><th class=\"size\">Size</th><th class=\"owner\">Owner</th><th class=\"date\">Last modified</th></tr>\n"

                     "</thead>\n<tbody>\n");

    // Actually print the directory listing, one table row at a time.
    do {
        status = rclReadCollection(resource->info->rods_conn, &coll_handle, &coll_entry);

        if (status < 0) {
            if (status == CAT_NO_ROWS_FOUND) {
                // End of collection.
            } else {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, resource->info->r,
                              "rcReadCollection failed for collection <%s> with error <%s>",
                              resource->info->rods_path, get_rods_error_msg(status));

                apr_brigade_destroy(bb);

                return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
                                     "Could not read a collection entry from a collection.");
            }
        } else {
            const char *name = coll_entry.objType == DATA_OBJ_T
                ? coll_entry.dataName
                : davrods_get_basename(coll_entry.collName);

            char *extension = NULL;
            if (coll_entry.objType == DATA_OBJ_T) {
                // Data object. Extract the extension to assist theming.
                const char *orig_extension = strrchr(name, '.'); // Includes the dot.
                if (orig_extension && strlen(orig_extension) > 1) {
                    extension = apr_pstrdup(pool, orig_extension + 1);
                    assert(extension);
                    size_t len = strlen(extension);
                    for (size_t i = 0; i < len; ++i) {
                        if (extension[i] >= 'A' && extension[i] <= 'Z') {
                            // Lowercase.
                            extension[i] = extension[i] + ('a' - 'A');

                        } else if ((extension[i] >= 'a' && extension[i] <= 'z')
                                || (extension[i] >= '0' && extension[i] <= '9')
                                || extension[i] == '-'
                                || extension[i] == '_') {
                            // OK.
                        } else {
                            // Restrict allowed extension characters to keep HTML class name well-formed.
                            extension = NULL;
                            break;
                        }
                    }
                }
            }

            apr_brigade_printf(bb, NULL, NULL, "  <tr class=\"object%s%s%s\">",
                               coll_entry.objType   == COLL_OBJ_T ? " collection"
                               : coll_entry.objType == DATA_OBJ_T ? " data-object"
                               : "",
                               extension ? " extension-" : "",
                               extension ?   extension   : "");

            // Generate link.
            if (coll_entry.objType == COLL_OBJ_T) {
                // Collection links need a trailing slash for the '..' links to work correctly.
                apr_brigade_printf(bb, NULL, NULL, "<td class=\"name\"><a href=\"%s/\">%s/</a></td>",
                                   ap_escape_html(pool, escape_uri_path(pool, name)),
                                   ap_escape_html(pool, name));
            } else {
                apr_brigade_printf(bb, NULL, NULL, "<td class=\"name\"><a href=\"%s\">%s</a></td>",
                                   ap_escape_html(pool, escape_uri_path(pool, name)),
                                   ap_escape_html(pool, name));
            }

            // Print data object size.
            if (coll_entry.objType == DATA_OBJ_T) {
                char size_buf[5] = { 0 };
                // Fancy file size formatting.
                apr_strfsize(coll_entry.dataSize, size_buf);
                if (size_buf[0])
                    apr_brigade_printf(bb, NULL, NULL, "<td class=\"size\">%s</td>", size_buf);
                else
                    apr_brigade_printf(bb, NULL, NULL, "<td class=\"size\">%lu</td>", coll_entry.dataSize);
            } else {
                apr_brigade_puts(bb, NULL, NULL, "<td class=\"size\"></td>");
            }

            // Print owner.
            apr_brigade_printf(bb, NULL, NULL, "<td class=\"owner\">%s</td>",
                               ap_escape_html(pool, coll_entry.ownerName));

            // Print modified-date string.
            uint64_t       timestamp    = atoll(coll_entry.modifyTime);
            apr_time_t     apr_time     = 0;
            apr_time_exp_t exploded     = { 0 };
            char           date_str[64] = { 0 };

            apr_time_ansi_put(&apr_time, timestamp);
            apr_time_exp_lt(&exploded, apr_time);

            size_t ret_size;
            if (!apr_strftime(date_str, &ret_size, sizeof(date_str), "%Y-%m-%d %H:%M", &exploded)) {
                apr_brigade_printf(bb, NULL, NULL, "<td class=\"date\">%s</td>",
                                   ap_escape_html(pool, date_str));
            } else {
                // Fallback, just in case.
                static_assert(sizeof(date_str) >= APR_RFC822_DATE_LEN,
                              "Size of date_str buffer too low for RFC822 date");
                int status = apr_rfc822_date(date_str, timestamp*1000*1000);
                apr_brigade_printf(bb, NULL, NULL, "<td class=\"date\">%s</td>",
                                   ap_escape_html(pool, status >= 0 ? date_str : "Thu, 01 Jan 1970 00:00:00 GMT"));
            }

            apr_brigade_puts(bb, NULL, NULL, "</tr>\n");
        }
    } while (status >= 0);

    apr_brigade_puts(bb, NULL, NULL, "</tbody>\n</table>\n");

    deliver_directory_try_insert_local_file(resource, bb, resource->info->conf->html_footer);

    // End HTML document.
    apr_brigade_puts(bb, NULL, NULL, "</body>\n</html>\n");

    // Flush.
    if ((status = ap_pass_brigade(output, bb)) != APR_SUCCESS) {
        apr_brigade_destroy(bb);
        return dav_new_error(pool, HTTP_INTERNAL_SERVER_ERROR, 0, status,
                             "Could not write contents to filter.");
    }

    bkt = apr_bucket_eos_create(output->c->bucket_alloc);

    APR_BRIGADE_INSERT_TAIL(bb, bkt);

    if ((status = ap_pass_brigade(output, bb)) != APR_SUCCESS) {
        apr_brigade_destroy(bb);
        return dav_new_error(pool, HTTP_INTERNAL_SERVER_ERROR, 0, status,
                             "Could not write content to filter.");
    }
    apr_brigade_destroy(bb);

    return NULL;
}
