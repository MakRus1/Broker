include_guard(GLOBAL)
function(download_userver)
    set(source "${CMAKE_SOURCE_DIR}/third_party/userver")
    if(EXISTS "${source}")
        find_package(Git REQUIRED)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" rev-parse HEAD
            OUTPUT_VARIABLE actual OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" status --porcelain --untracked-files=no
            OUTPUT_VARIABLE dirty COMMAND_ERROR_IS_FATAL ANY)
        if(NOT actual STREQUAL BROKER_USERVER_COMMIT OR NOT dirty STREQUAL "")
            message(FATAL_ERROR "third_party/userver must be clean at ${BROKER_USERVER_COMMIT}, got ${actual}")
        endif()
        add_subdirectory("${source}" "${CMAKE_BINARY_DIR}/third_party/userver")
    else()
        include(FetchContent)
        FetchContent_Declare(userver
            URL "https://github.com/userver-framework/userver/archive/${BROKER_USERVER_COMMIT}.tar.gz"
            URL_HASH SHA256=${BROKER_USERVER_ARCHIVE_SHA256}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(userver)
    endif()
endfunction()
