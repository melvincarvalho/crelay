cmake_minimum_required(VERSION 3.10)
project(crelay C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find necessary packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
pkg_check_modules(JANSSON REQUIRED jansson)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(SECP256K1 REQUIRED libsecp256k1)
pkg_check_modules(OPENSSL REQUIRED openssl)

# Include directories
include_directories(
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${JANSSON_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${SECP256K1_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add source files
set(SOURCES
    src/main.c
    src/relay.c
    src/event.c
    src/database.c
    src/subscription.c
    src/crypto.c
    src/json_utils.c
)

# Create executable
add_executable(crelay ${SOURCES})

# Link libraries
target_link_libraries(crelay
    ${LIBWEBSOCKETS_LIBRARIES}
    ${JANSSON_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${SECP256K1_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    -lm
    -lpthread
)

# Create include and src directories if they don't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src) 