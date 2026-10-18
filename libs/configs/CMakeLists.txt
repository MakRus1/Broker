add_library(broker_configs INTERFACE)
target_include_directories(broker_configs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(broker_configs INTERFACE userver::core)
