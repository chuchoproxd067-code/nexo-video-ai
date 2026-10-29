# NEXO VIDEO AI

Editor de video de escritorio para Windows con asistente de IA (en desarrollo).

> **Estado real:** editor de video funcional en construcción. Importar, línea de tiempo multipista (zoom, imán, marcadores, pistas), propiedades por clip (velocidad, inversa, congelar, transformación, fotogramas clave, color, efectos, croma, audio), texto y subtítulos, 15 estilos de transición, y exportación a MP4/MOV/WebM/GIF/MP3/WAV con `melt`. Las propiedades se miden con renders reales en CI; **el propietario aún no ha probado a mano esta versión.** No hay reproducción en tiempo real ni asistente de IA todavía. Ver [STATUS.md](STATUS.md).

## Descargar una compilación

1. Abre la pestaña **Actions** y entra en la ejecución más reciente de *Windows build (MSYS2 + MLT)*.
2. Si tiene el check verde, baja a **Artifacts** y descarga `NEXO-VIDEO-AI-msys2-windows-x64-<commit>`.
3. Descomprime todo y ejecuta `Iniciar NEXO VIDEO AI.cmd` (o `bin\nexo-video-ai.exe`).

Para lanzar una compilación a mano: **Actions → Windows build (MSYS2 + MLT) → Run workflow**. El ZIP incluye `melt.exe` y los módulos de MLT, con los que se exportará el video.

## Documentación
- [STATUS.md](STATUS.md): qué funciona y qué falta, con evidencia.
- [ROADMAP.md](ROADMAP.md): las siete fases.
- [BUILD.md](BUILD.md): compilación y límites de la infraestructura.
- [ARCHITECTURE.md](ARCHITECTURE.md) y [DECISIONS/](DECISIONS/).
- [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
