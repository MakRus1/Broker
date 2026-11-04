project(localization CXX)

include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_CURRENT_SOURCE_DIR}")

set(localization_catalog_dir "${CMAKE_SOURCE_DIR}/frontend/shared/i18n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${localization_catalog_dir}/ru.json" "${localization_catalog_dir}/en.json")
file(READ "${localization_catalog_dir}/ru.json" LOCALIZATION_RU)
file(READ "${localization_catalog_dir}/en.json" LOCALIZATION_EN)
string(REPLACE "\n" ")catalog\",\nR\"catalog(" LOCALIZATION_RU "${LOCALIZATION_RU}")
string(REPLACE "\n" ")catalog\",\nR\"catalog(" LOCALIZATION_EN "${LOCALIZATION_EN}")
configure_file(src/helpers/builtin_catalog.hpp.in generated/builtin_catalog.hpp @ONLY)

add_library(
    ${PROJECT_NAME}_objs OBJECT
    src/bootstrap/ping.cpp
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
target_include_directories(${PROJECT_NAME}_objs PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
broker_openapi_apply(${PROJECT_NAME}_objs)
target_compile_definitions(${PROJECT_NAME}_objs PUBLIC BROKER_VERSION="${CMAKE_PROJECT_VERSION}")


add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)
if(OPENAPI_VIEW_SRCS)
    target_sources(${PROJECT_NAME} PRIVATE ${OPENAPI_VIEW_SRCS})
    broker_openapi_apply(${PROJECT_NAME})
endif()

if(BUILD_TESTING)
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
