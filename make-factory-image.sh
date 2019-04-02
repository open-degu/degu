#!/bin/sh

MCUBOOT=$1
DEGU=$2
OUT=$3

check_ret ()
{
	if [ $? -ne 0 ]; then
		echo "Stop."
		exit 1
	fi
}

usage ()
{
	echo "make-factory-image
Usage: $0 bootloader firmware out

Make binary image for factory as degu-factory-firmware.bin .

args:
    bootloader - MCUboot binary (not .hex)
    firmware   - Degu firmware binary (not .hex)
    out        - Factory image output"
}

filesize ()
{
	wc -c $1 | awk '{print $1}'
}

pad ()
{
	REGION_SIZE=$1
	FILE=$2

	REGION_SIZE=`printf '%d' ${REGION_SIZE}`

	if [ -e ${FILE} ]; then
		FILE_SIZE=`filesize ${FILE}`
		PAD_SIZE=$(( REGION_SIZE - FILE_SIZE ))
		if [ ${PAD_SIZE} -lt 0 ]; then
			echo "Too large file:" ${FILE}
			return 1
		fi
	else
		PAD_SIZE=${REGION_SIZE}
	fi

	if [ "$3" = "magic" ]; then
		PAD_SIZE=$(( PAD_SIZE - 16 ))
	fi

	dd if=/dev/zero ibs=1 count=${PAD_SIZE} | tr "\000" "\377" > .pad
	if [ "$3" = "magic" ]; then
		cat .pad dat/img_magic.dat > .tmp
		mv .tmp .pad
	fi

	if [ -e ${FILE} ]; then
		cat ${FILE} .pad > .${FILE}.pad
	else
		cp .pad .${FILE}.pad
	fi

	rm .pad
	return 0
}

if [ $# -lt 3 ]; then
	usage
	exit 1
fi

case $1 in
	"-h" | "--help" )
		usage
		return 1
		;;
	* )
		pad 0x14000 ${MCUBOOT}
		check_ret
		pad 0x6e000 ${DEGU} magic
		check_ret
		pad 0x6e000 slot1
		check_ret
		pad 0x8000 scratch
		check_ret

		cat .${MCUBOOT}.pad .${DEGU}.pad .slot1.pad .scratch.pad dat/fat12.dat > ${OUT}
		echo "Done."

		rm .*.pad
		;;
esac

return 0
