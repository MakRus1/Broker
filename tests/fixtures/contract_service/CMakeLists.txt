# Отдельный сервис проверяет скомпилированный кодоген; в продукт не устанавливается.
project(contract_service CXX)
include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_CURRENT_SOURCE_DIR}")
add_library(contract_service_objs OBJECT ${OPENAPI_GEN_SRCS})
broker_openapi_apply(contract_service_objs)
target_link_libraries(contract_service_objs PUBLIC userver::core broker_common)
add_executable(contract_service main.cpp ${OPENAPI_VIEW_SRCS})
target_link_libraries(contract_service PRIVATE contract_service_objs)
broker_openapi_apply(contract_service)
target_link_libraries(contract_service PRIVATE userver::core broker_common)
userver_testsuite_add_simple()

# SERVICE_CMAKE_BEGIN
foreach(service_library IN ITEMS broker_common)
    if(NOT TARGET ${service_library})
        message(FATAL_ERROR "Не найден target библиотеки: ${service_library}")
    endif()
endforeach()
target_link_libraries(${PROJECT_NAME}_objs PUBLIC broker_common)
# SERVICE_CMAKE_END

set(client_contract "${CMAKE_CURRENT_BINARY_DIR}/client_test/client_contract.hpp")
file(GLOB client_generator_files CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/scripts/openapi_gen/*.py")
add_custom_command(OUTPUT "${client_contract}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_test_client.py" "${client_contract}"
    DEPENDS ${client_generator_files} "${CMAKE_CURRENT_SOURCE_DIR}/generate_test_client.py" "${CMAKE_CURRENT_SOURCE_DIR}/docs/api/contract.yaml"
    VERBATIM)
add_executable(client_contract_unittest support/client_test.cpp "${client_contract}")
target_include_directories(client_contract_unittest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/client_test" "${OPENAPI_INCLUDE_DIR}")
add_dependencies(client_contract_unittest contract_service_codegen)
target_link_libraries(client_contract_unittest PRIVATE userver::utest broker_common)
add_google_tests(client_contract_unittest)
