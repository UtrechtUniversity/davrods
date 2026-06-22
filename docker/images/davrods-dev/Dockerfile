FROM ubuntu:24.04
LABEL maintainer="Yoda team <yoda@uu.nl>"

# Network settings
EXPOSE 80
EXPOSE 443

# Application settings
ARG TAG=main
ENV IRODS_PACKAGE_VERSION="5.0.2-0~noble"
ENV APT_REPO_CONFIG="deb [arch=amd64] https://packages.irods.org/apt/ noble main"
ENV APT_IRODS_REPO_SIGNING_KEY_LOC="https://packages.irods.org/irods-signing-key.asc"
ENV DAVRODS_APT_URL="https://github.com/UtrechtUniversity/davrods/releases/download/5.0.2_1.5.4"
ENV DAVRODS_APT_PACKAGE="davrods-5.0.2-1.5.4.deb"
ENV DEBIAN_FRONTEND="noninteractive"

# Update packages
RUN apt-get update

# Install common tools
# hadolint ignore=DL3033
RUN apt-get install -y wget vim sudo netcat-traditional

# Install development tools
RUN apt-get install -y git vim cmake make gcc apache2 apache2-dev rpm libssl-dev

# Install and configure Apache
# hadolint ignore=DL3033
RUN apt-get install -y apache2 apache2-dev openssl
RUN ln -s /etc/apache2/mods-available/headers.load /etc/apache2/mods-enabled/headers.load
COPY apache2.conf /etc/apache2/apache2.conf
COPY ssl.conf /etc/apache2/mods-available/ssl.conf
RUN ln -s /etc/apache2/mods-available/ssl.load /etc/apache2/mods-enabled/ssl.load
RUN ln -s /etc/apache2/mods-available/dav.load /etc/apache2/mods-enabled/dav.load
RUN ln -s /etc/apache2/mods-available/dav_fs.load /etc/apache2/mods-enabled/dav_fs.load
RUN ln -s /etc/apache2/mods-available/dav_lock.load /etc/apache2/mods-enabled/dav_lock.load
RUN ln -s /etc/apache2/mods-available/davrods.load /etc/apache2/mods-enabled/davrods.load

# Install iRODS components: iCommands, runtime and development
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
# hadolint ignore=DL3033
RUN useradd -d /var/lib/irods irods && \
    chsh --shell /bin/bash irods && \
    wget -qO - "$APT_IRODS_REPO_SIGNING_KEY_LOC" | apt-key add - && \
    echo "$APT_REPO_CONFIG" | tee /etc/apt/sources.list.d/irods.list && \
    apt-get update && \
    apt-cache policy irods-runtime && \
    apt-cache policy irods-icommands && \
    apt-get -y install irods-runtime=${IRODS_PACKAGE_VERSION} irods-icommands=${IRODS_PACKAGE_VERSION} irods-dev=${IRODS_PACKAGE_VERSION}

# Copy DavRODS VHost and iRODS configuration files
COPY davrods-vhost.conf /etc/apache2/davrods-vhost.conf.orig
COPY davrods-anonymous-vhost.conf /etc/apache2/davrods-anonymous-vhost.conf.orig
RUN ln -s /etc/apache2/sites-available/davrods-vhost.conf /etc/apache2/sites-enabled/davrods-vhost.conf
RUN ln -s /etc/apache2/sites-available/davrods-anonymous-vhost.conf /etc/apache2/sites-enabled/davrods-anonymous-vhost.conf
COPY irods_environment.json /etc/apache2/irods/irods_environment.json.orig

# Initialize DavRODS lock database
COPY initialize-davrods-lockdb.py /usr/local/bin/initialize-davrods-lockdb.py
RUN mkdir /var/lib/davrods && \
    chmod 0755 /usr/local/bin/initialize-davrods-lockdb.py

# Copy pre-computed DH params
COPY ffdhe4096.pem /root/dhparams.pem

# Clone source repo
RUN cd /usr/src && \
    git clone https://github.com/UtrechtUniversity/davrods.git && \
    cd davrods && \
    git checkout "$TAG"

# Initialize
VOLUME [ "/sys/fs/cgroup" ]
COPY davrods_init.sh /var/lib/irods/scripts/davrods_init.sh
RUN chmod 0755 /var/lib/irods/scripts/davrods_init.sh
ENV TAG=${TAG}
CMD exec /var/lib/irods/scripts/davrods_init.sh $TAG
