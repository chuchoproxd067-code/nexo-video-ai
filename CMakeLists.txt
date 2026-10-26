cmake_minimum_required(VERSION 3.21)
project(NexoVideoAI VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Datos de compilación que se muestran en la aplicación y en build-info.txt.
set(NEXO_GIT_COMMIT "unknown" CACHE STRING "Commit con el que se compila")

option(NEXO_BUILD_APP "Compilar la aplicación Qt (requiere Qt 6.5+)" ON)
option(NEXO_BUILD_TESTS "Compilar las pruebas del núcleo" ON)

# El núcleo (modelo del proyecto, comandos, tiempos) no depende de Qt ni de MLT.
add_subdirectory(src/core)

if(NEXO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(NEXO_BUILD_APP)
  add_subdirectory(src/app)
endif()
