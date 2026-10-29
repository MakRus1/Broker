add_library(broker_outbox STATIC src/component.cpp)
target_include_directories(broker_outbox PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(broker_outbox PUBLIC broker_common userver::postgresql)
userver_add_sql_library(broker_outbox_sql
    NAMESPACE platform_outbox_sql
    SOURCE_DIR postgresql/queries
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_sql
    SQL_FILES *.sql)
target_link_libraries(broker_outbox PUBLIC broker_outbox_sql)
