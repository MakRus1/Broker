project(broker CXX)

include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_CURRENT_SOURCE_DIR}")

add_library(
    ${PROJECT_NAME}_objs OBJECT
    src/domain/greeting.cpp
    src/bootstrap/ping.cpp
    ${OPENAPI_GEN_SRCS}
)
target_link_libraries(
    ${PROJECT_NAME}_objs
    PUBLIC userver::core userver::otlp
)

if(BUILD_TESTING)
    target_sources(${PROJECT_NAME}_objs PRIVATE tests/support/trace_probe.cpp)
    target_include_directories(${PROJECT_NAME}_objs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tests/support")
    target_compile_definitions(${PROJECT_NAME}_objs PUBLIC BROKER_TRACE_TESTS=1)
endif()
# SERVICE_CMAKE_BEGIN
foreach(service_library IN ITEMS broker_common)
    if(NOT TARGET ${service_library})
        message(FATAL_ERROR "Не найден target библиотеки: ${service_library}")
    endif()
endforeach()
target_link_libraries(${PROJECT_NAME}_objs PUBLIC broker_common)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver::postgresql)
file(GLOB_RECURSE service_sql CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/postgresql/queries/*.sql")
userver_add_sql_library(${PROJECT_NAME}_sql
    NAMESPACE ${PROJECT_NAME}
    SOURCE_DIR postgresql/queries
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_sql
    SQL_FILES *.sql)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC ${PROJECT_NAME}_sql)
target_sources(${PROJECT_NAME}_objs PRIVATE src/transport/grpc/hello_grpc.cpp)
file(GLOB_RECURSE service_protos CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")
userver_add_grpc_library(${PROJECT_NAME}_proto PROTOS ${service_protos})
target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver::grpc ${PROJECT_NAME}_proto)
# SERVICE_CMAKE_END
broker_openapi_apply(${PROJECT_NAME}_objs)
target_compile_definitions(${PROJECT_NAME}_objs PUBLIC BROKER_VERSION="${CMAKE_PROJECT_VERSION}")


add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)
if(OPENAPI_VIEW_SRCS)
    target_sources(${PROJECT_NAME} PRIVATE ${OPENAPI_VIEW_SRCS})
    broker_openapi_apply(${PROJECT_NAME})
endif()

if(BUILD_TESTING)
    add_executable(${PROJECT_NAME}_unittest tests/unit/greeting_test.cpp)
    target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver::utest)
    add_google_tests(${PROJECT_NAME}_unittest)

    add_executable(${PROJECT_NAME}_benchmark tests/benchmarks/greeting_benchmark.cpp)
    target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver::ubench)
    add_google_benchmark_tests(${PROJECT_NAME}_benchmark)

    userver_testsuite_add_simple()
endif()

include(GNUInstallDirs)

if(DEFINED ENV{PREFIX})
    message(STATUS "Set install prefix: $ENV{PREFIX}")
    file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
    set(CMAKE_INSTALL_PREFIX "${PREFIX_PATH}")
endif()

file(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

install(
    TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${PROJECT_NAME}
)
install(
    FILES ${CONFIGS_FILES}
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
    COMPONENT ${PROJECT_NAME}
)
