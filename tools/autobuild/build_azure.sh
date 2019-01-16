#!/bin/bash
############################################################
set -e

. setup.env
apt-cacher-ng
make amd64
