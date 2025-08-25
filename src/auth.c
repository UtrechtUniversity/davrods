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
#include "auth.h"
#include "config.h"
#include "common.h"

#include <mod_auth.h>
#include <http_request.h>

#include <irods/rodsClient.h>
#include <irods/sslSockComm.h>

#ifdef APLOG_USE_MODULE
APLOG_USE_MODULE(davrods);
#endif

/**
 * \brief iRODS connection cleanup function.
 *
 * \param mem a pointer to a rcComm_t struct.
 */
static apr_status_t rods_conn_cleanup(void *mem) {
    rcComm_t *rods_conn = (rcComm_t*)mem;
    WHISPER("Closing iRODS connection at %p\n", rods_conn);

    if (rods_conn)
        rcDisconnect(rods_conn);

    WHISPER("iRODS connection CLOSED\n");
    return APR_SUCCESS;
}

/**
 * \brief Perform an iRODS PAM login, return a temporary password.
 *
 * \param[in]  r            request record
 * \param[in]  rods_conn
 * \param[in]  password
 * \param[in]  ttl          temporary password ttl
 * \param[out] tmp_password
 *
 * \return an iRODS status code (0 on success)
 */
static int do_rods_login_pam(
    request_rec *r,
    rcComm_t    *rods_conn,
    const char  *password,
    int          ttl,
    char       **tmp_password
) {

    // Perform a PAM login. The connection must be encrypted at this point.

    pamAuthRequestInp_t auth_req_params = {
        .pamPassword = apr_pstrdup(r->pool, password),
        .pamUser     = apr_pstrdup(r->pool, rods_conn->proxyUser.userName),
        .timeToLive  = ttl
    };

    pamAuthRequestOut_t *auth_req_result = NULL;
    int status = rcPamAuthRequest(rods_conn, &auth_req_params, &auth_req_result);
    if (status) {
        ap_log_rerror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, r,
                      "rcPamAuthRequest failed: %d = %s",
                      status, get_rods_error_msg(status));
        sslEnd(rods_conn);
        return status;
    }

    *tmp_password = apr_pstrdup(r->pool, auth_req_result->irodsPamPassword);

    // Who owns auth_req_result? I guess that's us.
    // Better not forget to free its contents too.
    free(auth_req_result->irodsPamPassword);
    free(auth_req_result);
    // (Is there no freeRodsObjStat equivalent for this? I don't know, it's undocumented).

    return status;
}

/**
 * \brief Connect to iRODS and attempt to login.
 *
 * \param[in]  r         request record
 * \param[in]  username
 * \param[in]  password
 * \param[out] rods_conn will be filled with the new iRODS connection, if auth is successful.
 *
 * \return An authn status code, AUTH_GRANTED if successful.
 */
static authn_status rods_login(
    request_rec *r,
    const char  *username,
    const char  *password,
    rcComm_t   **rods_conn
) {
    // Verify credentials lengths

    if (strlen(username) > 63) {
        // This is the NAME_LEN and DB_USERNAME_LEN limit set by iRODS.
        ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, r,
                      "Username exceeded max name length (63)");
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if (strlen(password) > 63) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, r,
            "Password exceeds length limits (%lu vs 63)", strlen(password));
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // Get config.
    davrods_dir_conf_t *conf = ap_get_module_config(
        r->per_dir_config,
        &davrods_module
    );
    assert(conf);

    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                  "Connecting to iRODS using address <%s:%d>, username <%s> and zone <%s>",
                  DAVRODS_CONF(conf, rods_host),
                  DAVRODS_CONF(conf, rods_port),
                  username,
                  DAVRODS_CONF(conf, rods_zone));

    authn_status result = AUTH_USER_NOT_FOUND;

    // Point the iRODS client library to the webserver's iRODS env file.
    //setenv("IRODS_ENVIRONMENT_FILE", "/dev/null", 1);
    setenv("IRODS_ENVIRONMENT_FILE", DAVRODS_CONF(conf, rods_env_file), 1);

    // Set spOption variable so that the connection will be labelled as
    // a Davrods connection in ips.
    setenv("spOption", "Davrods", 1);

    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                  "Using iRODS env file at <%s>", getenv("IRODS_ENVIRONMENT_FILE"));

    rErrMsg_t rods_errmsg;
    *rods_conn = rcConnect(
        DAVRODS_CONF(conf, rods_host),
        DAVRODS_CONF(conf, rods_port),
        username,
        DAVRODS_CONF(conf, rods_zone),
        0,
        &rods_errmsg
    );

    if (*rods_conn) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                      "Successfully connected to iRODS zone '%s'", DAVRODS_CONF(conf, rods_zone));
        miscSvrInfo_t *server_info = NULL;
        rcGetMiscSvrInfo(*rods_conn, &server_info);

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                      "Server version: %s", server_info->relVersion);

        // Whether to use SSL for the entire connection.
        // Note: SSL is always in effect during PAM auth, regardless of negotiation results.
        bool use_ssl = false;

        if ((*rods_conn)->negotiation_results
            && !strcmp((*rods_conn)->negotiation_results, "CS_NEG_USE_SSL")) {
            use_ssl = true;
        } else {
            // Negotiation was disabled or resulted in CS_NEG_USE_TCP (i.e. no SSL).
        }

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                      "SSL negotiation result: <%s>: %s",
                      (*rods_conn)->negotiation_results,
                      use_ssl
                          ? "will use SSL for the entire connection"
                          : "will NOT use SSL (if using PAM, SSL will only be used during auth)"
                      );

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                      "Is SSL currently on? (ssl* = %d, ssl_on = %d)"
                      " (ignore ssl_on, it seems 4.x does not update it after SSL is turned on automatically during rcConnect)",
                      (*rods_conn)->ssl?1:0, (*rods_conn)->ssl_on);

        if (use_ssl) {
            // Verify that SSL is in effect in compliance with the
            // negotiation result, to prevent any unencrypted
            // information (password or data) to be sent in the clear.

            if (!(*rods_conn)->ssl) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, r,
                              "SSL should have been turned on at this point (negotiation result was <%s>)."
                              " Aborting for security reasons.",
                              (*rods_conn)->negotiation_results);
                return HTTP_INTERNAL_SERVER_ERROR;
            }
        }

        // If the negotiation result requires plain TCP, but we are
        // using the PAM auth scheme, we need to turn on SSL during
        // auth.
        if (!use_ssl && DAVRODS_CONF(conf, rods_auth_scheme) == DAVRODS_AUTH_PAM) {
            if ((*rods_conn)->ssl) {
                // This should not happen.
                // In this situation we don't know if we should stop
                // SSL after PAM auth or keep it on, so we fail
                // instead.
                ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, r,
                              "SSL should NOT have been turned on at this point (negotiation result was <%s>). Aborting.",
                              (*rods_conn)->negotiation_results);

                return HTTP_INTERNAL_SERVER_ERROR;
            }
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, "Enabling SSL for PAM auth");

            int status = sslStart(*rods_conn);
            if (status) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, r,
                              "sslStart for PAM failed: %d = %s",
                              status, get_rods_error_msg(status));

                return HTTP_INTERNAL_SERVER_ERROR;
            }
        }

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, "Logging in");

        // clientLoginWithPassword()'s signature specifies a WRITABLE password parameter.
        // I don't expect it to actually write to this field, but we'll play it
        // safe and pass it a temporary buffer.
        //
        // This password field will be destroyed at the end of the HTTP request.
        char *password_buf = apr_pstrdup(r->pool, password);

        int status = 0;

        if (DAVRODS_CONF(conf, rods_auth_scheme) == DAVRODS_AUTH_PAM) {
            char *tmp_password = NULL;
            status = do_rods_login_pam(
                r,
                *rods_conn,
                password_buf,
                DAVRODS_CONF(conf, rods_auth_ttl),
                &tmp_password
            );
            if (!status) {
                password_buf = apr_pstrdup(r->pool, tmp_password);

                // Login using the received temporary password.
                status = clientLoginWithPassword(*rods_conn, password_buf);
            }

        } else if (DAVRODS_CONF(conf, rods_auth_scheme) == DAVRODS_AUTH_NATIVE) {
            status = clientLoginWithPassword(*rods_conn, password_buf);
        } else {
            // This shouldn't happen.
            assert(0 && "Unimplemented auth scheme");
        }

        if (status) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                          "Login failed: %d = %s", status, get_rods_error_msg(status));
            result = AUTH_DENIED;

            rcDisconnect(*rods_conn);
            *rods_conn = NULL;

        } else {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, "Login successful");
            result = AUTH_GRANTED;

            // Disable SSL if it was in effect during auth but negotiation (or lack
            // thereof) demanded plain TCP for the rest of the connection.
            if (!use_ssl && (*rods_conn)->ssl) {
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                              "Disabling SSL (was used for PAM only)");

                if (DAVRODS_CONF(conf, rods_auth_scheme) != DAVRODS_AUTH_PAM) {
                    // This should not happen.
                    ap_log_rerror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, r,
                                  "SSL was turned on, but not for PAM."
                                  " This conflicts with the negotiation result (%s)!",
                                  (*rods_conn)->negotiation_results);
                }
                status = sslEnd(*rods_conn);
                if (status) {
                    ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, r,
                                  "sslEnd failed after PAM auth: %d = %s",
                                  status, get_rods_error_msg(status));

                    return HTTP_INTERNAL_SERVER_ERROR;
                }
            }
        }
    } else {
        ap_log_rerror(
            APLOG_MARK, APLOG_ERR, APR_SUCCESS, r,
            "Could not connect to iRODS using address <%s:%d>,"
            " username <%s> and zone <%s>. iRODS says: '%s'",
            DAVRODS_CONF(conf, rods_host),
            DAVRODS_CONF(conf, rods_port),
            username,
            DAVRODS_CONF(conf, rods_zone),
            rods_errmsg.msg
        );

        return HTTP_INTERNAL_SERVER_ERROR;
    }

    return result;
}

// Check whether an open iRODS connection from a previous request can be reused
// by a HTTP keepalive request.
// For this to be permissible, we require that the current Davrods
// configuration options regarding authentication matches the configuration
// that was used in the previous request. In essence:
//
// 1. We must be using the same iRODS authentication scheme
// 2. Our anonymous mode switches must be the same
// 3. We must be using the same username with the same password
//
// This function is called in the following situations:
// - From `check_rods` during HTTP basic auth when an existing connection is found.
// - From repo.c `get_davrods_pool`.
//
// When username and password are NULL, they are not checked. This is used in
// repo.c which has no direct access to the username and password used for the
// current request. The check that the credentials are the same is then
// performed elsewhere. See repo.c `get_davrods_pool` for details.
//
bool davrods_user_can_reuse_connection(request_rec *r,
                                       const char *username,
                                       const char *password) {
    // Obtain davrods directory config.
    davrods_dir_conf_t *conf = ap_get_module_config(
        r->per_dir_config,
        &davrods_module
    );

    void *m;

    // Get davrods memory pool.
    apr_pool_t *pool = NULL;
    int status = apr_pool_userdata_get(&m, "davrods_pool", r->connection->pool);
    pool   = (apr_pool_t*)m;

    if (status || !pool)
        return false; // No pool yet.

    rcComm_t *rods_conn = NULL;
    status = apr_pool_userdata_get(&m, "rods_conn", pool);
    rods_conn = (rcComm_t*)m;

    if (status || !rods_conn)
        return false; // No iRODS connection set up yet.

    davrods_session_parameters_t *session_params = NULL;
    status = apr_pool_userdata_get((void**)&session_params, "session_params", pool);
    assert(!status && session_params);

    if (session_params->anon_mode != DAVRODS_CONF(conf, anonymous_mode))
        return false; // Disallow reusing a non-anonymous connection for
                      // authorized access and vice versa.

    if (session_params->auth_scheme != DAVRODS_CONF(conf, rods_auth_scheme))
        return false; // Disallow reusing a PAM authed connection for
                      // Native authed access and vice versa.

    char *current_username = NULL;
    status = apr_pool_userdata_get((void**)&current_username, "username", pool);
    assert(!status && current_username);

    if (username && strcmp(current_username, username))
        return false; // Disallow reusing a connection authed for user A
                      // to be reused by user B.

    char *current_password = NULL;
    status = apr_pool_userdata_get((void**)&current_password, "password", pool);
    assert(!status && current_password);

    if (password && strcmp(current_password, password))
        return false; // Password must match as well.

    return true;
}

authn_status check_rods(request_rec *r, const char *username, const char *password, bool is_basic_auth) {
    int status;

    // Obtain davrods directory config.
    davrods_dir_conf_t *conf = ap_get_module_config(
        r->per_dir_config,
        &davrods_module
    );
    authn_status result = AUTH_USER_NOT_FOUND;

    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                  "Authenticating iRODS username '%s' using %s auth scheme.",
                  username,
                  DAVRODS_CONF(conf, rods_auth_scheme) == DAVRODS_AUTH_PAM
                      ? "PAM"
                      : "Native"
                 );

    void *m;

    // Get davrods memory pool.
    apr_pool_t *pool = NULL;
    status = apr_pool_userdata_get(&m, "davrods_pool", r->connection->pool);
    pool   = (apr_pool_t*)m;

    if (status || !pool) {
        // We create a davrods pool as a child of the connection pool.
        // iRODS sessions last at most as long as the client's TCP connection.
        //
        // Using our own pool ensures that we can easily clear it (= close the
        // iRODS connection and free related resources) when a client reuses
        // their connection for a different username.
        //
        status = apr_pool_create(&pool, r->connection->pool);

        if (status || !pool) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r,
                          "Could not create Davrods apr pool");
            return HTTP_INTERNAL_SERVER_ERROR;
        }

        // It seems strange that we bind our pool to the connection pool twice,
        // firstly by creating it as a child, and secondly as a userdata
        // property so we can access it in later requests / processing steps.
        // If there were a method to enumerate child pools, the second binding
        // could be avoided, but alas.
        apr_pool_userdata_set(pool, "davrods_pool", apr_pool_cleanup_null, r->connection->pool);
    }

    // We have a pool, now try to extract its iRODS connection.
    rcComm_t *rods_conn = NULL;
    status = apr_pool_userdata_get(&m, "rods_conn", pool);
    rods_conn = (rcComm_t*)m;

    if (!status && rods_conn) {
        // We have an open iRODS connection with an authenticated user.
        // Can we safely reuse it for this request?
        bool can_reuse = davrods_user_can_reuse_connection(r, username, password);

        char *current_username = NULL;
        status = apr_pool_userdata_get(&m, "username", pool);
        current_username = (char*)m;
        assert(!status && current_username);

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                      "iRODS connection already open, authenticated user is '%s'",
                      current_username);

        if (can_reuse) {
            // Yes!
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                          "Granting access to already authenticated user on"
                          " existing iRODS connection");
            result = AUTH_GRANTED;

            // Mark this request (note: r->pool, not pool) as being authed
            // with user-supplied credentials.
            apr_pool_userdata_set(r, // Dummy pointer, just need anything that isn't NULL.
                                  "davrods_request_was_basic_authed",
                                  apr_pool_cleanup_null,
                                  r->pool);

        } else {
            // No! We need to reauthenticate to iRODS for the new user. Clean
            // up the resources of the current iRODS connection first.
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r,
                          "Closing existing iRODS connection for user '%s'"
                          " (need new connection for user '%s')",
                          current_username,
                          username);
            // This should run the cleanup function for rods_conn, rcDisconnect.
            apr_pool_clear(pool);
            rods_conn = NULL;
        }
    } else {
        rods_conn = NULL;
    }

    if (result == AUTH_USER_NOT_FOUND) {
        // User is not yet authenticated.

        result = rods_login(r, username, password, &rods_conn);
        if (result == AUTH_GRANTED) {
            assert(rods_conn);

            char *username_buf = apr_pstrdup(pool, username);
            char *password_buf = apr_pstrdup(pool, password);

            apr_pool_userdata_set(rods_conn,    "rods_conn", rods_conn_cleanup,     pool);
            apr_pool_userdata_set(username_buf, "username",  apr_pool_cleanup_null, pool);
            apr_pool_userdata_set(password_buf, "password",  apr_pool_cleanup_null, pool);

            // Get iRODS env and store it.
            rodsEnv *env = apr_palloc(pool, sizeof(rodsEnv));
            assert((status = getRodsEnv(env)) >= 0);

            apr_pool_userdata_set(env, "env", apr_pool_cleanup_null, pool);

            // Store authentication info.
            davrods_session_parameters_t *session_params
                = apr_palloc(pool, sizeof(davrods_session_parameters_t));
            assert(session_params);
            session_params->auth_scheme = DAVRODS_CONF(conf, rods_auth_scheme);
            session_params->anon_mode   = DAVRODS_CONF(conf, anonymous_mode);
            apr_pool_userdata_set(session_params, "session_params", apr_pool_cleanup_null, pool);

            if (is_basic_auth) {
                // Mark this request (note: r->pool, not pool) as being authed
                // with user-supplied credentials.
                apr_pool_userdata_set(r, // Dummy pointer, just need anything that isn't NULL.
                                      "davrods_request_was_basic_authed",
                                      apr_pool_cleanup_null,
                                      r->pool);
            }
        }
    }

    return result;
}

authn_status basic_auth_irods(request_rec *r, const char *username, const char *password) {
    return check_rods(r, username, password, true);
}


static const authn_provider authn_rods_provider = {
    &basic_auth_irods,
    NULL
};

void davrods_auth_register(apr_pool_t *p) {
    ap_register_auth_provider(
        p, AUTHN_PROVIDER_GROUP,
        "irods",
        AUTHN_PROVIDER_VERSION,
        &authn_rods_provider,
        AP_AUTH_INTERNAL_PER_CONF
    );
}
