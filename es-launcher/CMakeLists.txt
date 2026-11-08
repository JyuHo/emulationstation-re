# SPDX-License-Identifier: LGPL-3.0-or-later
#
# es-launcher: configures emulators and builds the command that starts a game.
# It is licensed under the LGPL 3.0 or later (see COPYING.LESSER), unlike the rest of
# EmulationStation-RE (MIT), because its generators are adapted from batocera-launch.
# It is built as a shared library so that ES only links to it dynamically.
#
# It depends on nothing from es-core or es-app: ES fills a LaunchContext and calls it.
# It can also be built on its own: cmake -S es-launcher -B build-launcher

cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PROJECT_NAME)
    project(es-launcher LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(GNUInstallDirs)
    set(ES_LAUNCHER_STANDALONE ON)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../CMake/Packages)
    # shadPS4's settings are JSON: RapidJSON, header-only, which ES uses too
    find_package(RapidJSON REQUIRED)
endif()

option(ES_LAUNCHER_TESTS "Build the es-launcher tests" ${ES_LAUNCHER_STANDALONE})

# The sources are compiled once, into an object library that both the shared library and the
# tests use, so that the tests can reach the internal classes the library does not export.
add_library(es-launcher-objects OBJECT
    src/Command.cpp
    src/EvdevLayout.cpp
    src/Generator.cpp
    src/LaunchContext.cpp
    src/Launcher.cpp
    src/Settings.cpp
    src/util/EmulatorCommand.cpp
    src/util/Files.cpp
    src/util/Gamepad.cpp
    src/util/IniFile.cpp
    src/util/YamlFile.cpp
    src/retroarch/ConfigFile.cpp
    src/retroarch/Cores.cpp
    src/retroarch/RetroArchGenerator.cpp
    src/duckstation/DuckStationGenerator.cpp
    src/dolphin/DolphinGenerator.cpp
    src/pcsx2/Pcsx2Generator.cpp
    src/ppsspp/PpssppGenerator.cpp
    src/azahar/AzaharGenerator.cpp
    src/cemu/CemuGenerator.cpp
    src/xemu/XemuGenerator.cpp
    src/xenia/XeniaGenerator.cpp
    src/rpcs3/Rpcs3Generator.cpp
    src/eden/EdenGenerator.cpp
    src/melonds/MelonDSGenerator.cpp
    src/mupen64plus/Mupen64PlusGenerator.cpp
    src/dosbox/DosboxStagingGenerator.cpp
    src/scummvm/ScummVMGenerator.cpp
    src/stella/StellaGenerator.cpp
    src/supermodel/SupermodelGenerator.cpp
    src/ymir/YmirGenerator.cpp
    src/vita3k/Vita3KGenerator.cpp
    src/shadps4/ShadPS4Generator.cpp
    src/redream/RedreamGenerator.cpp
    src/hatari/HatariGenerator.cpp
    src/vice/ViceGenerator.cpp
    src/openmsx/OpenMSXGenerator.cpp
    # Cemu's settings are XML: the copy of pugixml ES ships, built in with hidden symbols
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/pugixml/src/pugixml.cpp
)

target_include_directories(es-launcher-objects
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/../external/pugixml/src ${RAPIDJSON_INCLUDE_DIRS}
)
target_compile_definitions(es-launcher-objects PRIVATE ES_LAUNCHER_BUILDING)
target_compile_options(es-launcher-objects PRIVATE -Wall -Wextra)
set_target_properties(es-launcher-objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(es-launcher SHARED $<TARGET_OBJECTS:es-launcher-objects>)
target_include_directories(es-launcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(es-launcher PROPERTIES
    VERSION 0.1.0
    SOVERSION 0
)

install(TARGETS es-launcher LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(ES_LAUNCHER_TESTS)
    enable_testing()

    # the generators register themselves from static initializers: link every object file,
    # even those no test function refers to
    add_executable(es-launcher-tests
        tests/launcher_tests.cpp
        tests/retroarch_tests.cpp
        tests/standalone_tests.cpp
        tests/main.cpp
        $<TARGET_OBJECTS:es-launcher-objects>
    )
    target_include_directories(es-launcher-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../external/pugixml/src
        ${RAPIDJSON_INCLUDE_DIRS}
    )
    target_compile_options(es-launcher-tests PRIVATE -Wall -Wextra)
    add_test(NAME es-launcher-tests COMMAND es-launcher-tests)
endif()
