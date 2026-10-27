#pragma once
// Modelo de la línea de tiempo. Independiente de Qt y de MLT.
//
// Posiciones (ambas en fotogramas):
//   - sourceIn:      punto de entrada DENTRO del archivo fuente.
//   - timelineStart: posición del clip DENTRO de la secuencia.
// Un clip muestra los fotogramas [sourceIn, sourceIn + duration) del medio,
// colocados en [timelineStart, timelineStart + duration) de la secuencia.
// Dentro de una pista los clips no pueden solaparse.
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/Timecode.h"

namespace nexo {

using ClipId = std::uint32_t;

enum class TrackKind { Video, Audio };

struct Clip {
    ClipId id = 0;               // lo asigna Timeline::addClip
    std::string mediaPath;       // ruta del medio original (nunca se modifica)
    Frame sourceIn = 0;
    Frame duration = 0;
    Frame timelineStart = 0;
    double volume = 1.0;         // 0..4 (1 = sin cambio)
    Frame fadeIn = 0;            // fotogramas de fundido de entrada (audio y video)
    Frame fadeOut = 0;           // fotogramas de fundido de salida
    Frame timelineEnd() const { return timelineStart + duration; }
};

struct Track {
    std::string name;
    TrackKind kind = TrackKind::Video;
    std::vector<Clip> clips;     // ordenados por timelineStart
};

// Resultado de una operación: ok o mensaje de error explícito.
struct Result {
    bool ok = true;
    std::string error;
    ClipId clip = 0;             // clip creado o afectado, si aplica
    static Result success(ClipId c = 0) { return {true, {}, c}; }
    static Result failure(std::string msg) { return {false, std::move(msg), 0}; }
};

class Timeline {
public:
    explicit Timeline(FrameRate fps = {30, 1}) : fps_(fps) {}

    FrameRate frameRate() const { return fps_; }
    std::size_t addTrack(std::string name, TrackKind kind);
    std::size_t trackCount() const { return tracks_.size(); }
    const Track& track(std::size_t index) const { return tracks_.at(index); }

    // Fin de la secuencia = fin del último clip de cualquier pista.
    Frame duration() const;

    // Operaciones de edición. Ninguna modifica el estado si devuelve error.
    Result addClip(std::size_t track, Clip clip);
    // Inserta conservando el id del clip (para cargar proyectos guardados).
    // El id debe ser > 0 y no estar en uso.
    Result insertClip(std::size_t track, Clip clip);
    Result moveClip(ClipId id, std::size_t toTrack, Frame newStart);
    // Mueve el borde izquierdo (delta>0 acorta, delta<0 alarga si hay material y espacio).
    Result trimStart(ClipId id, Frame delta);
    // Mueve el borde derecho (delta<0 acorta, delta>0 alarga si hay espacio).
    Result trimEnd(ClipId id, Frame delta);
    // Divide en `at` (posición en la secuencia, estrictamente dentro del clip).
    Result splitClip(ClipId id, Frame at);
    Result removeClip(ClipId id);
    // Volumen y fundidos del clip. Error si volume fuera de [0,4], fundidos negativos o
    // fadeIn + fadeOut mayores que la duración.
    Result setClipFx(ClipId id, double volume, Frame fadeIn, Frame fadeOut);

    // Comprueba invariantes (sin solapes, duraciones > 0, ids únicos).
    // Devuelve cadena vacía si todo es válido.
    std::string validate() const;

private:
    struct Location { std::size_t track; std::size_t index; };
    bool find(ClipId id, Location& out) const;
    bool overlaps(std::size_t track, Frame start, Frame end, ClipId ignore) const;
    void sortTrack(std::size_t track);

    FrameRate fps_;
    std::vector<Track> tracks_;
    ClipId nextId_ = 1;
};

}  // namespace nexo
