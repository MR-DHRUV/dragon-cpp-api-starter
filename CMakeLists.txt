cmake_minimum_required(VERSION 3.22)
project(backend_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enforce config.json as single source of truth at compile time
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.json")
    message(FATAL_ERROR "Configuration file 'config.json' not found in project root! It is required as the single source of truth.")
endif()

# Optimization and warning flags
if(MSVC)
    add_compile_options(
        /W4
        /O2
        /Oi
        /Ot
        /Gy
        /GF
        /permissive-
        /utf-8
        /MP
        /EHsc
        /D_WIN32_WINNT=0x0A00
        /DNOMINMAX
        /DWIN32_LEAN_AND_MEAN
    )
    add_link_options(/OPT:REF /OPT:ICF)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -O3
        -march=native
        -pipe
    )
endif()

# Find required dependencies
find_package(Threads REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(Drogon CONFIG REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${RAPIDJSON_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/db/DatabaseManager.cpp
    src/services/UserService.cpp
    src/controllers/HealthController.cpp
    src/controllers/UserController.cpp
    src/controllers/MetricsController.cpp
    src/controllers/DocsController.cpp
    src/otel/OtelSpan.cpp
    src/otel/OtelMetrics.cpp
    src/otel/OtelManager.cpp
)

# Executable target
add_executable(backend_server ${SOURCES})

# Link dependencies
target_link_libraries(backend_server
    PRIVATE
        Drogon::Drogon
        rapidjson
        Threads::Threads
)

if(WIN32)
    target_link_libraries(backend_server
        PRIVATE
            ws2_32
            crypt32
            secur32
            shlwapi
            winhttp
    )
endif()

# Target include directories for consumers/tests
target_include_directories(backend_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Copy config.json to build output directory
add_custom_command(
    TARGET backend_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.json
        $<TARGET_FILE_DIR:backend_server>/config.json
    COMMENT "Copying config.json to target output folder"
)

# Unit test executable target
add_executable(unit_tests 
    tests/test_rapidjson_helper.cpp
    src/otel/OtelSpan.cpp
    src/otel/OtelMetrics.cpp
)
target_link_libraries(unit_tests
    PRIVATE
        Drogon::Drogon
        rapidjson
        Threads::Threads
)
if(WIN32)
    target_link_libraries(unit_tests PRIVATE ws2_32 crypt32 secur32 shlwapi winhttp)
endif()
target_include_directories(unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Convenient execution targets
add_custom_target(run
    COMMAND $<TARGET_FILE:backend_server>
    DEPENDS backend_server
    WORKING_DIRECTORY $<TARGET_FILE_DIR:backend_server>
    COMMENT "Starting backend server..."
)

add_custom_target(test_unit
    COMMAND $<TARGET_FILE:unit_tests>
    DEPENDS unit_tests
    WORKING_DIRECTORY $<TARGET_FILE_DIR:unit_tests>
    COMMENT "Running unit tests..."
)
