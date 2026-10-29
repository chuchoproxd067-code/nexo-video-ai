#pragma once
// Dibuja los clips de texto/título como PNG RGBA a tamaño completo del cuadro. El núcleo no
// dibuja texto: genera el XML apuntando a estos PNG (ExportSettings::textImages) y melt los
// compone como cualquier otra imagen con canal alfa.
#include <QString>

#include "core/MltXml.h"

namespace textrender {

// Escribe `path` (PNG, width x height, fondo transparente). false si no se pudo.
bool renderPng(const nexo::TextProps& text, int width, int height, const QString& path);

// Dibuja todos los clips de texto de la línea de tiempo en `dir` y rellena settings.textImages.
bool prepare(const nexo::Timeline& timeline, nexo::ExportSettings& settings, const QString& dir, QString& error);

}  // namespace textrender
