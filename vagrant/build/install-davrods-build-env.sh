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

function get_package_version()
{
     local package="$1"
     local IRODS_VERSION="$2"
     local distro="$3"
     package_version="$IRODS_VERSION"
}


if [ -f /etc/redhat-release ]
then

  echo "Installing DavRODS build environment on AlmaLinux."

  echo "Installing dependencies ..."
  sudo dnf -y install wget epel-release python3-dnf-plugin-versionlock

  echo "Importing repository signing key ..."
  sudo rpm --import "$YUM_IRODS_REPO_SIGNING_KEY_LOC"

  echo "Updating certificates for retrieving repository key ..."
  sudo dnf update -y ca-certificates

  echo "Adding iRODS repository ..."
  wget -qO - $YUM_REPO_FILE_LOC | sudo tee /etc/yum.repos.d/renci-irods.yum.repo

  for package in $DNF_IRODS_PACKAGES
  do echo "Installing package $package and its dependencies"
     get_package_version "$package" "$IRODS_VERSION" "almalinux"
     # $package_version is set by sourced function
     # shellcheck disable=SC2154
     sudo dnf -y install "$package-$package_version"
     sudo dnf versionlock add "$package"
  done

  for package in $DNF_GEN_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo dnf -y install "$package"
  done

elif lsb_release -i | grep -q Ubuntu
then

  echo "Installing DavRODS build environment on Ubuntu."

  echo "Downloading and installing iRODS repository signing key ..."
  wget -qO - "$APT_IRODS_REPO_SIGNING_KEY_LOC" | sudo apt-key add -

  echo "Adding iRODS repository ..."
cat << ENDAPTREPO | sudo tee /etc/apt/sources.list.d/irods.list
deb [arch=${APT_IRODS_REPO_ARCHITECTURE}] $APT_IRODS_REPO_URL $APT_IRODS_REPO_DISTRIBUTION $APT_IRODS_REPO_COMPONENT
ENDAPTREPO
  sudo apt-get update

  for package in $APT_IRODS_PACKAGES
  do echo "Installing package $package and its dependencies"
     get_package_version "$package" "$IRODS_VERSION" "ubuntu"
     sudo apt-get -y install "$package=$package_version"
     sudo apt-mark hold "$package"
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
