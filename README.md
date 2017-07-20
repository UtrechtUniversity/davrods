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

## Download ##

There are currently two supported Davrods versions:

- [`davrods-4.1_1.1.1`](https://github.com/UtrechtUniversity/davrods/releases/tag/4.1_1.1.1), branch `irods-4.1-libs`
- [`davrods-4.2_1.1.1`](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2_1.1.1), branch `master`

The left side of the version number indicates the version of the iRODS
client libraries that Davrods uses.

When installing Davrods on the same machine as an iRODS server, the
version of Davrods must match the iRODS version. Otherwise, both Davrods
versions listed above will work with either iRODS server version.

## Installation ##

This section describes the installation steps for `davrods-4.2_1.1`.

To view instructions for `davrods-4.1_1.1`, switch to the
[`irods-4.1-libs`](https://github.com/UtrechtUniversity/davrods/tree/irods-4.1-libs)
branch.

### Using the binary distribution ###

Davrods depends on certain iRODS packages, which as of iRODS 4.2 are
distributed at https://packages.irods.org/

After following the instructions for adding the iRODS repository to your
package manager at the link above, Davrods can be installed as a binary
package using the RPM on the [releases page](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2_1.1.1).

Download the Davrods package for your platform and install it using your
package manager, for example:

    yum install davrods-4.2_1.1.1-1.el7.centos.x86_64.rpm

We currently distribute RPM packages for CentOS 7 only.
If you require packaging for a different platform, please contact us
by creating an issue.

See the __Configuration__ section for instructions on how to configure
Davrods once it has been installed.

### Davrods and SELinux ##

If the machine on which you install Davrods is protected by SELinux,
you may need to make changes to your policies to allow Davrods to run:

- Apache HTTPD must be allowed to connect to TCP port 1247

For example, the following command can be used to resolve this
requirement:

    setsebool -P httpd_can_network_connect true

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

- `cmake`
- `make`
- `gcc`
- `httpd-devel >= 2.4`
- `irods-devel >= 4.2.0`
- `openssl-devel`
- `irods-runtime >= 4.2.0`
- `rpmdevtools` (if you are creating an RPM)

Additionally, the following runtime dependencies must be installed:

- `irods-runtime >= 4.2.0`
- `openssl-libs`
- `httpd >= 2.4`

Follow these instructions to build from source:

- First, browse to the directory where you have unpacked the Davrods
  source distribution.

- Check whether your umask is set to a sane value. If the output of
  `umask` is not `0022`, run `umask 0022` to fix it. This is important
  for avoiding conflicts in created packages later on.

- Create and generate a build directory.

```bash
mkdir build
cd build
cmake ..
```

- Compile the project

```bash
make
```

Now you can either build an RPM or install the project without a package
manager. Packaging for Linux distributions other than CentOS-likes is
not yet supported.

**To create a package:**

```
make package
```

That's it, you should now have an RPM in your build directory which you
can install using yum.

**To install without a package manager on CentOS:**

Run the following as user root:

```
make install
chown apache:apache /var/lib/davrods
chmod 700 /var/lib/davrods
```

**To install without a package manager on other distros:**

Distributions other than CentOS (e.g. Ubuntu) have different HTTPD
configuration layouts, which are not yet supported by the build system.
For this reason you will need to install the files manually:

- Copy `mod_davrods.so` to your Apache module directory.
- Copy `davrods.conf` to your Apache module configuration/load directory.
- Copy `davrods-vhost.conf` to your Apache vhost configuration directory.
- Create an `irods` directory in a location where Apache HTTPD has read
  access.
- Copy `irods_environment.json` to the `irods` directory.
- Create directory `/var/lib/davrods`, and give apache exclusive access
  to it: `chown apache:apache /var/lib/davrods; chmod 700 /var/lib/davrods`

## Bugs and ToDos ##

Please report any issues you encounter on the
[issues page](https://github.com/UtrechtUniversity/davrods/issues).

## Author ##

[Chris Smeele](https://github.com/cjsmeele)

## Contact information ##

For questions or support, contact Chris Smeele or Ton Smeele either
directly or via the
[Utrecht University RDM](http://www.uu.nl/en/research/research-data-management/contact-us)
page.

## License ##

Copyright (c) 2016, 2017, Utrecht University.

Davrods is licensed under the GNU Lesser General Public License version
3 or higher (LGPLv3+). See the COPYING.LESSER file for details.

The `lock_local.c` file was adapted from the source of `mod_dav_lock`,
a component of Apache HTTPD, and is used with permission granted by
the Apache License. See the copyright and license notices in this file
for details.
