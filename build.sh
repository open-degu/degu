#!/bin/bash

git submodule update --init --recursive

source ./env.sh
make
