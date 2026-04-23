cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(RPSopwith C CXX ASM)

add_executable(RPSopwith)

# XRAM Assets
rp6502_asset(RPSopwith 0x10000 images/player_bank_strip_4bpp.bin)
rp6502_asset(RPSopwith 0x11000 images/terrain_world_tilemap.bin)
rp6502_asset(RPSopwith 0x13BF2 images/terrain_tileset_4bpp.bin)

rp6502_executable(RPSopwith 
    DATA file 
    RESET file
)

target_sources(RPSopwith PRIVATE
    src/main.c
    src/sprite_mode5.c
    src/tile_mode2.c
)

