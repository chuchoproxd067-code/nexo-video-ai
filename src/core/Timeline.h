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

#include "core/ClipProps.h"
#include "core/Timecode.h"

namespace nexo {

using ClipId = std::uint32_t;

enum class TrackKind { Video, Audio };
enum class ClipKind { Media, Text };

// Texto/título superpuesto (clip de tipo Text; no usa archivo).
struct TextProps {
    std::string text;
    std::string font = "Arial";
    double size = 8;                 // altura del texto en % de la altura del cuadro (1..50)
    std::uint32_t color = 0xFFFFFF;  // 0xRRGGBB
    double posX = 50, posY = 85;     // centro del texto en % del cuadro (0..100)
    bool bold = false;
    bool operator==(const TextProps& o) const {
        return text == o.text && font == o.font && size == o.size && color == o.color && posX == o.posX &&
               posY == o.posY && bold == o.bold;
    }
};

struct Marker {
    Frame frame = 0;
    std::string name;
};

struct Clip {
    ClipId id = 0;               // lo asigna Timeline::addClip
    std::string mediaPath;       // ruta del medio original (nunca se modifica)
    Frame sourceIn = 0;
    Frame duration = 0;
    Frame timelineStart = 0;
    double volume = 1.0;         // 0..4 (1 = sin cambio)
    Frame fadeIn = 0;            // fotogramas de fundido de entrada (audio y video)
    Frame fadeOut = 0;           // fotogramas de fundido de salida
    // Fundido cruzado con el clip anterior de la pista: los dos clips se solapan exactamente estos
    // fotogramas (timelineStart = fin del anterior - transitionIn). No necesita material extra.
    Frame transitionIn = 0;
    ClipKind kind = ClipKind::Media;
    TextProps text;              // solo si kind == Text
    ClipProps props;             // velocidad, transformación, color, efectos, croma, audio
    Frame timelineEnd() const { return timelineStart + duration; }
};

struct Track {
    std::string name;
    TrackKind kind = TrackKind::Video;
    std::vector<Clip> clips;     // ordenados por timelineStart
    bool muted = false;          // pista de audio silenciada (o audio de una pista de video)
    bool hidden = false;         // pista de video oculta
    bool locked = false;         // no admite cambios en sus clips
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
    // Pone (frames>0) o quita (0) el fundido cruzado con el clip anterior. Los clips deben estar
    // pegados; este y los siguientes de la pista se desplazan para conservar la continuidad.
    Result setTransition(ClipId id, Frame frames);
    // Reemplaza las propiedades del clip. Si cambian velocidad/inversa/congelado, la duración se
    // recalcula para conservar el mismo tramo del medio (error si chocaría con otro clip).
    Result setClipProps(ClipId id, const ClipProps& props);
    Result addTextClip(std::size_t track, const TextProps& text, Frame start, Frame duration);
    Result setText(ClipId id, const TextProps& text);
    // Elimina el clip y desplaza hacia atrás los siguientes de la misma pista.
    Result removeClipRipple(ClipId id);
    // Crea en `audioTrack` un clip con el audio del clip de video y silencia el original.
    Result detachAudio(ClipId id, std::size_t audioTrack);
    // Pistas: añadir/quitar (solo vacías) y banderas.
    Result removeTrack(std::size_t track);
    Result setTrackFlags(std::size_t track, bool muted, bool hidden, bool locked);
    Result renameTrack(std::size_t track, std::string name);
    // Marcadores (ordenados por fotograma).
    Result addMarker(Frame frame, std::string name);
    Result removeMarker(std::size_t index);
    const std::vector<Marker>& markers() const { return markers_; }
    Result setClipFx(ClipId id, double volume, Frame fadeIn, Frame fadeOut);

    // Comprueba invariantes (sin solapes, duraciones > 0, ids únicos).
    // Devuelve cadena vacía si todo es válido.
    std::string validate() const;

private:
    struct Location { std::size_t track; std::size_t index; };
    bool find(ClipId id, Location& out) const;
    bool overlaps(std::size_t track, Frame start, Frame end, ClipId ignore) const;
    void sortTrack(std::size_t track);
    std::string transitionProblem(std::size_t track) const;

    FrameRate fps_;
    std::vector<Track> tracks_;
    ClipId nextId_ = 1;
    std::vector<Marker> markers_;
    bool trackLocked(ClipId id) const;
};

}  // namespace nexo
