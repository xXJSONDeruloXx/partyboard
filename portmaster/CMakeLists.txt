cmake_minimum_required(VERSION 3.16)

project(partyboard_portmaster_probe LANGUAGES C)

enable_language(CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# The target libraries are borrowed from the device only for linking.  The
# installed launcher must resolve them from the CFW runtime, never from a
# host build path.
set(CMAKE_SKIP_RPATH ON)

set(PM_TARGET_LIB_DIR "" CACHE PATH "Directory containing target SDL2/EGL/GLES libraries")
set(PM_SDL2_INCLUDE_DIR "" CACHE PATH "Directory containing the SDL2 include tree")
set(PM_GLES2_INCLUDE_DIR "" CACHE PATH "Directory containing GLES2/gl2.h")

find_path(PM_SDL2_INCLUDE_PATH SDL2/SDL.h
    HINTS ${PM_SDL2_INCLUDE_DIR}
    PATHS /usr/include /usr/local/include
    NO_CMAKE_FIND_ROOT_PATH
)
find_path(PM_GLES2_INCLUDE_PATH GLES2/gl2.h
    HINTS ${PM_GLES2_INCLUDE_DIR}
    PATHS /usr/include /usr/local/include
    NO_CMAKE_FIND_ROOT_PATH
)
find_path(PM_GLES3_INCLUDE_PATH GLES3/gl32.h
    HINTS ${PM_GLES2_INCLUDE_DIR}
    PATHS /usr/include /usr/local/include
    NO_CMAKE_FIND_ROOT_PATH
)

if(PM_TARGET_LIB_DIR)
    find_library(PM_SDL2_LIBRARY NAMES SDL2 SDL2-2.0
        PATHS ${PM_TARGET_LIB_DIR}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
    find_library(PM_EGL_LIBRARY NAMES EGL
        PATHS ${PM_TARGET_LIB_DIR}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
    find_library(PM_GLES2_LIBRARY NAMES GLESv2 GLESv2_CM
        PATHS ${PM_TARGET_LIB_DIR}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
else()
    find_library(PM_SDL2_LIBRARY NAMES SDL2 SDL2-2.0)
    find_library(PM_EGL_LIBRARY NAMES EGL)
    find_library(PM_GLES2_LIBRARY NAMES GLESv2 GLESv2_CM)
endif()

if(NOT PM_SDL2_INCLUDE_PATH OR NOT PM_GLES2_INCLUDE_PATH OR NOT PM_GLES3_INCLUDE_PATH)
    message(FATAL_ERROR "SDL2, GLES2, and GLES3.2 development headers are required")
endif()
if(NOT PM_SDL2_LIBRARY OR NOT PM_EGL_LIBRARY OR NOT PM_GLES2_LIBRARY)
    message(FATAL_ERROR "SDL2, EGL, and GLESv2 target libraries are required")
endif()

add_executable(partyboard-portmaster-probe src/bringup.c)
target_include_directories(partyboard-portmaster-probe PRIVATE
    ${PM_SDL2_INCLUDE_PATH}
    ${PM_GLES2_INCLUDE_PATH}
)
target_compile_definitions(partyboard-portmaster-probe PRIVATE
    PM_PROBE_NAME="Party Board PortMaster bring-up"
)
target_link_libraries(partyboard-portmaster-probe PRIVATE
    ${PM_SDL2_LIBRARY}
    ${PM_EGL_LIBRARY}
    ${PM_GLES2_LIBRARY}
    m
    dl
    pthread
)

add_executable(partyboard-portmaster-gx-probe
    src/gx_probe.c
    src/pc/pc_gx.c
    src/pc/pc_gx_tev.c
    src/pc/pc_gx_texture.c
    src/pc/pc_prof.c
)
target_compile_definitions(partyboard-portmaster-gx-probe PRIVATE
    TARGET_PC
    PC_USE_GLES
)
target_include_directories(partyboard-portmaster-gx-probe PRIVATE
    ${PM_SDL2_INCLUDE_PATH}
    ${PM_GLES2_INCLUDE_PATH}
    ${PM_GLES3_INCLUDE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/pc
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include
)
target_link_libraries(partyboard-portmaster-gx-probe PRIVATE
    ${PM_SDL2_LIBRARY}
    ${PM_EGL_LIBRARY}
    ${PM_GLES2_LIBRARY}
    m
    dl
    pthread
)
add_custom_command(TARGET partyboard-portmaster-gx-probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:partyboard-portmaster-gx-probe>/shaders
)

install(TARGETS partyboard-portmaster-probe partyboard-portmaster-gx-probe RUNTIME DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION .)

# Compile the decompilation against the handheld platform layer without
# pulling Aurora's SDL3/Dawn desktop runtime into the PortMaster binary.  This
# object target is intentionally separate from the eventual executable while
# the platform replacements are brought up one subsystem at a time.
include(${CMAKE_CURRENT_SOURCE_DIR}/../files.cmake)
set(PM_GAME_C_FILES)
foreach(game_file ${GAME_FILES})
    list(APPEND PM_GAME_C_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../${game_file})
endforeach()

add_library(partyboard-portmaster-game-objects OBJECT ${PM_GAME_C_FILES})
target_compile_definitions(partyboard-portmaster-game-objects PRIVATE
    TARGET_PC
    PARTY_BOARD_PORTMASTER
    NON_MATCHING
    OPTIMIZED_TEXTURE_LOADING
    BYTESWAPPING
    TARGET_DOL
    PC_USE_GLES
    VERSION=1
    MUSY_TARGET=MUSY_TARGET_PC
    MUSY_VERSION_MAJOR=1
    MUSY_VERSION_MINOR=5
    MUSY_VERSION_PATCH=4
)
target_include_directories(partyboard-portmaster-game-objects PRIVATE
    ${PM_SDL2_INCLUDE_PATH}
    ${PM_GLES2_INCLUDE_PATH}
    ${PM_GLES3_INCLUDE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/pc
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/musyx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include/dolphin
)

# The original game loads each REL overlay at runtime.  Build the decompiled
# overlay sources as ordinary ELF shared objects and let the main executable
# provide the common game/platform symbols, matching the desktop loader model.
set(PM_REL_TARGETS)
set(PM_REL_BINARY_TARGETS)
foreach(rel_file ${REL_FILES})
    get_filename_component(rel_dir ${rel_file} DIRECTORY)
    get_filename_component(rel_target ${rel_dir} NAME)
    list(APPEND PM_REL_TARGETS ${rel_target})
endforeach()
list(REMOVE_DUPLICATES PM_REL_TARGETS)

foreach(rel_target ${PM_REL_TARGETS})
    set(PM_REL_SOURCE_FILES ${REL_FILES})
    list(FILTER PM_REL_SOURCE_FILES INCLUDE REGEX "^src/REL/${rel_target}/.*\\.c$")
    if(PM_REL_SOURCE_FILES)
        set(PM_REL_SOURCES)
        foreach(rel_source ${PM_REL_SOURCE_FILES})
            list(APPEND PM_REL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../${rel_source})
        endforeach()
        if(rel_target MATCHES "^w..Dll")
            list(APPEND PM_REL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/REL/board_executor.c)
        endif()

        add_library(partyboard-portmaster-rel-${rel_target} SHARED ${PM_REL_SOURCES})
        list(APPEND PM_REL_BINARY_TARGETS partyboard-portmaster-rel-${rel_target})
        set_target_properties(partyboard-portmaster-rel-${rel_target} PROPERTIES
            PREFIX ""
            OUTPUT_NAME ${rel_target}
        )
        target_compile_definitions(partyboard-portmaster-rel-${rel_target} PRIVATE
            TARGET_PC
            PARTY_BOARD_PORTMASTER
            NON_MATCHING
            OPTIMIZED_TEXTURE_LOADING
            BYTESWAPPING
            PC_USE_GLES
            VERSION=1
            MUSY_TARGET=MUSY_TARGET_PC
            MUSY_VERSION_MAJOR=1
            MUSY_VERSION_MINOR=5
            MUSY_VERSION_PATCH=4
        )
        target_include_directories(partyboard-portmaster-rel-${rel_target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include/pc
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../extern
            ${CMAKE_CURRENT_SOURCE_DIR}/../extern/musyx/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include/dolphin
        )
        target_link_options(partyboard-portmaster-rel-${rel_target} PRIVATE
            "LINKER:--allow-shlib-undefined"
        )
        target_link_libraries(partyboard-portmaster-rel-${rel_target} PRIVATE m)
    endif()
endforeach()

add_library(partyboard-portmaster-platform-objects OBJECT
    src/pc/pc_aram.c
    src/pc/pc_card.c
    src/pc/pc_misc.c
    src/pc/pc_mtx.c
    src/pc/pc_mtx_compat.c
    src/pc/pc_os.c
    src/pc/pc_dvd.c
    src/pc/pc_pad.c
    src/pc/pc_prof.c
    src/pc/pc_vi.c
    src/pm_settings.c
    src/pm_version.c
)
target_compile_options(partyboard-portmaster-game-objects PRIVATE -fgnu89-inline)
target_compile_definitions(partyboard-portmaster-platform-objects PRIVATE
    TARGET_PC
    PARTY_BOARD_PORTMASTER
)
target_include_directories(partyboard-portmaster-platform-objects PRIVATE
    ${PM_SDL2_INCLUDE_PATH}
    ${PM_GLES2_INCLUDE_PATH}
    ${PM_GLES3_INCLUDE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/pc
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/musyx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include/dolphin
)

add_executable(partyboard-portmaster
    src/pm_runtime.c
    src/pm_dolassets.cpp
    src/pm_stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/port/audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/port/byteswap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/libco/libco.c
    src/pc/pc_gx.c
    src/pc/pc_gx_tev.c
    src/pc/pc_gx_texture.c
    $<TARGET_OBJECTS:partyboard-portmaster-game-objects>
    $<TARGET_OBJECTS:partyboard-portmaster-platform-objects>
)
target_compile_definitions(partyboard-portmaster PRIVATE
    TARGET_PC
    PARTY_BOARD_PORTMASTER
    NON_MATCHING
    OPTIMIZED_TEXTURE_LOADING
    BYTESWAPPING
    TARGET_DOL
    PC_USE_GLES
    VERSION=1
    MUSY_TARGET=MUSY_TARGET_PC
    MUSY_VERSION_MAJOR=1
    MUSY_VERSION_MINOR=5
    MUSY_VERSION_PATCH=4
)
target_include_directories(partyboard-portmaster PRIVATE
    ${PM_SDL2_INCLUDE_PATH}
    ${PM_GLES2_INCLUDE_PATH}
    ${PM_GLES3_INCLUDE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/pc
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/musyx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include/dolphin
)
target_link_libraries(partyboard-portmaster PRIVATE
    ${PM_SDL2_LIBRARY}
    ${PM_EGL_LIBRARY}
    ${PM_GLES2_LIBRARY}
    m
    dl
    pthread
)
target_link_options(partyboard-portmaster PRIVATE "LINKER:--export-dynamic")
add_dependencies(partyboard-portmaster ${PM_REL_BINARY_TARGETS})
add_custom_command(TARGET partyboard-portmaster POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:partyboard-portmaster>/shaders
)

install(TARGETS partyboard-portmaster RUNTIME DESTINATION .)
install(TARGETS ${PM_REL_BINARY_TARGETS}
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)
