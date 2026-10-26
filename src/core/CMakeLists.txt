add_library(nexo_core STATIC
  Timecode.cpp
  Timeline.cpp
  History.cpp
)
target_include_directories(nexo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(nexo_core PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(nexo_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(nexo_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
