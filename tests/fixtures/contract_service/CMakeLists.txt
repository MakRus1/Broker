# Отдельный сервис проверяет скомпилированный кодоген; в продукт не устанавливается.
project(contract_service CXX)
include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(contract_service main.cpp ${OPENAPI_GEN_SRCS} ${OPENAPI_VIEW_SRCS})
broker_openapi_apply(contract_service)
target_link_libraries(contract_service PRIVATE userver::core broker_common)
userver_testsuite_add_simple()
