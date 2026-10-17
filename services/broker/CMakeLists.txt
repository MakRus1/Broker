project(broker CXX)

include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_CURRENT_SOURCE_DIR}")

add_library(
    ${PROJECT_NAME}_objs OBJECT
    src/greeting.cpp
    src/hello_grpc.cpp
    src/ping.cpp
    ${OPENAPI_GEN_SRCS}
)
target_link_libraries(
    ${PROJECT_NAME}_objs
    PUBLIC userver::core userver::grpc userver::postgresql userver::otlp broker_common
)

if(BUILD_TESTING)
    target_sources(${PROJECT_NAME}_objs PRIVATE tests/support/trace_probe.cpp)
    target_include_directories(${PROJECT_NAME}_objs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tests/support")
    target_compile_definitions(${PROJECT_NAME}_objs PUBLIC BROKER_TRACE_TESTS=1)
endif()
# SQL_QUERIES_BEGIN
file(GLOB_RECURSE service_sql CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/postgresql/queries/*.sql")
userver_add_sql_library(${PROJECT_NAME}_sql
    NAMESPACE ${PROJECT_NAME}
    SOURCE_DIR postgresql/queries
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_sql
    SQL_FILES *.sql)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC ${PROJECT_NAME}_sql)
# SQL_QUERIES_END
broker_openapi_apply(${PROJECT_NAME}_objs)
target_compile_definitions(${PROJECT_NAME}_objs PUBLIC BROKER_VERSION="${CMAKE_PROJECT_VERSION}")

userver_add_grpc_library(${PROJECT_NAME}_proto PROTOS handlers/hello.proto)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC ${PROJECT_NAME}_proto)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)
if(OPENAPI_VIEW_SRCS)
    target_sources(${PROJECT_NAME} PRIVATE ${OPENAPI_VIEW_SRCS})
    broker_openapi_apply(${PROJECT_NAME})
endif()

if(BUILD_TESTING)
    add_executable(${PROJECT_NAME}_unittest src/greeting_test.cpp)
    target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver::utest)
    add_google_tests(${PROJECT_NAME}_unittest)

    add_executable(${PROJECT_NAME}_benchmark src/greeting_benchmark.cpp)
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
