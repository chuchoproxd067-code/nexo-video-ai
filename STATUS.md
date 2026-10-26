# Estado del proyecto

Fase actual: **2 — Base y compilación remota** (en curso).

| Función | Clasificación | Evidencia |
|---|---|---|
| Modelo de línea de tiempo (añadir, mover, recortar, dividir, eliminar) | COMPLETA Y VERIFICADA (solo el modelo) | `tests/core_tests.cpp`: 75 comprobaciones, 0 fallos, compilado con GCC 13 en Linux |
| Deshacer/rehacer | COMPLETA Y VERIFICADA (modelo) | Mismas pruebas |
| Conversión de tiempos | COMPLETA Y VERIFICADA | Mismas pruebas; sin drop-frame |
| Ventana Qt con línea de tiempo de demostración | IMPLEMENTADA, PERO CON PRUEBAS PENDIENTES | Código escrito; **no compilado todavía** (no hay Qt en el entorno de desarrollo) |
| Workflow de Windows | IMPLEMENTADA, PERO CON PRUEBAS PENDIENTES | **Nunca ejecutado.** Hay que lanzarlo desde GitHub |
| Importación, biblioteca, vista previa | PENDIENTE | Fase 3 |
| Motor multimedia (MLT/FFmpeg) | PENDIENTE | Viabilidad en Windows sin probar |
| Exportación de video | PENDIENTE | Fase 3 |
| Asistente de IA | PENDIENTE | Fase 5 |

## Limitaciones conocidas
- La línea de tiempo de la ventana es una demostración con un clip de marcador (`demo.mp4`); no hay medios reales.
- La licencia del proyecto **no está elegida**: falta decisión del propietario (propuesta: GPLv3). Hasta entonces no se añade archivo LICENSE.
- Las acciones de GitHub usan versiones por etiqueta mayor (`@v4`); conviene fijarlas por SHA antes de la Fase 7.

## Próxima tarea
Ejecutar *Windows build* en GitHub, corregir lo que falle y confirmar que el ZIP arranca. Después: prueba de viabilidad de MLT + FFmpeg en el runner.
