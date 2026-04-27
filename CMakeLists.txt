cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(RPSopwith C CXX ASM)

add_executable(RPSopwith)

# XRAM Assets
rp6502_asset(RPSopwith 0x10000 images/player_bank_strip_4bpp.bin)
rp6502_asset(RPSopwith 0x11800 images/terrain_world_tilemap.bin)
rp6502_asset(RPSopwith 0x143F2 images/terrain_tileset_4bpp.bin)
rp6502_asset(RPSopwith 0x163F2 images/targets_strip_4bpp.bin)
rp6502_asset(RPSopwith 0x172F2 images/projectiles_strip_4bpp.bin)
rp6502_asset(RPSopwith 0x18AD2 images/HUD_map.bin)
rp6502_asset(RPSopwith 0x18F82 images/HUD_tiles_4bpp.bin)
rp6502_asset(RPSopwith 0x1C8D4 images/ground_profile_3000.bin)

#ROM Assets
rp6502_asset(RPSopwith SOPWITH.BIN Music/SOPWITH.BIN) 

rp6502_executable(RPSopwith 
    DATA file 
    RESET file
)

target_sources(RPSopwith PRIVATE
    src/main.c
    src/music_player.c
    src/menu.c
    src/input.c
    src/flight.c
    src/enemy_planes.c
    src/ambient_flocks.c
    src/ambient_birds.c
    src/resources.c
    src/projectiles.c
    src/ground_targets.c
    src/minimap.c
    src/tile_mode2.c
    src/sprite_mode5.c
    src/text_mode1.c
)

