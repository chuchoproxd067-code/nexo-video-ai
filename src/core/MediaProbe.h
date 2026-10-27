#pragma once
// Interpreta la salida de `melt <archivo> out=0 -consumer xml`, que describe un medio
// (duración, tamaño, pistas de audio y video). Es texto puro: el proceso `melt` lo
// lanza la aplicación; aquí solo se interpreta el resultado.
//
// Notas verificadas con la salida real de MLT 7.42 (ver STATUS.md):
//  - `length` está en fotogramas del PERFIL de melt, que se elige solo: el del propio
//    clip si es video, PAL de 25 fps si es audio puro. Hay que convertir con la tasa
//    que indica el elemento <profile>.
//  - En imágenes `length` no es una duración útil (15000); se usa una duración por defecto.
#include <string>

#include "core/Timecode.h"

namespace nexo {

enum class MediaKind { Video, Audio, Image };

struct MediaInfo {
    MediaKind kind = MediaKind::Video;
    Frame frames = 0;        // duración en fotogramas del proyecto
    double seconds = 0;
    int width = 0;           // 0 en audio
    int height = 0;
    bool hasVideo = false;
    bool hasAudio = false;
};

struct ProbeResult {
    bool ok = false;
    std::string error;
    MediaInfo info;
};

constexpr double kDefaultImageSeconds = 5.0;

ProbeResult parseMeltProbeXml(const std::string& xml, FrameRate projectFps);

}  // namespace nexo
