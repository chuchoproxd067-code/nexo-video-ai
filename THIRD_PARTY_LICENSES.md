# Dependencias y licencias

Evidencia: anotaciones del paso *Versions and licensing evidence* del workflow `windows-msys2.yml`
(commit 258b3a5), tomadas de los metadatos de los paquetes de MSYS2 y de `ffmpeg -version`.
Es un primer inventario, **no una auditoría legal completa**.

| Componente | Versión | Licencia declarada | Uso en NEXO |
|---|---|---|---|
| MLT | 7.42.0 | LGPL-2.1-or-later (el módulo `libmltplusgpl` y otros módulos pueden ser GPL; sin revisar uno por uno) | `melt.exe` y módulos, como proceso aparte (ver DECISIONS/0004) |
| FFmpeg | 9.0.2 | GPL-3.0-or-later; configuración con `--enable-gpl --enable-version3 --enable-libx264 --enable-libx265` | Códecs, enlazado por los módulos de MLT |
| Qt 6 (base) | 6.12.0 | LGPL-3.0-only con excepción Qt-GPL, más licencias de componentes internos (BSD, Apache-2.0, etc.) | Interfaz, enlace dinámico |
| Resto de DLL del paquete (~160) | — | Sin inventariar | Dependencias de las anteriores (fftw, libsamplerate, sox, rubberband, etc.) |

## Consecuencias
- El paquete distribuido contiene FFmpeg con GPLv3, por lo que **NEXO VIDEO AI se distribuye bajo GPLv3 con su código fuente disponible** (repositorio público). Es la decisión adoptada (ver STATUS.md).
- x264 y x265 pueden tener patentes de códec en algunos países; es una cuestión legal aparte que no se ha analizado.
- Pendiente: obtener la licencia de cada DLL redistribuida y añadir los textos de licencia al paquete; añadir el archivo `LICENSE` al repositorio.
