#!/bin/bash

#TODO: it mey "real" directory creation by repo
#ln -sr ../szephyr .
git clone --depth 1 git://sv-scm/git/takumi.ando/ryukyu/zephyr.git -b degu_evk_20181122
ln -sr ../openthread .
ln -sr ../micropython 
