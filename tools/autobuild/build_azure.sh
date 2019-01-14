#!/bin/bash
############################################################
set -e

docker/tools/onlbuilder
. setup.env
apt-cacher-ng
make amd64
