project(web_bff CXX)

include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_CURRENT_SOURCE_DIR}")

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
foreach(service_library IN ITEMS broker_common broker_auth broker_browser broker_configs)
    if(NOT TARGET ${service_library})
        message(FATAL_ERROR "Не найден target библиотеки: ${service_library}")
    endif()
endforeach()
target_link_libraries(${PROJECT_NAME}_objs PUBLIC broker_common broker_auth broker_browser broker_configs)
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
