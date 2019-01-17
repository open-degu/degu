#!/bin/bash

function clone(){
	if [ ! -e $2 ]; then
		git clone $@
	fi
}

function sync(){
	git --git-dir=${PWD}/$1/.git --work-tree=${PWD}/$1/ remote update
	git --git-dir=${PWD}/$1/.git --work-tree=${PWD}/$1/ checkout $2
}

clone git://sv-scm/git/ohsawa/zephyr.git zephyr
clone  https://github.com/openthread/openthread.git openthread
sync openthread 301888
clone git://sv-scm/git/ohsawa/micropython.git micropython
sync micropython 20190117
