#!/bin/bash

function clone(){
	if [ ! -e $1 ]; then
		git clone --depth 1 $@
	fi
}

function sync(){
	git --git-dir=${PWD}/$1/.git --work-tree=${PWD}/$1/ remote update
	git --git-dir=${PWD}/$1/.git --work-tree=${PWD}/$1/ checkout $2
}

clone zephyr git://sv-scm/git/ohsawa/zephyr.git
clone openthread https://github.com/openthread/openthread.git
sync openthread 301888
clone micropython git://sv-scm/git/ohsawa/micropython.git
sync micropython 20190117
