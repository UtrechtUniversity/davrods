Davrods - An Apache WebDAV interface to iRODS
=============================================

Davrods provides access to iRODS servers using the WebDAV protocol.
It is a bridge between the WebDAV protocol and the iRODS API,
implemented as an Apache HTTPD module.

Davrods leverages the Apache server implementation of the WebDAV
protocol, `mod_dav`, for compliance with the WebDAV Class 2 standard.

Notable features include:

- Supports WebDAV Class 2. Locks are local to the Apache server.
- Supports PAM and Native (a.k.a. STANDARD) iRODS authentication.
- Supports SSL encryption for the entire iRODS connection.
- Easy to configure using Apache configuration directives.
- Supports iRODS server versions 4+ and is backwards compatible with 3.3.1.

## Installation ##

### Prerequisites ###

Davrods requires the following packages to be installed on your server:

- Apache httpd 2.4+
- iRODS 4.1.x client libraries (in package `irods-runtime`, available
  from [the iRODS website](http://irods.org/download/))

### Using the binary distribution ###

For binary installation, download the package for your platform at
https://github.com/cjsmeele/davrods/releases and install it using your
package manager.

We currently distribute RPM packages for CentOS 7 only.
If you require packaging for a different platform, please contact us
by creating an issue.

See the __Configuration__ section for instructions on how to configure
Davrods once it has been installed.

### Davrods and SELinux ##

If the machine on which you install Davrods is protected by SELinux,
you may need to make changes to your policies to allow davrods to run:

- Apache HTTPD must be allowed to connect to TCP port 1247
- Davrods must be allowed to dynamically load iRODS client plugin
  libraries in /var/lib/irods/plugins/network

For example, the following two commands can be used to resolve these
requirements:

    setsebool -P httpd_can_network_connect true
    chcon -t lib_t /var/lib/irods/plugins/network/lib*.so

## Configuration ##

Davrods is configured in two locations: In a HTTPD vhost configuration
file and in an iRODS environment file. The vhost config is the main
configuration file, the iRODS environment file is used for iRODS
client library configuration, similar to the configuration of
icommands.

### HTTPD vhost configuration ###

The Davrods RPM distribution installs a commented out vhost template
in `/etc/httpd/conf.d/davrods-vhost.conf`. With the comment marks
(`#`) removed, this provides you with a sane default configuration
that you can tune to your needs. All Davrods configuration options are
documented in this file and can be changed to your liking.

### The iRODS environment file ###

The binary distribution installs the `irods_environment.json` file in
`/etc/httpd/irods`. In most iRODS setups, this file can be used as
is.

Importantly, the first seven options (from `irods_host` up to and
including `irods_zone_name`) are **not** read from this file. These
settings are taken from their equivalent Davrods configuration
directives in the vhost file instead.

The options in the provided environment file starting from
`irods_client_server_negotiation` *do* affect the behaviour of
Davrods. See the official documentation for help on these settings at:
https://docs.irods.org/master/manual/configuration/#irodsirods_environmentjson

For instance, if you want Davrods to connect to iRODS 3.3.1, the
`irods_client_server_negotiation` option must be set to `"none"`.

## Building from source ##

To build from source, the following build-time dependencies must be
installed (package names may differ on your platform):

- `httpd-devel >= 2.4`
- `apr-devel`
- `apr-util-devel`
- `irods-dev`

Additionally, the following runtime dependencies must be installed:

- `httpd >= 2.4`
- `irods-runtime >= 4.1.8`
- `jansson`
- `boost`
- `boost-system`
- `boost-filesystem`
- `boost-regex`
- `boost-thread`
- `boost-chrono`

First, browse to the directory where you have unpacked the Davrods
source distribution.

Running `make` without parameters will generate the Davrods module .so
file in the `.libs` directory. `make install` will install the module
into Apache's modules directory.

After installing the module, copy the `davrods.conf` file to
`/etc/httpd/conf.modules.d/01-davrods.conf`.

Note: Non-Redhat platforms may have a different convention for the
location of the above file and the method for enabling/disabling
modules, consult the respective documentation for details.

Create a `irods` folder in a location where Apache HTTPD has read
access (e.g. `/etc/httpd/irods`). Place the provided
`irods_environment.json` file in this directory. For most setups, this
file can be used as is (but please read the __Configuration__ section).

Finally, set up httpd to serve Davrods where you want it to. An
example vhost config is provided for your convenience.


## Bugs and ToDos ##

- The Davrods root cannot currently start in a sub-directory (that is,
  `https://example.com/dav/` doesn't work, use
  `https://(dav.)example.com/` instead).

## Author ##

[Chris Smeele](https://github.com/cjsmeele)

## Contact information ##

For questions or support, contact Chris Smeele or Ton Smeele either
directly or via the
[Utrecht University RDM](http://www.uu.nl/en/research/research-data-management/contact-us)
page.

## License ##

Copyright (c) 2016, Utrecht University.

Davrods is licensed under the GNU Lesser General Public License version
3 or higher (LGPLv3+). See the COPYING.LESSER file for details.

The `lock_local.c` file was adapted from the source of `mod_dav_lock`,
a component of Apache HTTPD, and is used with permission granted by
the Apache License. See the copyright and license notices in this file
for details.
