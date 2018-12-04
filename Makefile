.PHONY: dirs patch

OT_DIR = $(abspath ./openthread)
ZP_DIR = $(abspath ./zephyr)
MP_DIR = $(abspath ./micropython)
DEGU_DIR = $(CURDIR)

all: dirs patch mp zp

zp:
	cmake -H. -Bbuild -GNinja -DBOARD=degu_evk -DEXTERNAL_PROJECT_PATH_OPENTHREAD=$(OT_DIR)  -DOVERLAY_CONFIG=overlay-ot.conf
	ninja -C build

mp:
	make -C micropython/ports/zephyr BOARD=degu_evk

patch:
	patch -d$(ZP_DIR) -N -p1 < $(wildcard $(DEGU_DIR)/patch/*); then echo 'patch alrady applied'; fi
dirs:
	mkdir -p build

clean:
	rm -rf build
	make -C micropython/ports/zephyr clean
