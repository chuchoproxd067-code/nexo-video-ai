#pragma once
// Subtítulos SRT: lectura, escritura y paso desde/hacia clips de texto de una pista de video.
#include <string>
#include <vector>

#include "core/Timeline.h"

namespace nexo {

struct SubtitleCue {
    Frame start = 0;   // en fotogramas de la secuencia
    Frame end = 0;     // exclusivo
    std::string text;  // puede tener varias líneas
};

// Lee un archivo SRT. Tolera BOM UTF-8, CRLF y numeración irregular. Devuelve false con `error`
// (con el número de bloque) si un bloque está mal formado; no devuelve resultados parciales.
bool parseSrt(const std::string& srt, FrameRate fps, std::vector<SubtitleCue>& out, std::string& error);

// Escribe SRT numerado desde 1.
std::string formatSrt(const std::vector<SubtitleCue>& cues, FrameRate fps);

// Los clips de texto de la pista, ordenados, como subtítulos.
std::vector<SubtitleCue> cuesFromTrack(const Timeline& tl, std::size_t track);

}  // namespace nexo
