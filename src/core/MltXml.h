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

#include "core/Timeline.h"

namespace nexo {

struct ExportSettings {
    int width = 1280;
    int height = 720;
};

struct XmlResult {
    bool ok = false;
    std::string xml;
    std::string error;
};

XmlResult buildMltXml(const Timeline& timeline, const ExportSettings& settings);

}  // namespace nexo
