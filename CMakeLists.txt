cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/python")
    set(Python3_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/python" CACHE FILEPATH "Python interpreter" FORCE)
endif ()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

project(MY-RP6502-PROJECT C CXX ASM)

option(SOPWITH_ENABLE_TERRAIN "Enable terrain simulation scaffolding" ON)
option(SOPWITH_ENABLE_FLIGHT "Enable flight simulation scaffolding" ON)
option(SOPWITH_ENABLE_COLLISIONS "Enable collision scaffolding" OFF)

add_executable(RPSopwith)

set(PLAYER_STRIP_PNG ${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/player_bank_strip.png)
set(PLAYER_BIN ${CMAKE_CURRENT_BINARY_DIR}/assets/player_bank_strip_4bpp.bin)
set(PLAYER_PALETTE_BIN ${CMAKE_CURRENT_BINARY_DIR}/assets/player_bank_strip_4bpp_palette.bin)

add_custom_command(
    OUTPUT ${PLAYER_STRIP_PNG}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/generate_player_strip.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/generate_player_strip.py
)

add_custom_command(
    OUTPUT ${PLAYER_BIN} ${PLAYER_PALETTE_BIN}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/assets
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/convert_sprite.py
            ${PLAYER_STRIP_PNG}
            --mode tile
            --bpp 4
            --out-dir ${CMAKE_CURRENT_BINARY_DIR}/assets
            --extract-palette
    DEPENDS ${PLAYER_STRIP_PNG} ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/convert_sprite.py
)

rp6502_asset(RPSopwith help src/help.txt)
rp6502_asset(RPSopwith 0x12800 ${PLAYER_BIN})
rp6502_asset(RPSopwith 0x13800 ${PLAYER_PALETTE_BIN})
rp6502_executable(RPSopwith DATA file RESET file)
target_include_directories(RPSopwith PRIVATE src)
target_compile_definitions(RPSopwith PRIVATE
    SOPWITH_FEATURE_TERRAIN=$<BOOL:${SOPWITH_ENABLE_TERRAIN}>
    SOPWITH_FEATURE_FLIGHT=$<BOOL:${SOPWITH_ENABLE_FLIGHT}>
    SOPWITH_FEATURE_COLLISIONS=$<BOOL:${SOPWITH_ENABLE_COLLISIONS}>
)
target_sources(RPSopwith PRIVATE
    src/main.c
    src/platform/frame_sync.c
    src/platform/input.c
    src/platform/video.c
    src/game/game.c
    src/game/terrain/terrain.c
)
