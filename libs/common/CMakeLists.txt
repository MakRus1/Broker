add_library(broker_common INTERFACE)
target_include_directories(broker_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(broker_common INTERFACE userver::core)
target_compile_definitions(broker_common INTERFACE PLATFORM_VERSION="${CMAKE_PROJECT_VERSION}")
