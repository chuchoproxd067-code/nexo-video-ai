#pragma once
// Proyecto = nombre + biblioteca de medios + línea de tiempo, con guardado y carga
// en un formato JSON versionado (ver docs/PROJECT_FORMAT.md).
//
// Principios:
//  - Los medios originales nunca se modifican; el proyecto solo guarda rutas.
//  - Lo que se carga se valida entero: un archivo dañado produce un error claro y
//    nunca un proyecto a medias.
//  - Un proyecto con `schema` mayor que el soportado se rechaza (no se adivina).
#include <string>
#include <vector>

#include "core/Timeline.h"

namespace nexo {

constexpr int kProjectSchemaVersion = 2;   // 2: propiedades de clip, textos, banderas de pista y marcadores (los proyectos de esquema 1 se abren igual)

struct Project {
    std::string name = "Sin título";
    std::vector<std::string> media;   // rutas de la biblioteca (sin duplicados)
    Timeline timeline;

    // Añade a la biblioteca si no estaba. Devuelve false si la ruta está vacía.
    bool addMedia(const std::string& path);
    // Rutas (de la biblioteca y de los clips) que no existen en disco, sin repetir.
    std::vector<std::string> missingMedia() const;
};

std::string toJson(const Project& p);
bool fromJson(const std::string& text, Project& out, std::string& error);

// Guarda escribiendo primero un archivo temporal y renombrándolo, para no dejar
// el proyecto anterior a medias si falla la escritura.
bool saveProject(const Project& p, const std::string& path, std::string& error);
bool loadProject(const std::string& path, Project& out, std::string& error);

}  // namespace nexo
