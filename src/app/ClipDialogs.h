#pragma once
// Diálogos de edición de clips y de exportación. No modifican el modelo: devuelven los valores y
// la ventana principal los aplica con History (así deshacer/rehacer sirve para todo).
#include <QString>
#include <QWidget>

#include "core/MltXml.h"
#include "core/Timeline.h"

namespace dialogs {

// Propiedades de un clip (tiempo, transformación, color, efectos, croma, audio, fotogramas clave).
// Solo muestra las pestañas que tienen sentido para el tipo de clip y de pista.
bool editProps(QWidget* parent, const nexo::Clip& clip, nexo::TrackKind kind, nexo::FrameRate fps, nexo::ClipProps& out);

// Texto/título: contenido, fuente, tamaño, color, posición.
bool editText(QWidget* parent, const QString& title, nexo::TextProps& text);

// Formato, calidad y resolución. Devuelve false si se cancela.
bool chooseExport(QWidget* parent, nexo::ExportSettings& settings, QString& outPath, const QString& defaultDir);

}  // namespace dialogs
