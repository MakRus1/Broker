# Совместимые зависимости для нативной сборки macOS.
if(NOT APPLE)
    return()
endif()
find_program(BROKER_BREW_EXECUTABLE brew REQUIRED)
execute_process(COMMAND "${BROKER_BREW_EXECUTABLE}" --prefix
    OUTPUT_VARIABLE BROKER_BREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
list(PREPEND CMAKE_PREFIX_PATH
    "${BROKER_BREW_PREFIX}/opt/openssl@3"
    "${BROKER_BREW_PREFIX}/opt/openldap"
    "${BROKER_BREW_PREFIX}/opt/postgresql@16"
    "${BROKER_BREW_PREFIX}/opt/gettext"
    "${BROKER_BREW_PREFIX}/opt/icu4c"
    "${BROKER_BREW_PREFIX}/opt/zlib"
    "${BROKER_BREW_PREFIX}/opt/bzip2")
# FindOpenSSL может предпочесть openssl@4 через pkg-config даже при CMAKE_PREFIX_PATH.
set(OPENSSL_ROOT_DIR "${BROKER_BREW_PREFIX}/opt/openssl@3" CACHE PATH "OpenSSL 3" FORCE)
set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include" CACHE PATH "Заголовки OpenSSL 3" FORCE)
set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.a" CACHE FILEPATH "OpenSSL Crypto 3" FORCE)
set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.a" CACHE FILEPATH "OpenSSL SSL 3" FORCE)
set(USERVER_FORCE_DOWNLOAD_PROTOBUF ON CACHE BOOL "" FORCE)
set(USERVER_FORCE_DOWNLOAD_GRPC ON CACHE BOOL "" FORCE)
set(USERVER_FORCE_DOWNLOAD_BOOST ON CACHE BOOL "" FORCE)
set(USERVER_FORCE_DOWNLOAD_ABSEIL ON CACHE BOOL "" FORCE)
set(USERVER_FORCE_DOWNLOAD_FMT ON CACHE BOOL "" FORCE)

# В общих каталогах Homebrew могут находиться несовместимые версии заголовков.
function(broker_native_headers directory)
    get_property(local_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS local_targets)
        get_target_property(kind "${target}" TYPE)
        if(kind MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$"
           AND (target MATCHES "^(userver-|grpc|gpr)" OR NOT directory MATCHES "/_deps/"))
            target_include_directories("${target}" SYSTEM BEFORE PRIVATE
                "${BROKER_BREW_PREFIX}/opt/openssl@3/include"
                "${CPM_PACKAGE_Protobuf_SOURCE_DIR}/src"
                "${CPM_PACKAGE_gRPC_SOURCE_DIR}/include"
                "${CPM_PACKAGE_abseil-cpp_SOURCE_DIR}"
                "${CPM_PACKAGE_fmt_SOURCE_DIR}/include")
        endif()
    endforeach()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        broker_native_headers("${child}")
    endforeach()
endfunction()
