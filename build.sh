#!/bin/bash -e

git submodule update --init --recursive

source ./env.sh
make
