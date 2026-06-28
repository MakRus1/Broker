project(broker CXX)

include_directories(src)

add_library(
    ${PROJECT_NAME}_objs OBJECT
    src/greeting.cpp
    src/hello.cpp
    src/hello_postgres.cpp
    src/hello_grpc.cpp
)
target_link_libraries(
    ${PROJECT_NAME}_objs
    PUBLIC userver::core userver::grpc userver::postgresql broker_common
)

userver_add_grpc_library(${PROJECT_NAME}_proto PROTOS handlers/hello.proto)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC ${PROJECT_NAME}_proto)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)

add_executable(${PROJECT_NAME}_unittest src/greeting_test.cpp)
target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver::utest)
add_google_tests(${PROJECT_NAME}_unittest)

add_executable(${PROJECT_NAME}_benchmark src/greeting_benchmark.cpp)
target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver::ubench)
add_google_benchmark_tests(${PROJECT_NAME}_benchmark)

userver_testsuite_add_simple()

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
