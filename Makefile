.PHONY: dirs patch

OT_DIR = $(abspath ./openthread)
ZP_DIR = $(abspath ./zephyr)
MP_DIR = $(abspath ./micropython)
DEGU_DIR = $(CURDIR)

CMAKE_BUILD_TYPE := Debug #we still not release it.

all: patch mp

zp: dirs
	cmake -H. -Bbuild -GNinja -DBOARD=degu_evk -DEXTERNAL_PROJECT_PATH_OPENTHREAD=$(OT_DIR)  -DOVERLAY_CONFIG=overlay-ot.conf
	ninja -C build

mp:
	make -C micropython/ports/zephyr CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) BOARD=degu_evk EXTERNAL_PROJECT_PATH_OPENTHREAD=$(OT_DIR)

PATCH = $(wildcard $(DEGU_DIR)/patch/*.patch)

define apply_patch
	@patch -d$(ZP_DIR) -N -p1 < $(1); then echo 'patch alrady applied'; fi
endef

patch:
	$(foreach p, $(wildcard $(DEGU_DIR)/patch/*.patch), $(call apply_patch, $(p)))

clean:
	rm -rf build
	make -C micropython/ports/zephyr clean
	make -C openthread clean
