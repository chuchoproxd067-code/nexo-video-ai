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
  ClipDialogs.cpp
  ClipDialogs.h
  Exporter.cpp
  Exporter.h
  Previewer.cpp
  Previewer.h
  TextRender.cpp
  TextRender.h
  TimelineWidget.cpp
  TimelineWidget.h
  Theme.h
)
target_include_directories(nexo-video-ai PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(nexo-video-ai PRIVATE nexo_core Qt6::Widgets)

# Prueba del dibujado de texto (usa la plataforma "offscreen" de Qt; sin ventana).
qt_add_executable(text_render_test TextRenderTest.cpp TextRender.cpp TextRender.h)
target_link_libraries(text_render_test PRIVATE nexo_core Qt6::Widgets)
add_test(NAME text_render COMMAND text_render_test)
