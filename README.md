Davrods - An Apache WebDAV interface to iRODS
=============================================

Davrods provides access to iRODS servers using the WebDAV protocol.
It is a bridge between the WebDAV protocol and the iRODS API,
implemented as an Apache HTTPD module.

Davrods leverages the Apache server implementation of the WebDAV
protocol, `mod_dav`, for compliance with the WebDAV Class 2 standard.

Notable features include:

- WebDAV Class 2 support, with locks local to the Apache server
- Connects to iRODS server versions 4+
- PAM and Native (a.k.a. STANDARD) iRODS authentication
- Optional negotiated SSL encryption for the entire iRODS connection
- Configurable using Apache configuration directives
- Optional anonymous access mode for password-less public access
- Themeable directory listings for read-only web browser access.
- Partial file up- and downloads and resumes (HTTP byte-ranges)
- iRODS ticket-based access

Themeable listings and anonymous access were inspired by Simon Tyrrell's
[work](https://github.com/billyfish/eirods-dav) at Earlham Institute.

## Download ##

Please choose the right version for your platform:

1. If you run Davrods on the same server as your iRODS service, you need a
   Davrods version built against the same version iRODS *runtime*.
2. If you run Davrods separately, on its own server, then the iRODS runtime
   version does not matter - just pick the newest Davrods you can get.
   All Davrods packages below should be compatible with any iRODS 4.x
   server version.

| Date        | Davrods ver. | iRODS runtime ver. | Packages                                                                          | Remarks
| ----------- | ------------ | ------------------ | --------------------------------------------------------------------------------- | ---------------- |
| 2022-06-22  | 1.5.0        | 4.3.0              | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.3.0_1.5.0) | Pre-release      |
| 2023-07-27  | 1.5.1        | 4.2.12             | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.12_1.5.1)| Pre-release      |
| 2021-12-20  | 1.5.0        | 4.2.11             | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.11_1.5.0)|                  |
| 2021-07-28  | 1.5.0        | 4.2.10             | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.10_1.5.0)|                  |
| 2021-06-16  | 1.5.0        | 4.2.9              | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.9_1.5.0) |                  |
| 2020-06-02  | 1.5.0        | 4.2.8              | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.8_1.5.0) |                  |
| 2019-12-20  | 1.4.2        | 4.2.7              | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.7_1.4.2) |                  |
| 2019-06-20  | 1.4.2        | 4.2.6              | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.6_1.4.2) |                  |
| 2019-04-03  | 1.4.2        | 4.2.5              | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.5_1.4.2) |                  |
| 2018-12-17  | 1.4.2        | 4.2.4              | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.4_1.4.2) |                  |
| 2018-07-15  | 1.4.1        | 4.2.3              | [RPM, DEB](https://github.com/UtrechtUniversity/davrods/releases/tag/4.2.3_1.4.1) |                  |
| 2018-07-15  | 1.4.1        | 4.1.x              | [RPM](https://github.com/UtrechtUniversity/davrods/releases/tag/4.1_1.4.1)        |                  |

(older versions are still available on the
[releases page](https://github.com/UtrechtUniversity/davrods/releases))

If you require a certain Davrods/iRODS runtime version combination that
is not listed above, you can most likely still build it yourself (see
"Building from source").

A log describing which features were added and which bugs were fixed in
each version can be found in [changelog.txt](changelog.txt).

We currently distribute RPM packages for AlmaLinux 9 & RHEL systems and
DEB packages for Debian & Ubuntu systems.
We test our packages on AlamaLinux 9 and Ubuntu 24.04.

## Installation ##

This section describes the installation steps for iRODS 4.2+ based
Davrods releases.

To view instructions for iRODS 4.1-based Davrods releases, switch to the
[`irods-4.1-libs`](https://github.com/UtrechtUniversity/davrods/tree/irods-4.1-libs)
branch.

### Using the binary distribution ###

Davrods depends on certain iRODS packages, which as of iRODS 4.2 are
distributed at https://packages.irods.org/

After following the instructions for adding the iRODS repository to your
package manager at the link above, Davrods can be installed as a binary
package using the RPM or DEB file from the
[releases page](https://github.com/UtrechtUniversity/davrods/releases)
(use the table near the top of this README to select the right version).

Download the Davrods package for your platform and install it using your
package manager, for example:

    yum install davrods-4.2.8_1.5.0-1.rpm
    --or--
    apt install davrods-4.2.8_1.5.0.deb

Now see the __Configuration__ section for instructions on how to
configure Davrods once it has been installed.

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

The Davrods RPM distribution installs two vhost template files:

1. `/etc/httpd/conf.d/davrods-vhost.conf`
2. `/etc/httpd/conf.d/davrods-anonymous-vhost.conf`

(for Ubuntu, replace `/etc/httpd/conf.d` with
`/etc/apache2/sites-available`)

These files are provided completely commented out. To enable either
configuration, simply remove the first column of `#` signs, and then
tune the settings to your needs.

Note that on Ubuntu, you will additionally need to enable the Davrods
module and vhosts, like so:

    a2enmod dav
    a2enmod davrods
    a2ensite davrods-vhost
    a2ensite davrods-anonymous-vhost

The normal vhost configuration (1) provides sane defaults for
authenticated access.

The anonymous vhost configuration (2) allows password-less public
access using the `anonymous` iRODS account.

You can enable both configurations simultaneously, as long as their
ServerName values are unique (for example, you might use
`dav.example.com` for authenticated access and
`public.dav.example.com` for anonymous access).

### The iRODS environment file ###

The binary distribution installs the `irods_environment.json` file in
`/etc/httpd/irods`. In most iRODS setups, this file can be used as
is.

Importantly, options such as `irods_host` and `irods_zone_name` are
**not** read from this file (and are omitted for that reason).
These settings are taken from their equivalent Davrods configuration
directives in the vhost config file instead.

Options in `irods_environment.json` that are known to affect Davrods
behavior are the negotiation, ssl and encryption settings.

See the official documentation for more information on these settings:
https://docs.irods.org/4.2.7/system_overview/configuration/#irodsirods_environmentjson

### Ticket-based access ###

Ticket access is disabled by default, and requires special configuration
to enable, depending on your use case. Please see
[README.advanced.md](./README.advanced.md) for more information.

## Building from source ##

This repository includes a Vagrant configuration for building Davrods from source
on either AlmaLinux 9 (for the RPM package) or Ubuntu 24.04 LTS (for the DEB package).
It can be found in `vagrant/build`. In order to build a package using Vagrant, edit
the .env file in the Vagrant build directory. Adjust the BOXNAME and IRODS_VERSION vars
as needed. Then run `vagrant up` to provision the VM. The VM has all dependencies
pre-installed, as well as a clone of the Davrods repository. Log in on the VM
using `vagrant ssh`, then generate the build directory and create the package (see below).

To build from source without using the Vagrant configuration, the following build-time
dependencies must be installed (package names may differ on your platform):

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

Now you can either build an RPM/DEB or install the project without a
package manager.

**To create a package:**

```
make package
```

That's it, you should now have an RPM or DEB in your build directory
which you can install using yum or apt.

**To install without a package manager on AlmaLinux:**

Run the following as user root:

```
make install
chown apache:apache /var/lib/davrods
chmod 700 /var/lib/davrods
```

**To install without a package manager on Debian:**

Run the following as user root:

```
make install
chown www-data:www-data /var/lib/davrods
chmod 700 /var/lib/davrods
```

**To install without a package manager on other distros:**

Linux distributions other than RHEL, Debian and their derivatives may
have different HTTPD configuration and directory layouts, which are not
currently supported by the build system.
For this reason you will need to install the files manually on such
Linux distributions:

- Copy `mod_davrods.so` to your Apache module directory.
- Copy `davrods.conf` to your Apache module configuration/load directory.
- Copy `davrods-vhost.conf` and `davrods-anonymous-vhost.conf` to your
  Apache vhost configuration directory.
- Create an `irods` directory in a location where Apache HTTPD has read
  access.
- Copy `irods_environment.json` to the `irods` directory.
- Create directory `/var/lib/davrods`, and give apache exclusive access
  to it.

## Bugs and feature requests ##

Please report any issues you encounter on the
[issues page](https://github.com/UtrechtUniversity/davrods/issues).

## Contact information ##

For questions or support, please contact the Yoda team at yoda@uu.nl.

## Authors ##

- [Chris Smeele](https://github.com/cjsmeele)

## License ##

Copyright (c) 2016 - 2025, Utrecht University.

Davrods is licensed under the GNU Lesser General Public License version
3 or higher (LGPLv3+). See the COPYING.LESSER file for details.

The `lock_local.c` and `byterange.c` files were adapted from components
of Apache HTTPD, and are used with permission granted by the Apache
License. See the copyright and license notices in these files for
details.
