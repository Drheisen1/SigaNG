cmake_minimum_required(VERSION 3.21)

project(
    SIGAPlugin
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(CommonLibSSE CONFIG REQUIRED)

# SimpleIni is header-only, find it manually
find_path(SIMPLEINI_INCLUDE_DIRS "SimpleIni.h")

add_library(
    ${PROJECT_NAME}
    SHARED
    src/Main.cpp
    src/AnimationHandler.cpp
    src/CombatEventHandler.cpp
    src/SlowMotion.cpp
    src/Config.cpp
   )

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SIMPLEINI_INCLUDE_DIRS}
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        CommonLibSSE::CommonLibSSE
)

target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE
        include/PCH.h
)

# Set output directory
set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/Release"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/Release"
)