#pragma once
// Genera un documento MLT XML a partir de la línea de tiempo. Es texto puro: no
// depende de MLT, así que se puede probar en cualquier plataforma. El render real
// lo hace `melt` (ver DECISIONS/0004) leyendo este XML.
//
// Estructura generada:
//   pista 0  = fondo negro de la duración total (base del compositing)
//   pista i+1 = pista i del proyecto (las posteriores quedan por encima)
//   mix       = mezcla el audio de cada pista con la base
//   composite = superpone el video de cada pista de video con la base
// Las pistas de audio llevan hide="video".
#include <string>
#include <vector>

#include "core/Timeline.h"

namespace nexo {

enum class ExportFormat { Mp4, Mov, WebM, Gif, Mp3, Wav };

struct ExportSettings {
    int width = 1280;
    int height = 720;
    ExportFormat format = ExportFormat::Mp4;
    int quality = 1;          // 0 = alta, 1 = media, 2 = baja (solo formatos con pérdida)
};

const char* formatExtension(ExportFormat f);     // sin punto: "mp4"
const char* formatLabel(ExportFormat f);         // para mostrar al usuario
bool formatIsAudioOnly(ExportFormat f);
// Argumentos del consumidor de melt para escribir `outPath` en ese formato (desde "-consumer").
std::vector<std::string> meltConsumerArgs(const ExportSettings& settings, const std::string& outPath);

struct XmlResult {
    bool ok = false;
    std::string xml;
    std::string error;
};

XmlResult buildMltXml(const Timeline& timeline, const ExportSettings& settings);

}  // namespace nexo
