#!/bin/bash
# shellcheck disable=SC1090

set -e
set -o pipefail
set -u

export DEBIAN_FRONTEND=noninteractive

SETTINGSFILE=${1:-./local-repo.env}

if [ -f "$SETTINGSFILE" ]
then source "$SETTINGSFILE"
else echo "Error: settings file $SETTINGSFILE not found." && exit 1
fi

if [ -f /etc/centos-release ]
then

  echo "Installing DavRODS build environment on CentOS."

  echo "Installing dependencies ..."
  sudo yum -y install wget epel-release yum-plugin-versionlock

  echo "Importing repository signing key ..."
  sudo rpm --import "$YUM_IRODS_REPO_SIGNING_KEY_LOC"

  echo "Updating certificates for retrieving repository key ..."
  sudo yum update -y ca-certificates

  echo "Adding iRODS repository ..."
  wget -qO - https://packages.irods.org/renci-irods.yum.repo | sudo tee /etc/yum.repos.d/renci-irods.yum.repo

  for package in $YUM_IRODS_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo yum -y install "$package-${IRODS_VERSION}"
     sudo yum versionlock "$package"
  done

  for package in $YUM_GEN_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo yum -y install "$package"
  done

elif lsb_release -i | grep -q Ubuntu
then

  echo "Installing DavRODS build environment on Ubuntu."

  echo "Installing dependencies ..."
  sudo apt-get -y install aptitude

  echo "Downloading and installing iRODS repository signing key ..."
  wget -qO - "$APT_IRODS_REPO_SIGNING_KEY_LOC" | sudo apt-key add -

  echo "Adding iRODS repository ..."
cat << ENDAPTREPO | sudo tee /etc/apt/sources.list.d/irods.list
deb [arch=${APT_IRODS_REPO_ARCHITECTURE}] $APT_IRODS_REPO_URL $APT_IRODS_REPO_DISTRIBUTION $APT_IRODS_REPO_COMPONENT
ENDAPTREPO
  sudo apt-get update

  for package in $APT_IRODS_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo apt-get -y install "$package=${IRODS_VERSION}"
     sudo aptitude hold "$package"
  done

  for package in $APT_GEN_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo apt-get -y install "$package"
  done

else
  echo "Error: install script is not suitable for this box."

fi

git clone https://github.com/UtrechtUniversity/davrods.git
mkdir davrods/build
chown -R vagrant:vagrant davrods
