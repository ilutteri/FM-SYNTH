cmake_minimum_required(VERSION 3.10)
project(FMSynth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows con MSYS2/MinGW
if(WIN32)
    # Buscar rtaudio
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(RTAUDIO rtaudio)
    endif()

    if(NOT RTAUDIO_FOUND)
        find_path(RTAUDIO_INCLUDE_DIRS RtAudio.h)
        find_library(RTAUDIO_LIBRARIES rtaudio)
        if(RTAUDIO_INCLUDE_DIRS AND RTAUDIO_LIBRARIES)
            set(RTAUDIO_FOUND TRUE)
        endif()
    endif()

    # Buscar raylib
    if(PkgConfig_FOUND)
        pkg_check_modules(RAYLIB raylib)
    endif()

    if(NOT RAYLIB_FOUND)
        find_path(RAYLIB_INCLUDE_DIRS raylib.h)
        find_library(RAYLIB_LIBRARIES raylib)
        if(RAYLIB_INCLUDE_DIRS AND RAYLIB_LIBRARIES)
            set(RAYLIB_FOUND TRUE)
        endif()
    endif()

elseif(APPLE)
    execute_process(
        COMMAND brew --prefix rtaudio
        OUTPUT_VARIABLE RTAUDIO_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    if(RTAUDIO_PREFIX)
        set(RTAUDIO_INCLUDE_DIRS "${RTAUDIO_PREFIX}/include")
        set(RTAUDIO_LIBRARY_DIRS "${RTAUDIO_PREFIX}/lib")
        find_library(RTAUDIO_LIBRARIES 
            NAMES rtaudio 
            PATHS ${RTAUDIO_LIBRARY_DIRS}
            NO_DEFAULT_PATH
        )
        set(RTAUDIO_FOUND TRUE)
    endif()
endif()

if(NOT RTAUDIO_FOUND)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(RTAUDIO rtaudio)
    endif()
endif()

if(NOT RTAUDIO_FOUND)
    find_path(RTAUDIO_INCLUDE_DIRS RtAudio.h
        PATHS /usr/include /usr/local/include)
    find_library(RTAUDIO_LIBRARIES rtaudio
        PATHS /usr/lib /usr/local/lib)
    
    if(RTAUDIO_INCLUDE_DIRS AND RTAUDIO_LIBRARIES)
        set(RTAUDIO_FOUND TRUE)
    endif()
endif()

if(NOT RTAUDIO_FOUND)
    message(FATAL_ERROR "RtAudio not found")
endif()

add_executable(fm_synth fm_synth.cpp)

target_include_directories(fm_synth PRIVATE ${RTAUDIO_INCLUDE_DIRS})
target_link_libraries(fm_synth ${RTAUDIO_LIBRARIES})

if(APPLE)
    target_link_libraries(fm_synth 
        "-framework CoreAudio"
        "-framework CoreFoundation"
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(fm_synth pthread)
endif()

if(WIN32)
    target_link_libraries(fm_synth ole32 winmm dsound)
endif()

message(STATUS "RtAudio include: ${RTAUDIO_INCLUDE_DIRS}")
message(STATUS "RtAudio library: ${RTAUDIO_LIBRARIES}")

# ============================================================================
# FM Synth GUI (con raylib)
# ============================================================================

# Buscar raylib
if(APPLE)
    execute_process(
        COMMAND brew --prefix raylib
        OUTPUT_VARIABLE RAYLIB_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(RAYLIB_PREFIX)
        set(RAYLIB_INCLUDE_DIRS "${RAYLIB_PREFIX}/include")
        set(RAYLIB_LIBRARY_DIRS "${RAYLIB_PREFIX}/lib")
        find_library(RAYLIB_LIBRARIES
            NAMES raylib
            PATHS ${RAYLIB_LIBRARY_DIRS}
            NO_DEFAULT_PATH
        )
        set(RAYLIB_FOUND TRUE)
    endif()
endif()

if(NOT RAYLIB_FOUND)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(RAYLIB raylib)
    endif()
endif()

if(NOT RAYLIB_FOUND)
    find_path(RAYLIB_INCLUDE_DIRS raylib.h
        PATHS /usr/include /usr/local/include)
    find_library(RAYLIB_LIBRARIES raylib
        PATHS /usr/lib /usr/local/lib)

    if(RAYLIB_INCLUDE_DIRS AND RAYLIB_LIBRARIES)
        set(RAYLIB_FOUND TRUE)
    endif()
endif()

if(RAYLIB_FOUND)
    message(STATUS "raylib found - building GUI version")
    message(STATUS "raylib include: ${RAYLIB_INCLUDE_DIRS}")
    message(STATUS "raylib library: ${RAYLIB_LIBRARIES}")

    add_executable(fm_synth_gui fm_synth_gui.cpp)

    target_include_directories(fm_synth_gui PRIVATE
        ${RTAUDIO_INCLUDE_DIRS}
        ${RAYLIB_INCLUDE_DIRS}
    )

    target_link_libraries(fm_synth_gui
        ${RTAUDIO_LIBRARIES}
        ${RAYLIB_LIBRARIES}
    )

    if(APPLE)
        target_link_libraries(fm_synth_gui
            "-framework CoreAudio"
            "-framework CoreFoundation"
            "-framework IOKit"
            "-framework Cocoa"
            "-framework OpenGL"
        )
    endif()

    if(UNIX AND NOT APPLE)
        target_link_libraries(fm_synth_gui pthread GL m dl)
    endif()

    if(WIN32)
        target_link_libraries(fm_synth_gui
            ole32 winmm dsound
            opengl32 gdi32 user32 shell32
        )
    endif()
else()
    message(STATUS "raylib not found - GUI version will not be built")
    message(STATUS "Install raylib: brew install raylib (macOS) or apt install libraylib-dev (Ubuntu)")
endif()