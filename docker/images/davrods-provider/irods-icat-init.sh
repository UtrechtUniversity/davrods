#!/bin/bash

set -e
set -o pipefail

DATA_VERSION="$1"

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
  before_update "Starting rsyslogd"
  sudo /usr/sbin/rsyslogd
  progress_update "Rsyslogd started"

  before_update "Starting iRODS"
  sudo su - irods -c 'irodsServer -d'
  until nc -z provider.davrods 1247
  do
      sleep 1
  done
  progress_update "iRODS started"

  before_update "Initializing authentication token"
  sudo -u irods bash -c "echo rods | iinit"

  progress_update "Authentication token initialized"

  progress_update "Container startup complete. iRODS is running."

  sleep infinity
}

# Generate keys
if [ -z "$ZONE_KEY" ]
then ZONE_KEY=$(pwgen -N 1 -n 16)
fi

if [ -z "$NEG_KEY" ]
then NEG_KEY=$(pwgen -N 1 -n 32)
fi

if [ -z "$CP_KEY" ]
then CP_KEY=$(pwgen -N 1 -n 32)
fi

PROVIDER_HOSTNAME=${PROVIDER_HOSTNAME:-provider.davrods}
ZONE_NAME=${ZONE_NAME:-tempZone}
ADMIN_PASSWORD=${ADMIN_PASSWORD:-rods}

# Generate and install self-signed certificates
cd /root
before_update "Generating self-signed certificate"
openssl req -x509 -newkey rsa:4096 -keyout docker.key -out docker.pem -days 3650 -nodes -subj "/C=NL/ST=UT/L=Utrecht/O=Utrecht University/OU=ITS/CN=provider.davrods"
install -m 0644 docker.pem /etc/irods/localhost_and_chain.crt
install -m 0644 docker.pem /etc/irods/localhost.crt
install -m 0644 docker.key /etc/irods/localhost.key
install -m 0644 dhparams.pem /etc/irods/dhparams.pem
progress_update "Certificate data generated"

# Wait for database container to become available
before_update "Waiting for PostgreSQL container to come up ..."
export PGPASSWORD=davrodsdev
while ! psql -U irods -d ICAT -h db.davrods -p 5432 -c 'SELECT 1' >& /dev/null ; do
  printf "."
  sleep 0.5
done
progress_update "PostgreSQL is now up."

# Check whether iRODS has already been set up
if psql -U irods -d ICAT -h db.davrods -p 5432 -tAc "SELECT 1 FROM R_ZONE_MAIN LIMIT 1;" 2>/dev/null | grep -q 1
then echo "Database already initialized. Starting service."
     start_service
fi

# Set up iRODS
export PROVIDER_HOSTNAME DB_PASSWORD ZONE_NAME ZONE_KEY NEG_KEY CP_KEY ADMIN_PASSWORD
envsubst < /tmp/provider-unattended-install.irods-5.0.template > /tmp/provider-unattended-install.irods-5.0.config
echo "provider.davrods" | python3 /var/lib/irods/scripts/setup_irods.py --json_configuration_file /tmp/provider-unattended-install.irods-5.0.config

INSTALL_TIMESTAMP=$(date -u +'%Y-%m-%dT%H:%M:%S.000000Z')
cat > /var/lib/irods/version.json << VERSION
{
    "catalog_schema_version": 12,
    "commit_id": "e16f20424192f24db2dcc3dbaaa95cee0334a2b6",
    "installation_time": "$INSTALL_TIMESTAMP",
    "irods_version": "5.0.2",
    "schema_name": "version",
    "schema_version": "v5"
}
VERSION
chown irods:irods /var/lib/irods/version.json
chown irods:irods /var/run/irods

start_service
