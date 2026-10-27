# Estado del proyecto

Fase actual: **2 — Base y compilación remota** (en curso).

| Función | Clasificación | Evidencia |
|---|---|---|
| Modelo de línea de tiempo (añadir, mover, recortar, dividir, eliminar) | COMPLETA Y VERIFICADA (solo el modelo) | `tests/core_tests.cpp`: pruebas del núcleo, compiladas con GCC 13 en Linux (ver el total más abajo) |
| Deshacer/rehacer | COMPLETA Y VERIFICADA (modelo y ventana) | Mismas pruebas; confirmado a mano por el propietario en la ventana |
| Conversión de tiempos | COMPLETA Y VERIFICADA | Mismas pruebas; sin drop-frame |
| Ventana Qt con línea de tiempo de demostración | PARCIAL: compila, empaqueta y abre | Run #1 de *Windows build* en verde (commit 7b2298b): compila con MSVC y Qt 6.8, pasa `ctest`, el ZIP empaquetado arranca con `--version`. El propietario confirmó a mano que el ZIP descargado abre la interfaz. El propietario confirmó a mano que **Deshacer y Rehacer funcionan**. Sin confirmar a mano: Abrir/Guardar |
| Workflow de Windows | COMPLETA Y VERIFICADA (primera versión) | Run #1: éxito en 2 min 10 s; artefacto de 45,9 MB. Aviso de GitHub: Node.js 20 obsoleto en `checkout@v4`, `upload-artifact@v4` y `msvc-dev-cmd@v1` (sin efecto por ahora) |
| Viabilidad de MLT + FFmpeg en Windows | COMPLETA Y VERIFICADA (render) | Run de *Windows build (MSYS2 + MLT)* en commit 1e3fd78: `melt` renderiza el XML generado por NEXO y `ffprobe` confirma 640x360, audio y 120 fotogramas, tanto con MSYS2 como con el paquete SIN MSYS2 en el PATH. Licencias: ver la fila de auditoría (pendiente) |
| Formato de proyecto, guardar y abrir (núcleo) | COMPLETA Y VERIFICADA (núcleo) | 143 comprobaciones en total, 0 fallos (GCC 13, Linux): ida y vuelta, archivos dañados, esquema futuro, solapes, ids duplicados, guardado atómico y detección de medios que faltan. Documentado en `docs/PROJECT_FORMAT.md`. Limitación: rutas absolutas, sin relocalización |
| Botones Abrir/Guardar en la ventana | IMPLEMENTADA, PERO CON PRUEBAS PENDIENTES | Compila en Windows (*Windows build* en verde en ec2f8b7); falta probarlo a mano |
| Lector de medios (núcleo: `MediaProbe`) | COMPLETA Y VERIFICADA (parser) | 176 comprobaciones en total, 0 fallos; el CI lo contrasta con la salida real de `melt` (video, audio, imagen). Ver el resultado de la ejecución en el commit correspondiente |
| Importación y biblioteca en la ventana | IMPLEMENTADA, PERO CON PRUEBAS PENDIENTES | Código escrito (`MediaImporter`, `MainWindow`); falta confirmar que compila en CI y probarlo a mano |
| Vista previa / reproducción | PENDIENTE | Fase 3 |
| Generador de MLT XML (línea de tiempo → render con `melt`) | COMPLETA Y VERIFICADA (render básico) | 159 comprobaciones en Linux + render real en CI: 2 clips de video en secuencia y un clip de audio en otra pista (mezcla y composición). Sin probar: transiciones, efectos, imágenes, clips con velocidad distinta, resoluciones distintas a la del proyecto |
| Paquete de Windows con MSYS2 (`bin/`, `lib/mlt`, `share/mlt`) | PARCIAL | Se compila, empaqueta y arranca en CI sin MSYS2 (`--version`). **El propietario aún no ha abierto este ZIP a mano.** Tamaño y tiempo de compilación sin medir |
| Auditoría de licencias del paquete | PARCIAL | Inventario de MLT, FFmpeg y Qt en `THIRD_PARTY_LICENSES.md` (FFmpeg es GPL-3.0 con x264/x265 → el proyecto es GPLv3). Falta inventariar las ~160 DLL restantes y añadir los textos de licencia al paquete |
| Motor multimedia en la app (importar, vista previa) | PENDIENTE | Decisiones 0003 y 0004 |
| Exportación de video desde la app | IMPLEMENTADA, PERO CON PRUEBAS PENDIENTES | Botón Exportar, `Exporter` lanza `melt` y relee el MP4 para verificarlo; el formato de `-progress2` aún no está verificado con la salida real; falta probarlo a mano |
| Asistente de IA | PENDIENTE | Fase 5 |

## Limitaciones conocidas
- La línea de tiempo de la ventana empieza vacía (V1 y A1); los clips se añaden desde la biblioteca. Sin vista previa todavía.
- Licencia: el propietario delegó la decisión; se adopta **GPLv3 con repositorio público** (MLT y FFmpeg con x264 son GPL). El texto `LICENSE` aún no está en el repositorio: hay que añadirlo desde la web de GitHub (Add file → Create new file → `LICENSE` → *Choose a license template*).
- Las acciones de GitHub usan versiones por etiqueta mayor (`@v4`); conviene fijarlas por SHA antes de la Fase 7.

## Próxima tarea
Que el propietario abra a mano el ZIP de *Windows build (MSYS2 + MLT)*. Después: confirmar que la app con importación/exportación compila en CI, probarlo a mano y retirar el workflow MSVC.
