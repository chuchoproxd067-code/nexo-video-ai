# Estado del proyecto

Fase actual: **2 — Base y compilación remota** (en curso).

| Función | Clasificación | Evidencia |
|---|---|---|
| Modelo de línea de tiempo (añadir, mover, recortar, dividir, eliminar) | COMPLETA Y VERIFICADA (solo el modelo) | `tests/core_tests.cpp`: 75 comprobaciones, 0 fallos, compilado con GCC 13 en Linux |
| Deshacer/rehacer | COMPLETA Y VERIFICADA (modelo) | Mismas pruebas |
| Conversión de tiempos | COMPLETA Y VERIFICADA | Mismas pruebas; sin drop-frame |
| Ventana Qt con línea de tiempo de demostración | PARCIAL: compila, empaqueta y abre | Run #1 de *Windows build* en verde (commit 7b2298b): compila con MSVC y Qt 6.8, pasa `ctest`, el ZIP empaquetado arranca con `--version`. El propietario confirmó a mano que el ZIP descargado abre la interfaz. **Sin confirmar:** que los botones Dividir, Deshacer y Rehacer funcionen |
| Workflow de Windows | COMPLETA Y VERIFICADA (primera versión) | Run #1: éxito en 2 min 10 s; artefacto de 45,9 MB. Aviso de GitHub: Node.js 20 obsoleto en `checkout@v4`, `upload-artifact@v4` y `msvc-dev-cmd@v1` (sin efecto por ahora) |
| Viabilidad de MLT + FFmpeg en Windows | PROTOTIPO: primer intento FALLÓ | Run #1 de *MLT feasibility* en rojo a los 34 s (commit adb081f), antes de renderizar; causa sin confirmar (probable nombre de paquete MSYS2 incorrecto; MINGW64 está obsoleto). Se reescribió para usar UCRT64 y guardar diagnóstico siempre |
| Importación, biblioteca, vista previa | PENDIENTE | Fase 3 |
| Motor multimedia (MLT/FFmpeg) | PENDIENTE | Viabilidad en Windows sin probar |
| Exportación de video | PENDIENTE | Fase 3 |
| Asistente de IA | PENDIENTE | Fase 5 |

## Limitaciones conocidas
- La línea de tiempo de la ventana es una demostración con un clip de marcador (`demo.mp4`); no hay medios reales.
- La licencia del proyecto **no está elegida**: falta decisión del propietario (propuesta: GPLv3). Hasta entonces no se añade archivo LICENSE.
- Las acciones de GitHub usan versiones por etiqueta mayor (`@v4`); conviene fijarlas por SHA antes de la Fase 7.

## Próxima tarea
Leer la evidencia del nuevo run de *MLT feasibility (Windows)* (MP4 de 75 fotogramas, licencias, capacidades). Con el resultado, decidir la cadena de herramientas (MinGW/MSYS2 para todo, o MSVC para la interfaz y MinGW para MLT) y registrarlo en `DECISIONS/`.
