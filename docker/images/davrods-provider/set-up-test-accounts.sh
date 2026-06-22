#!/bin/bash
# Override test user passwords with user-defined values
for user in researcher
do usermod -p "$TEST_USER_PASSWORD_HASH" "$user"
  # Ignore failures in case accounts already exist
  sudo -iu irods iadmin mkuser "$user" rodsuser || true
done
sudo -iu irods ichmod read researcher /tempZone/home
