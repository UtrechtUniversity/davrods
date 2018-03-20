/**
 * \file
 * \brief     Davrods configuration.
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
#include "config.h"

#include <apr_strings.h>

APLOG_USE_MODULE(davrods);

static int set_exposed_root(davrods_dir_conf_t *conf, const char *exposed_root) {
    conf->rods_exposed_root = exposed_root;

    if (!strcasecmp("Zone", exposed_root)) {
        conf->rods_exposed_root_type = DAVRODS_ROOT_ZONE_DIR;
    } else if (!strcasecmp("Home", exposed_root)) {
        conf->rods_exposed_root_type = DAVRODS_ROOT_HOME_DIR;
    } else if (!strcasecmp("User", exposed_root)) {
        conf->rods_exposed_root_type = DAVRODS_ROOT_USER_DIR;
    } else {
        conf->rods_exposed_root_type = DAVRODS_ROOT_CUSTOM_DIR;
        if (!strlen(exposed_root) || exposed_root[0] != '/')
            return -1;
    }
    return 0;
}

void *davrods_create_dir_config(apr_pool_t *p, char *dir) {
    davrods_dir_conf_t *conf = apr_pcalloc(p, sizeof(davrods_dir_conf_t));
    if (conf) {
        // We have no 'enabled' flag. Module activation is triggered by
        // directives 'AuthBasicProvider irods' and 'Dav irods'.
        conf->rods_host              = "localhost";
        conf->rods_port              = 1247;
        conf->rods_zone              = "tempZone";
        conf->rods_default_resource  = "";
        conf->rods_env_file          = "/etc/httpd/irods/irods_environment.json";
        conf->rods_auth_scheme       = DAVRODS_AUTH_NATIVE;

        // Default to having the user's home directory as the exposed root
        // because that collection is more or less guaranteed to be readable by
        // the current user (as opposed to the /<zone>/home directory, which
        // is hidden for rodsusers by default).
        conf->rods_exposed_root      = "User"; // NOTE: Keep this in sync with the option below.
        conf->rods_exposed_root_type = DAVRODS_ROOT_USER_DIR;

        // Default to 4 MiB buffer sizes, which is a good balance
        // between resource usage and transfer performance in common
        // setups.
        conf->rods_tx_buffer_size    = 4 * 1024 * 1024;
        conf->rods_rx_buffer_size    = 4 * 1024 * 1024;

        conf->tmpfile_rollback       = DAVRODS_TMPFILE_ROLLBACK_OFF;
        conf->locallock_lockdb_path  = "/var/lib/davrods/lockdb_locallock";

        conf->anonymous_mode          = DAVRODS_ANONYMOUS_MODE_OFF;
        conf->anonymous_auth_username = "anonymous";
        conf->anonymous_auth_password = "";

        // Use the minimum PAM temporary password TTL. We
        // re-authenticate using PAM on every new HTTP connection, so
        // there's no use keeping the temporary password around for
        // longer than the maximum keepalive time. (We don't ever use
        // a temporary password more than once).
        conf->rods_auth_ttl = 1; // In hours.

        conf->html_head   = "";
        conf->html_header = "";
        conf->html_footer = "";
    }
    return conf;
}

void *davrods_merge_dir_config(apr_pool_t *p, void *_parent, void *_child) {
    davrods_dir_conf_t *parent = _parent;
    davrods_dir_conf_t *child  = _child;
    davrods_dir_conf_t *conf   = davrods_create_dir_config(p, "merge__");

#define DAVRODS_PROP_MERGE(_prop) \
    conf->_prop = child->_prop \
        ? child->_prop \
        : parent->_prop \
            ? parent->_prop \
            : conf->_prop

    DAVRODS_PROP_MERGE(rods_host);
    DAVRODS_PROP_MERGE(rods_port);
    DAVRODS_PROP_MERGE(rods_zone);
    DAVRODS_PROP_MERGE(rods_default_resource);
    DAVRODS_PROP_MERGE(rods_env_file);
    DAVRODS_PROP_MERGE(rods_auth_scheme);

    const char *exposed_root = child->rods_exposed_root
        ? child->rods_exposed_root
        : parent->rods_exposed_root
            ? parent->rods_exposed_root
            : conf->rods_exposed_root;

    DAVRODS_PROP_MERGE(rods_tx_buffer_size);
    DAVRODS_PROP_MERGE(rods_rx_buffer_size);

    DAVRODS_PROP_MERGE(tmpfile_rollback);
    DAVRODS_PROP_MERGE(locallock_lockdb_path);

    DAVRODS_PROP_MERGE(anonymous_mode);
    DAVRODS_PROP_MERGE(anonymous_auth_username);
    DAVRODS_PROP_MERGE(anonymous_auth_password);

    assert(set_exposed_root(conf, exposed_root) >= 0);

    DAVRODS_PROP_MERGE(html_head);
    DAVRODS_PROP_MERGE(html_header);
    DAVRODS_PROP_MERGE(html_footer);

#undef DAVRODS_PROP_MERGE

    return conf;
}

// Config setters {{{

static const char *cmd_davrodsserver(
    cmd_parms *cmd, void *config,
    const char *arg1, const char *arg2
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    conf->rods_host = arg1;

#define DAVRODS_MIN(x, y) ((x) <= (y) ? (x) : (y))
#define DAVRODS_MAX(x, y) ((x) >= (y) ? (x) : (y))

    apr_int64_t port = apr_atoi64(arg2);
    if (port == DAVRODS_MIN(65535, DAVRODS_MAX(1, port))) {
        conf->rods_port = port;
        return NULL;
    } else {
        return "iRODS server port out of range (1-65535)";
    }

#undef DAVRODS_MIN
#undef DAVRODS_MAX

}

static const char *cmd_davrodsauthscheme(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    if (!strcasecmp("Native", arg1)) {
        conf->rods_auth_scheme = DAVRODS_AUTH_NATIVE;
    } else if (!strcasecmp("PAM", arg1)) {
        conf->rods_auth_scheme = DAVRODS_AUTH_PAM;
    } else if (!strcasecmp("Standard", arg1)) {
        return "Invalid iRODS authentication scheme. Did you mean 'Native'?";
    } else {
        return "Invalid iRODS authentication scheme. Valid options are 'Native' or 'PAM'.";
    }
    return NULL;
}

static const char *cmd_davrodsauthttl(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    apr_int64_t ttl = apr_atoi64(arg1);
    if (ttl <= 0) {
        return "The auth TTL must be higher than zero.";
    } else if (errno == ERANGE || ttl >> 31) {
        return "Auth TTL is too high - please specify a value that fits in an int32_t (i.e.: no more than 2 billion hours).";
    } else {
        conf->rods_auth_ttl = (int)ttl;
        return NULL;
    }
}

static const char *cmd_davrodszone(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    conf->rods_zone = arg1;
    return NULL;
}

static const char *cmd_davrodsdefaultresource(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    conf->rods_default_resource = arg1;
    return NULL;
}

static const char *cmd_davrodsenvfile(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    conf->rods_env_file = arg1;
    return NULL;
}

static const char *cmd_davrodsexposedroot(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    if (set_exposed_root(conf, arg1) < 0) {
        if (
            conf->rods_exposed_root_type == DAVRODS_ROOT_CUSTOM_DIR
            && (
                !strlen(conf->rods_exposed_root)
              || conf->rods_exposed_root[0] != '/'
            )
        ) {
            return "iRODS exposed root must be one of 'Zone', 'Home', 'User' or a custom path starting with a '/'";
        } else {
            return "Could not set iRODS exposed root, is your path sane?";
        }
    }
    return NULL;
}

static const char *cmd_davrodstxbufferkbs(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    size_t kb = apr_atoi64(arg1);
    conf->rods_tx_buffer_size = kb * 1024;

    if (errno == ERANGE || conf->rods_tx_buffer_size < kb) {
        // This must be an overflow.
        return "Please check if your transfer buffer size is sane";
    }

    return NULL;
}

static const char *cmd_davrodsrxbufferkbs(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    size_t kb = apr_atoi64(arg1);
    conf->rods_rx_buffer_size = kb * 1024;

    if (errno == ERANGE || conf->rods_rx_buffer_size < kb) {
        return "Please check if your receive buffer size is sane";
    }

    return NULL;
}

static const char *cmd_davrodstmpfilerollback(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    if (!strcasecmp(arg1, "on") || !strcasecmp(arg1, "yes")) {
        conf->tmpfile_rollback = DAVRODS_TMPFILE_ROLLBACK_ON;
    } else if (!strcasecmp(arg1, "off") || !strcasecmp(arg1, "no")) {
        conf->tmpfile_rollback = DAVRODS_TMPFILE_ROLLBACK_OFF;
    } else {
        return "This directive accepts only 'On' and 'Off' values";
    }

    return NULL;
}

static const char *cmd_davrodslockdb(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    conf->locallock_lockdb_path = arg1;

    return NULL;
}

static const char *cmd_davrodsanonymousmode(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    if (!strcasecmp(arg1, "on")) {
        conf->anonymous_mode = DAVRODS_ANONYMOUS_MODE_ON;
    } else if (!strcasecmp(arg1, "off")) {
        conf->anonymous_mode = DAVRODS_ANONYMOUS_MODE_OFF;
    } else {
        return "This directive accepts only 'On' and 'Off' values";
    }

    return NULL;
}

static const char *cmd_davrodsanonymouslogin(
    cmd_parms *cmd, void *config,
    int argc, char * const *argv
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    if (argc < 1 || argc > 2)
        return "Specify a username and optionally a password";

    if (!strlen(argv[0]))
        return "Username must not be empty";

    conf->anonymous_auth_username = argv[0];

    if (argc == 2)
        conf->anonymous_auth_password = argv[1];
    else
        conf->anonymous_auth_password = "";

    return NULL;
}

static bool file_readable(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) {
        fclose(f);
        return true;
    } else {
        return false;
    }
}

static const char *cmd_davrodshtmlhead(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    if (strlen(arg1) && !file_readable(arg1))
        return "The given HtmlHead file is not readable by apache";

    conf->html_head = arg1;
    return NULL;
}

static const char *cmd_davrodshtmlheader(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    if (strlen(arg1) && !file_readable(arg1))
        return "The given HtmlHeader file is not readable by apache";

    conf->html_header = arg1;
    return NULL;
}

static const char *cmd_davrodshtmlfooter(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    if (strlen(arg1) && !file_readable(arg1))
        return "The given HtmlFooter file is not readable by apache";

    conf->html_footer = arg1;
    return NULL;
}

// }}}

const command_rec davrods_directives[] = {
    AP_INIT_TAKE2(
        DAVRODS_CONFIG_PREFIX "Server", cmd_davrodsserver,
        NULL, ACCESS_CONF, "iRODS host and port to connect to"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "AuthScheme", cmd_davrodsauthscheme,
        NULL, ACCESS_CONF, "iRODS authentication scheme to use (either Native or PAM)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "AuthTTLHours", cmd_davrodsauthttl,
        NULL, ACCESS_CONF, "Time-to-live of the temporary password in hours (only applies to PAM currently)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "Zone", cmd_davrodszone,
        NULL, ACCESS_CONF, "iRODS zone"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "DefaultResource", cmd_davrodsdefaultresource,
        NULL, ACCESS_CONF, "iRODS default resource (optional)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "EnvFile", cmd_davrodsenvfile,
        NULL, ACCESS_CONF, "iRODS environment file (defaults to /etc/httpd/irods/irods_environment.json)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "ExposedRoot", cmd_davrodsexposedroot,
        NULL, ACCESS_CONF, "Root iRODS collection to expose (one of: Zone, Home, User, or a custom path)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "TxBufferKbs", cmd_davrodstxbufferkbs,
        NULL, ACCESS_CONF, "Amount of file KiBs to upload to iRODS at a time on PUTs"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "RxBufferKbs", cmd_davrodsrxbufferkbs,
        NULL, ACCESS_CONF, "Amount of file KiBs to download from iRODS at a time on GETs"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "TmpfileRollback", cmd_davrodstmpfilerollback,
        NULL, ACCESS_CONF, "Support PUT rollback through the use of temporary files on the target iRODS resource"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "LockDB", cmd_davrodslockdb,
        NULL, ACCESS_CONF, "Lock database location, used by the davrods-locallock DAV provider"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "AnonymousMode", cmd_davrodsanonymousmode,
        NULL, ACCESS_CONF, "Anonymous mode On/Off switch"
    ),
    AP_INIT_TAKE_ARGV(
        DAVRODS_CONFIG_PREFIX "AnonymousLogin", cmd_davrodsanonymouslogin,
        NULL, ACCESS_CONF, "Anonymous mode username and optional password"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HtmlHead", cmd_davrodshtmlhead,
        NULL, ACCESS_CONF, "File that's inserted into HTML directory listings, in the head tag"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HtmlHeader", cmd_davrodshtmlheader,
        NULL, ACCESS_CONF, "File that's inserted into HTML directory listings, in the body tag"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HtmlFooter", cmd_davrodshtmlfooter,
        NULL, ACCESS_CONF, "File that's inserted into HTML directory listings, in the body tag"
    ),

    { NULL }
};
