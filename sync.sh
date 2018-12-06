#!/bin/bash

git clone --depth 1 git://sv-scm/git/takumi.ando/ryukyu/zephyr.git -b degu_evk_20181122
git clone https://github.com/openthread/openthread.git
git --git-dir=${PWD}/openthread/.git --work-tree=${PWD}/openthread/ checkout 30222e8
git clone --depth 1 git://sv-scm/git/ohsawa/micropython.git
