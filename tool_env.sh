export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
./select_zephyr_sdk.sh
EXPORT_SDK=$(cat .cache_path)
export $EXPORT_SDK
