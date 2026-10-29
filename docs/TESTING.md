# Pruebas

| Capa | Dónde | Qué comprueba |
|---|---|---|
| Núcleo (C++17, sin Qt) | `tests/core_tests.cpp` (`ctest`, ~385 comprobaciones) | Modelo, History, proyecto JSON, MLT XML (texto del XML), propiedades, SRT, estilos de transición |
| Dibujado de texto | `src/app/TextRenderTest.cpp` (`ctest`, plataforma Qt *offscreen*) | El PNG tiene letras, color y posición correctos |
| Render real | `tools/ci_props_check.sh` (paso «Verify clip properties with real renders» de `windows-msys2.yml`) | Cada propiedad se renderiza con `melt` y se mide con `ffmpeg`/`ffprobe` (luminancia, PSNR, volumen, formatos). Resultado: anotaciones «Propiedades de clip N» |
| Laboratorio | `lab/*.sh` + `mlt-lab.yml` | Experimentos con melt para fijar la sintaxis ANTES de generarla (historial en `lab/old/`) |
| Paquete | pasos de humo de `windows-msys2.yml` | El ZIP arranca y `melt` empaquetado renderiza sin MSYS2 en el PATH |
| A mano | propietario | Todo lo marcado «pruebas a mano pendientes» en STATUS.md |

Reglas: una función solo es «VERIFICADA» si hay una medición reproducible; las mediciones del
render no sustituyen probar la interfaz a mano.
