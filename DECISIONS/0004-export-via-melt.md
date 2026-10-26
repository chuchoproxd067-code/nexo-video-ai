# 0004 — Exportación: la app genera MLT XML y ejecuta `melt`

**Estado:** aceptada (reversible).

En lugar de enlazar la API C de MLT, `src/core/MltXml.cpp` convierte la línea de tiempo en un documento MLT XML y la aplicación lanza el `melt.exe` empaquetado como proceso aparte. Ventajas: el generador es texto puro (se prueba en cualquier sistema), el render no bloquea la interfaz, un fallo del motor no tumba la aplicación y no se enlaza código GPL de MLT dentro del ejecutable de la interfaz. Inconvenientes: el progreso hay que leerlo de la salida de `melt`, y la vista previa en tiempo real necesitará otra vía (API de MLT o `melt` con consumidor SDL), a decidir en la Fase 3.

Verificación: `tests/core_tests.cpp` comprueba la estructura del XML; el workflow `windows-msys2.yml` renderiza el XML de una línea de tiempo real con `melt` y valida con `ffprobe` (resolución, audio y 120 fotogramas).
