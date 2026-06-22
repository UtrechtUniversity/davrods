FROM ubuntu:24.04
LABEL maintainer="Yoda team <yoda@uu.nl>"
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Application settings
ARG TAG=irods-5
ENV IRODS_VERSION="5.0.2-0~noble"
ENV IRODS_PLUS_VERSION="5.0.1-0+5.0.2~noble"
ENV IRODS_MSVC_IRODS_VERSION="5.0.2"
ENV APT_REPO_CONFIG="deb [arch=amd64] https://packages.irods.org/apt/ noble main"
ENV APT_IRODS_REPO_SIGNING_KEY_LOC="https://packages.irods.org/irods-signing-key.asc"
ENV DEBIAN_FRONTEND="noninteractive"

# Network settings
EXPOSE 1247
EXPOSE 20000-20199

# Update packages
RUN apt-get update

# Install common tools
# hadolint ignore=DL3033
RUN apt-get install -y wget git sudo netcat-traditional gcc vim pwgen gettext-base

# Install rsyslog and configure it for logging iRODS messages
# hadolint ignore=DL3033
RUN apt-get install -y rsyslog
COPY irods-log-transform.py /usr/local/bin/irods-log-transform.py
RUN chmod +x /usr/local/bin/irods-log-transform.py
COPY rsyslogd-irods.conf /etc/rsyslog.d/40-irods.conf
COPY rsyslog.conf /etc/rsyslog.conf
RUN mkdir /var/lib/irods-log && chown syslog /var/lib/irods-log && chmod 0750 /var/lib/irods-log

# Install Python 3.12
# hadolint ignore=DL3033
RUN apt-get install -y gcc python3 python3-dev python3-pip python3.12-venv python3-virtualenv python3-wheel python3-magic python3-requests

# Install PostgreSQL 18 for ODBC drivers, checks whether database container is up, as well as troubleshooting
# hadolint ignore=DL3033
RUN wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add -
RUN echo "deb http://apt.postgresql.org/pub/repos/apt/ noble-pgdg main" | tee /etc/apt/sources.list.d/pgdg.list
RUN apt-get update
RUN apt-get -y install postgresql-18 unixodbc unixodbc-dev odbcinst python3-psycopg2 acl
COPY odbc-settings.txt /tmp/odbc-settings.txt
RUN odbcinst -i -d -r < /tmp/odbc-settings.txt

# Install iRODS
# hadolint ignore=DL3033,DL3047
RUN useradd -d /var/lib/irods irods && \
    wget -qO - "$APT_IRODS_REPO_SIGNING_KEY_LOC" | apt-key add - && \
    echo "$APT_REPO_CONFIG" | tee /etc/apt/sources.list.d/irods.list && \
    apt-get update && \
    for package in irods-runtime irods-icommands irods-server irods-database-plugin-postgres; \
    do echo "Installing package ${package} and its dependencies ..."; \
       apt-get -y install "$package=${IRODS_VERSION}" ; \
    done && \
    for package in irods-rule-engine-plugin-python irods-rule-engine-plugin-indexing irods-rule-engine-plugin-elasticsearch; \
    do echo "Installing package ${package} and its dependencies ..."; \
       apt-get -y install "$package=${IRODS_PLUS_VERSION}";  \
       done

# Remove default iRODS msiExecCMD files
# hadolint ignore=DL3059
RUN for file in irodsServerMonPerf test_execstream.py univMSSInterface.sh.template ; do rm "/var/lib/irods/msiExecCmd_bin/${file}" ; done

# Set up PAM authentication
# hadolint ignore=DL3033
RUN chmod 04755 /usr/sbin/irodsPamAuthCheck
COPY irods.pam /tmp/irods.pam
RUN install -m 0644 /tmp/irods.pam /etc/pam.d/irods

# Configure iRODS
COPY server_config.json /etc/irods/server_config.json
RUN mkdir /var/lib/irods/.irods

COPY core.py.template /etc/irods/core.py
COPY core.re.template /etc/irods/core.re
COPY core.dvm.template /etc/irods/core.dvm
COPY core.fnm.template /etc/irods/core.fnm

COPY service_account.config /tmp/service_account.config
RUN install -o irods -g irods -m 0644 /tmp/service_account.config /etc/irods/service_account.config
RUN mkdir /var/lib/irods/pregenerated-data && \
    chown irods:irods /var/lib/irods/pregenerated-data && \
    chmod 0750 /var/lib/irods/pregenerated-data

# Fix up file ownership iRODS files
RUN chown -R irods:irods /var/lib/irods /etc/irods

# Copy pre-computed DH params
COPY ffdhe4096.pem /root/dhparams.pem

# Create test accounts
# hadolint ignore=SC2016
RUN for user in researcher ; \
    do useradd -m -p '$6$rounds=656000$UYCy5.rN8/Nr5ooZ$.D2DtHyqjMIwQePZ8oGBXkEKTtOeyVPsSK1Kzn/DtIBojPh8ZGxOzVtlrRFMumtgcO7CWzgMAQqxJUORvGiFy0' "$user" ; \
    done

# Set login shell for irods account
RUN chsh --shell /bin/bash irods

# Install init script
# hadolint ignore=DL3033
COPY irods-icat-init.sh /var/lib/irods/scripts/irods-icat-init.sh
RUN chmod 0755 /var/lib/irods/scripts/irods-icat-init.sh

# Unattended install data
COPY provider-unattended-install.irods-5.0.template /tmp/provider-unattended-install.irods-5.0.template
COPY set-up-test-accounts.sh /root/set-up-test-accounts.sh
RUN chmod 0775 /root/set-up-test-accounts.sh

# Create vault directory
RUN install -d -o irods -g irods -m 0700 /var/lib/irods/Vault

VOLUME [ "/sys/fs/cgroup" ]
ENV TAG=${TAG}
CMD exec /var/lib/irods/scripts/irods-icat-init.sh $TAG
