add_executable(nexo_core_tests core_tests.cpp)
target_link_libraries(nexo_core_tests PRIVATE nexo_core)
add_test(NAME nexo_core_tests COMMAND nexo_core_tests)

# Herramienta que usa el workflow de CI para probar el render real con melt.
add_executable(mlt_xml_tool ${CMAKE_SOURCE_DIR}/tools/mlt_xml_tool.cpp)
target_link_libraries(mlt_xml_tool PRIVATE nexo_core)
add_executable(props_case_tool ${CMAKE_SOURCE_DIR}/tools/props_case_tool.cpp)
target_link_libraries(props_case_tool PRIVATE nexo_core)

# Lee la salida real de `melt` en CI y comprueba que NEXO la interpreta bien.
add_executable(probe_tool ${CMAKE_SOURCE_DIR}/tools/probe_tool.cpp)
target_link_libraries(probe_tool PRIVATE nexo_core)
