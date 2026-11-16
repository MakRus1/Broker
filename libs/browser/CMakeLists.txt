file(GLOB browser_sql CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/postgresql/queries/*.sql")
userver_add_sql_library(broker_browser_sql
    NAMESPACE browser_storage
    SOURCE_DIR postgresql/queries
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_sql
    SQL_FILES *.sql)

add_library(broker_browser STATIC
    src/sessions.cpp
    src/browser_session.cpp
    src/login.cpp
    src/assets.cpp)
target_include_directories(broker_browser PUBLIC include)
target_link_libraries(broker_browser PUBLIC broker_auth broker_common userver::core userver::postgresql
    PRIVATE broker_browser_sql)
