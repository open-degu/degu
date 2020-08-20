cmake_minimum_required(VERSION 3.8)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)


set(DEGU_VERSION_MAJOR "1")
set(DEGU_VERSION_MINOR "0")
set(DEGU_VERSION_REVISION "0-rc1")
configure_file("$ENV{ZEPHYR_BASE}/../version.h.in" "$ENV{ZEPHYR_BASE}/../version.h")

target_sources(app PRIVATE src/zephyr_start.c src/zephyr_getchar.c src/power.c)

zephyr_include_directories(
  $ENV{ZEPHYR_BASE}/../modules/lib/se_hostlib/inc
  $ENV{ZEPHYR_BASE}/../modules/lib/se_hostlib/common
  $ENV{ZEPHYR_BASE}/../modules/lib/se_hostlib/platform
  $ENV{ZEPHYR_BASE}/../modules/lib/se_hostlib/wrapper
  )

add_definitions(
  -DNO_SECURE_CHANNEL_SUPPORT
  -DZEPHYR
  -DFTR_FILE_SYSTEM
  -DTGT_A71CH
  -DI2C
  -DMBEDTLS
  -DSCP_MODE=NO_C_MAC_NO_C_ENC_NO_R_MAC_NO_R_ENC
  -DROUTER_ONLY
  )

add_library(libmicropython STATIC IMPORTED)
set_target_properties(libmicropython PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libmicropython.a)
target_link_libraries(app PUBLIC libmicropython)

zephyr_get_include_directories_for_lang_as_string(C includes)
zephyr_get_system_include_directories_for_lang_as_string(C system_includes)
zephyr_get_compile_definitions_for_lang_as_string(C definitions)
zephyr_get_compile_options_for_lang_as_string(C options)

add_custom_target(
  outputexports
  COMMAND echo CC="${CMAKE_C_COMPILER}"
  COMMAND echo Z_CFLAGS=${system_includes} ${includes} ${definitions} ${options}
  VERBATIM
  USES_TERMINAL
)
