project(shipments CXX)

include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_CURRENT_SOURCE_DIR}")

add_library(
    ${PROJECT_NAME}_objs OBJECT
    src/operations/request_transportations.cpp
    src/components/transportation_dispatcher.cpp
    src/components/cargo_suggestions_cleanup.cpp
    src/bootstrap/ping.cpp
    src/domain/route_details.cpp
    ${OPENAPI_GEN_SRCS}
)
target_link_libraries(
    ${PROJECT_NAME}_objs
    PUBLIC userver::core userver::otlp
)

# SERVICE_CMAKE_BEGIN
foreach(service_library IN ITEMS broker_common broker_configs broker_auth)
    if(NOT TARGET ${service_library})
        message(FATAL_ERROR "Не найден target библиотеки: ${service_library}")
    endif()
endforeach()
target_link_libraries(${PROJECT_NAME}_objs PUBLIC broker_common broker_configs broker_auth)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver::postgresql)
file(GLOB_RECURSE service_sql CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/postgresql/queries/*.sql")
if(service_sql)
userver_add_sql_library(${PROJECT_NAME}_sql
    NAMESPACE ${PROJECT_NAME}
    SOURCE_DIR postgresql/queries
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_sql
    SQL_FILES *.sql)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC ${PROJECT_NAME}_sql)
endif()
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
    add_executable(shipments_registry_query_dump EXCLUDE_FROM_ALL tests/unit/registry_query_dump.cpp)
    target_link_libraries(shipments_registry_query_dump PRIVATE ${PROJECT_NAME}_objs)
    broker_openapi_apply(shipments_registry_query_dump)
    userver_testsuite_add_simple()
    add_executable(shipments_unittest tests/unit/date_validation_test.cpp tests/unit/dictionaries_test.cpp)
    target_include_directories(shipments_unittest PRIVATE src)
    target_link_libraries(shipments_unittest PRIVATE broker_common userver::utest)
    add_google_tests(shipments_unittest)
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
