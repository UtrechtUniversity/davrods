#!/bin/sh
export DOCKER_SCAN_SUGGEST=false
docker build . -t "ghcr.io/utrechtuniversity/davrods-dev:5.0.2_1.5.4" "$@"
