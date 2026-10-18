include(FetchContent)
FetchContent_Declare(broker_jwt_cpp
    URL https://codeload.github.com/Thalhammer/jwt-cpp/tar.gz/b0ea29a58fc852a67d4e896d266880c2c63b0c4c
    URL_HASH SHA256=df9c4782fc3a9141b7a1156dd031756f747146952af6af101d5689e91a513efd
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR broker-no-build)
# Подключаем только заголовки; примеры и зависимости стороннего проекта не собираются.
FetchContent_MakeAvailable(broker_jwt_cpp)
add_library(broker_auth STATIC src/verifier.cpp src/component.cpp)
target_include_directories(broker_auth PUBLIC include PRIVATE "${broker_jwt_cpp_SOURCE_DIR}/include")
target_link_libraries(broker_auth PUBLIC userver::core broker_common)
