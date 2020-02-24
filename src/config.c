/**
 * \file
 * \brief     Davrods configuration.
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

/// A set of default configuration options.
/// Keep these values in sync with the example vhost files.
/// Note that changes to default values are breaking changes.
const davrods_dir_conf_t default_config = {
    // We have no 'enabled' flag. Module activation is triggered by
    // directives 'AuthBasicProvider irods' and 'Dav irods'.
    .rods_host              = "localhost",
    .rods_port              = 1247,
    .rods_zone              = "tempZone",
    .rods_default_resource  = "",
    .rods_auth_scheme       = DAVRODS_AUTH_NATIVE,

    // The default path should ideally be based on the known(?) location of
    // Apache's config dir, distro-dependent...
    .rods_env_file          = "/etc/httpd/irods/irods_environment.json",

    // Default to having the user's home directory as the exposed root
    // because that collection is more or less guaranteed to be readable by
    // the current user (as opposed to the /<zone>/home directory, which
    // is hidden for rodsusers by default).
    .rods_exposed_root      = "User", // NOTE: Keep this in sync with the option below.
    .rods_exposed_root_type = DAVRODS_ROOT_USER_DIR,

    // Default to 4 MiB buffer sizes, which is a good balance
    // between resource usage and transfer performance in common
    // setups.
    .rods_tx_buffer_size    = 4 * 1024 * 1024,
    .rods_rx_buffer_size    = 4 * 1024 * 1024,

    .tmpfile_rollback       = DAVRODS_TMPFILE_ROLLBACK_OFF,
    .locallock_lockdb_path  = "/var/lib/davrods/lockdb_locallock",

    .anonymous_mode          = DAVRODS_ANONYMOUS_MODE_OFF,
    .anonymous_auth_username = "anonymous",
    .anonymous_auth_password = "",

    // Use the minimum PAM temporary password TTL. We
    // re-authenticate using PAM on every new HTTP connection, so
    // there's no use keeping the temporary password around for
    // longer than the maximum keepalive time. (We don't ever use
    // a temporary password more than once).
    .rods_auth_ttl = 1, // In hours.

    .html_head   = "",
    .html_header = "",
    .html_footer = "",

    .force_download = DAVRODS_FORCE_DOWNLOAD_OFF,
};

void *davrods_create_dir_config(apr_pool_t *p, char *dir) {
    // Zeroed configuration => default value is used for everything.
    // This allows us to detect whether a config value was actually set for a
    // given directory, and properly allow unset options to be overridden
    // during a config merge.
    return apr_pcalloc(p, sizeof(davrods_dir_conf_t));
}

void *davrods_merge_dir_config(apr_pool_t *p, void *_parent, void *_child) {
    davrods_dir_conf_t *parent = _parent;
    davrods_dir_conf_t *child  = _child;
    davrods_dir_conf_t *conf   = davrods_create_dir_config(p, "merge__");

#define MERGE(_prop) \
    conf->_prop = child->_prop \
        ? child->_prop \
        : parent->_prop \
            ? parent->_prop \
            : conf->_prop

    MERGE(rods_host);
    MERGE(rods_port);
    MERGE(rods_zone);
    MERGE(rods_default_resource);
    MERGE(rods_env_file);
    MERGE(rods_auth_scheme);

    const char *exposed_root = child->rods_exposed_root
        ? child->rods_exposed_root
        : parent->rods_exposed_root
            ? parent->rods_exposed_root
            : conf->rods_exposed_root;

    MERGE(rods_tx_buffer_size);
    MERGE(rods_rx_buffer_size);

    MERGE(tmpfile_rollback);
    MERGE(locallock_lockdb_path);

    MERGE(anonymous_mode);
    MERGE(anonymous_auth_username);
    MERGE(anonymous_auth_password);

    { int ret = set_exposed_root(conf, exposed_root);
      assert(ret >= 0); }

    MERGE(html_head);
    MERGE(html_header);
    MERGE(html_footer);

    MERGE(force_download);

#undef MERGE

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
    int argc, char *const argv[]
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    if (argc == 0)
        conf->rods_default_resource = "";
    else if (argc == 1)
        conf->rods_default_resource = argv[0];
    else
        return "Expected either an empty string or a single resource name";
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

static const char *cmd_davrodsforcedownload(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    if (!strcasecmp(arg1, "on")) {
        conf->force_download = DAVRODS_FORCE_DOWNLOAD_ON;
    } else if (!strcasecmp(arg1, "off")) {
        conf->force_download = DAVRODS_FORCE_DOWNLOAD_OFF;
    } else {
        return "This directive accepts only 'On' and 'Off' values";
    }

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
    AP_INIT_TAKE_ARGV(
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
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "ForceDownload", cmd_davrodsforcedownload,
        NULL, ACCESS_CONF, "When On, prevents inline display of files in web browsers"
    ),

    { NULL }
};
