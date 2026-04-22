cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(MY-RP6502-PROJECT C CXX ASM)

option(SOPWITH_ENABLE_TERRAIN "Enable terrain simulation scaffolding" ON)
option(SOPWITH_ENABLE_FLIGHT "Enable flight simulation scaffolding" ON)
option(SOPWITH_ENABLE_COLLISIONS "Enable collision scaffolding" OFF)

add_executable(RPSopwith)
rp6502_asset(RPSopwith help src/help.txt)
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
