cmake_minimum_required(VERSION 3.20.0)

#CONSTANTES
add_definitions(-DSIGNATURE_SIZE=64 -DPRIVATE_KEY="my_private_key" -DPUBLIC_KEY="my_public_key")

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(crazyradio2)

# Definir o caminho onde a biblioteca sign_scheme foi compilada
set(SIGN_SCHEME_DIR "/home/souza/workspace/ita/andre/sign_scheme")

# Adicionar os headers da biblioteca
include_directories(${SIGN_SCHEME_DIR}/include)

# Linkar a biblioteca ao app
target_link_libraries(app PRIVATE ${SIGN_SCHEME_DIR}/build/libSignScheme.a)

include_directories(${SIGN_SCHEME_DIR}/install/include/)
include_directories(/usr/include)
include_directories(/usr/include/x86_64-linux-gnu)
link_directories(/usr/lib)

target_sources(app PRIVATE src/main.c src/esb.c src/crusb.c src/led.c src/rpc.c src/radio_mode.c src/api.c src/fem.c src/contwave.c src/button.c src/system.c src/power_measurement.c src/legacy_usb.c)
