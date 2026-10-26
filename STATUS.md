# Estado del proyecto

Fase actual: **2 — Base y compilación remota** (en curso).

| Función | Clasificación | Evidencia |
|---|---|---|
| Modelo de línea de tiempo (añadir, mover, recortar, dividir, eliminar) | COMPLETA Y VERIFICADA (solo el modelo) | `tests/core_tests.cpp`: pruebas del núcleo, compiladas con GCC 13 en Linux (ver el total más abajo) |
| Deshacer/rehacer | COMPLETA Y VERIFICADA (modelo y ventana) | Mismas pruebas; confirmado a mano por el propietario en la ventana |
| Conversión de tiempos | COMPLETA Y VERIFICADA | Mismas pruebas; sin drop-frame |
| Ventana Qt con línea de tiempo de demostración | PARCIAL: compila, empaqueta y abre | Run #1 de *Windows build* en verde (commit 7b2298b): compila con MSVC y Qt 6.8, pasa `ctest`, el ZIP empaquetado arranca con `--version`. El propietario confirmó a mano que el ZIP descargado abre la interfaz. El propietario confirmó a mano que **Deshacer y Rehacer funcionan**. Sin confirmar a mano: Abrir/Guardar |
| Workflow de Windows | COMPLETA Y VERIFICADA (primera versión) | Run #1: éxito en 2 min 10 s; artefacto de 45,9 MB. Aviso de GitHub: Node.js 20 obsoleto en `checkout@v4`, `upload-artifact@v4` y `msvc-dev-cmd@v1` (sin efecto por ahora) |
| Viabilidad de MLT + FFmpeg en Windows | PARCIAL | El primer intento (adb081f) falló. Con UCRT64 (e767e61) el job `mlt` terminó en verde, y su workflow solo pasa si `ffprobe` encuentra el MP4 de 75 fotogramas. **No se ha leído la evidencia** (licencias, capacidades) |
| Formato de proyecto, guardar y abrir (núcleo) | COMPLETA Y VERIFICADA (núcleo) | 143 comprobaciones en total, 0 fallos (GCC 13, Linux): ida y vuelta, archivos dañados, esquema futuro, solapes, ids duplicados, guardado atómico y detección de medios que faltan. Documentado en `docs/PROJECT_FORMAT.md`. Limitación: rutas absolutas, sin relocalización |
| Botones Abrir/Guardar en la ventana | IMPLEMENTADA, PERO CON PRUEBAS PENDIENTES | Compila en Windows (*Windows build* en verde en ec2f8b7); falta probarlo a mano |
| Importación, biblioteca, vista previa | PENDIENTE | Fase 3 |
| Generador de MLT XML (línea de tiempo → render con `melt`) | IMPLEMENTADA, PERO CON PRUEBAS PENDIENTES | 159 comprobaciones en Linux (estructura del XML, huecos, escapes, errores, XML bien formado con Python). Falta el render real con `melt`: lo prueba `windows-msys2.yml`, aún sin ejecutar |
| Motor multimedia en la app (importar, vista previa) | PENDIENTE | Decisiones 0003 y 0004 |
| Exportación de video desde la app | PENDIENTE | Falta el botón y lanzar `melt` desde la app (Fase 3) |
| Asistente de IA | PENDIENTE | Fase 5 |

## Limitaciones conocidas
- La línea de tiempo de la ventana es una demostración con un clip de marcador (`demo.mp4`); no hay medios reales.
- Licencia: el propietario delegó la decisión; se adopta **GPLv3 con repositorio público** (MLT y FFmpeg con x264 son GPL). El texto `LICENSE` aún no está en el repositorio: hay que añadirlo desde la web de GitHub (Add file → Create new file → `LICENSE` → *Choose a license template*).
- Las acciones de GitHub usan versiones por etiqueta mayor (`@v4`); conviene fijarlas por SHA antes de la Fase 7.

## Próxima tarea
Ver el resultado de *Windows build (MSYS2 + MLT)*: compila con GCC, pasa pruebas, `melt` renderiza el MP4 del proyecto y el paquete funciona sin MSYS2. Si pasa: retirar el workflow MSVC, añadir el botón Exportar (lanza `melt`) y la importación con lectura de duración.
