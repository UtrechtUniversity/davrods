#!/bin/bash

set -e
set -o pipefail
set -u

DATA_VERSION="$1"
TAG="${TAG:-main}"

if [ -z "$DATA_VERSION" ]
then echo "Error: no data version argument provided."
     exit 1
fi

function before_update {
  echo -e "[...] ${1}"
}

function progress_update {
  GREEN='\033[0;32m'
  RESET='\033[0m'
  echo -e "[ ${GREEN}\xE2\x9C\x94${RESET} ] ${1}"
}

function start_service {
  apache2ctl -D FOREGROUND || true
  echo "Error: Apache either terminated or would not start. Keeping container running for troubleshooting purposes."
  sleep infinity
}

if [ -f "/container_initialized" ]
then echo "Container has already been initialized. Starting service."
     start_service
fi

# Generate and install self-signed certificates
cd /root
before_update "Generating self-signed certificate"
openssl req -x509 -newkey rsa:4096 -keyout docker.key -out docker.pem -days 3650 -nodes -subj "/C=NL/ST=UT/L=Utrecht/O=Utrecht University/OU=ITS/CN=data.davrods" -addext "subjectAltName=DNS:public.data.davrods"
install -m 0644 docker.pem /etc/ssl/certs/localhost.crt
install -m 0644 docker.pem /etc/ssl/certs/localhost_and_chain.crt
install -m 0644 docker.key /etc/ssl/private/localhost.key
install -m 0644 dhparams.pem /etc/ssl/private/dhparams.pem
progress_update "Certificate data generated"

# Initialize lock database
if [[ -f "/var/lib/davrods/lockdb_locallock" ]]
then progress_update "Lock database has already been initialized"
else before_update "Initializing lock database"
     /usr/local/bin/initialize-davrods-lockdb.py /var/lib/davrods
     chown -R www-data:www-data /var/lib/davrods
     chmod 0775 /var/lib/davrods
     progress_update "Lock database initialized"
fi

# Compile davrods
before_update "Building Davrods"
cd /usr/src/davrods
git fetch
git checkout "$TAG"
if ! [[ -d "build" ]]
then mkdir build
fi
cd build
cmake ..
make
progress_update "Building Davrods complete"

# Install built DavRODS version
before_update "Installing DavRODS"
make install
chown www-data:www-data /var/lib/davrods
chmod 0700 /var/lib/davrods
progress_update "Installing Davrods complete"

# Restoring Docker setup specific Vhost files
before_update "Restoring custom Vhost files after installation"
cp /etc/apache2/davrods-vhost.conf.orig /etc/apache2/sites-available/davrods-vhost.conf
cp /etc/apache2/davrods-anonymous-vhost.conf.orig /etc/apache2/sites-available/davrods-anonymous-vhost.conf
cp /etc/apache2/irods/irods_environment.json.orig /etc/apache2/irods/irods_environment.json
progress_update "Custom vhost files restored"

# Wait for iRODS provider to become available
until nc -vz provider.davrods 1247 > /dev/null 2>&1
do before_update "Waiting until iRODS provider is reachable from Davrods container ..."
   sleep 1
done
progress_update "iRODS provider is online"

# Start Apache
touch /container_initialized
before_update "Initialization complete. Starting Apache"
start_service
