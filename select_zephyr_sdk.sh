#!/bin/bash

CACHE=".cache_path"

if [ -e ${CACHE} ]; then
   . ${CACHE}
   exit 0
fi

SDKS=$(find /opt $HOME -maxdepth 1 -name "zephyr-sdk*")

line=$(echo "$SDKS" | wc -l)

if [ $line -eq "1" ]; then
    PATH=$SDKS
else
    echo "$SDKS" | nl
    echo -n Select SDK path[1-${line}]:
    read ans
    if [ $ans -gt $line ]; then
	echo "ERROR: No SDK selected."
	exit 1
    fi
    PATH=$(echo "$SDKS"|sed -n ${ans}P)
fi

echo "ZEPHYR_SDK_INSTALL_DIR=${PATH}" > ${CACHE}
