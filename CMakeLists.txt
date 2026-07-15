cmake_minimum_required(VERSION 3.17)
project(MinaPracticeMod LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    dllmain.cpp
)

add_library(PracticeMod MODULE ${SOURCES})

if(WIN32)
    set_target_properties(PracticeMod PROPERTIES PREFIX "" SUFFIX ".dll")
    target_compile_definitions(PracticeMod PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(UNIX)
    set_target_properties(PracticeMod PROPERTIES PREFIX "" SUFFIX ".so" OUTPUT_NAME "mod")
    target_compile_options(PracticeMod PRIVATE -fvisibility=hidden)
endif()

target_include_directories(PracticeMod PRIVATE .)
