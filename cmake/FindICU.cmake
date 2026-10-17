# userver 3.2 ошибочно ищет заголовки keg-only ICU в общем include Homebrew.
if(APPLE AND BROKER_BREW_PREFIX)
    set(ICU_INCLUDE_DIR "${BROKER_BREW_PREFIX}/opt/icu4c/include" CACHE PATH "Заголовки ICU" FORCE)
endif()
include("${CMAKE_ROOT}/Modules/FindICU.cmake")
