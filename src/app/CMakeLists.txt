find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

configure_file(BuildInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/BuildInfo.h @ONLY)

qt_add_executable(nexo-video-ai WIN32
  main.cpp
  MainWindow.cpp
  MainWindow.h
  MeltRunner.cpp
  MeltRunner.h
  MediaImporter.cpp
  MediaImporter.h
  Exporter.cpp
  Exporter.h
  Previewer.cpp
  Previewer.h
  TimelineWidget.cpp
  TimelineWidget.h
  Theme.h
)
target_include_directories(nexo-video-ai PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(nexo-video-ai PRIVATE nexo_core Qt6::Widgets)
