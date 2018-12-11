#!/bin/bash

git clone --depth 1 git://sv-scm/git/ohsawa/zephyr.git
git clone https://github.com/openthread/openthread.git
git --git-dir=${PWD}/openthread/.git --work-tree=${PWD}/openthread/ checkout 301888
git clone --depth 1 git://sv-scm/git/ohsawa/micropython.git
