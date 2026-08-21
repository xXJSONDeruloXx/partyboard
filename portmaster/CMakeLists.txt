cmake_minimum_required(VERSION 3.16)

project(partyboard_portmaster_probe LANGUAGES C)

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
