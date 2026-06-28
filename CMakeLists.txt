cmake_minimum_required(VERSION 3.12...3.31)
project(broker-monorepo CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DownloadUserver)

find_package(
    userver
    COMPONENTS core grpc postgresql
    QUIET
)
if(NOT userver_FOUND)
    set(USERVER_FEATURE_GRPC ON CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_POSTGRESQL ON CACHE BOOL "" FORCE)
    download_userver(TRY_DIR third_party/userver VERSION 2.15)
endif()

userver_setup_environment()

add_subdirectory(libs/common)
add_subdirectory(services/broker)
