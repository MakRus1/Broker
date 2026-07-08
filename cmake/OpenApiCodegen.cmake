# OpenAPI codegen integration for userver services.
#
# Expects:
#   - docs/api/*.yaml specs
#   - scripts/openapi_gen/generate.py at repo root
#
# Sets:
#   OPENAPI_GEN_SRCS, OPENAPI_VIEW_SRCS
# Adds include directory .gen/include when present.

function(broker_openapi_generate SERVICE_DIR)
    set(OPENAPI_GEN_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/openapi_gen/generate.py")
    set(OPENAPI_ENSURE_VENV "${CMAKE_SOURCE_DIR}/scripts/openapi_gen/ensure_venv.sh")
    set(OPENAPI_SPEC_DIR "${SERVICE_DIR}/docs/api")

    if(NOT EXISTS "${OPENAPI_SPEC_DIR}")
        set(OPENAPI_GEN_SRCS "" PARENT_SCOPE)
        set(OPENAPI_VIEW_SRCS "" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND bash "${OPENAPI_ENSURE_VENV}"
        OUTPUT_VARIABLE OPENAPI_GEN_PYTHON
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE OPENAPI_VENV_RESULT
        ERROR_VARIABLE OPENAPI_VENV_ERROR
    )
    if(NOT OPENAPI_VENV_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to prepare OpenAPI codegen venv: ${OPENAPI_VENV_ERROR}")
    endif()

    execute_process(
        COMMAND "${OPENAPI_GEN_PYTHON}" "${OPENAPI_GEN_SCRIPT}" --service-dir "${SERVICE_DIR}"
        WORKING_DIRECTORY "${SERVICE_DIR}"
        RESULT_VARIABLE OPENAPI_GEN_RESULT
        ERROR_VARIABLE OPENAPI_GEN_ERROR
    )
    if(NOT OPENAPI_GEN_RESULT EQUAL 0)
        message(FATAL_ERROR "OpenAPI codegen failed: ${OPENAPI_GEN_ERROR}")
    endif()

    file(GLOB_RECURSE _gen_srcs "${SERVICE_DIR}/.gen/src/*.cpp")
    file(GLOB_RECURSE _view_srcs "${SERVICE_DIR}/src/views/*.cpp")
    set(OPENAPI_GEN_SRCS "${_gen_srcs}" PARENT_SCOPE)
    set(OPENAPI_VIEW_SRCS "${_view_srcs}" PARENT_SCOPE)
endfunction()

function(broker_openapi_apply TARGET)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.gen/include")
        target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.gen/include")
    endif()
endfunction()
