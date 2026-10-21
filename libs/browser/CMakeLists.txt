add_library(broker_browser STATIC
    src/sessions.cpp
    src/browser_session.cpp
    src/login.cpp
    src/assets.cpp)
target_include_directories(broker_browser PUBLIC include)
target_link_libraries(broker_browser PUBLIC broker_auth broker_common userver::core)
