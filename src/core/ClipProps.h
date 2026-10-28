#pragma once
// Propiedades de edición de un clip (velocidad, transformación, color, efectos, croma, audio).
// Independiente de Qt y de MLT. Todos los valores tienen un rango validado en validateProps().
#include <cstdint>
#include <string>
#include <vector>

#include "core/Timecode.h"

namespace nexo {

// Efectos de video con un solo parámetro `amount` (0..1). La lista es cerrada para que cada uno
// tenga una traducción verificada a MLT.
enum class EffectKind { Blur, Sharpen, Vignette, Mosaic, Grayscale, Sepia, Invert, Mirror };

struct Effect {
    EffectKind kind = EffectKind::Blur;
    double amount = 0.5;
};

// Posición/escala/rotación/opacidad en un instante del clip. `frame` es relativo al inicio del
// clip en la secuencia (0 = primer fotograma del clip). Se interpola linealmente.
struct Keyframe {
    Frame frame = 0;
    double posX = 0, posY = 0;     // % del tamaño del cuadro, desde el centro
    double scale = 100;            // %
    double rotation = 0;           // grados
    double opacity = 1.0;          // 0..1
};

struct ChromaKey {
    bool enabled = false;
    std::uint32_t rgb = 0x00FF00;  // color a hacer transparente (0xRRGGBB)
    double similarity = 0.3;       // 0..1
    double blend = 0.1;            // 0..1
};

struct ClipProps {
    // --- tiempo ---
    double speed = 1.0;            // 0.1..16 (1 = normal)
    bool reverse = false;
    bool freeze = false;           // mantiene el primer fotograma (sourceIn) todo el clip; sin audio

    // --- transformación (video) ---
    double posX = 0, posY = 0;     // % del cuadro, desde el centro: -200..200
    double scale = 100;            // 1..1000 %
    double rotation = 0;           // -360..360 grados
    double opacity = 1.0;          // 0..1
    double cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;   // % 0..100 (L+R < 100, T+B < 100)
    bool flipH = false, flipV = false;

    // --- color ---
    double brightness = 0;         // -1..1
    double contrast = 1;           // 0..3
    double saturation = 1;         // 0..3
    double temperature = 6500;     // 2000..12000 K (6500 = neutro)

    std::vector<Effect> effects;   // como máximo 8
    ChromaKey chroma;
    std::vector<Keyframe> keyframes;   // estrictamente crecientes en `frame`
    bool stabilize = false;

    // --- audio ---
    bool muted = false;
    bool denoise = false;
    bool normalize = false;
    double pitch = 0;              // semitonos -12..12 (solo velocidad normal)
    double bass = 0, treble = 0;   // dB -20..20

    bool operator==(const ClipProps& o) const;
    bool isDefault() const { return *this == ClipProps(); }
    bool hasTransform() const;     // posición/escala/rotación/opacidad/recorte/volteo distintos de lo neutro
};

// Devuelve "" si todo está en rango; si no, el motivo en español.
// `clipDuration` limita los fotogramas clave.
std::string validateProps(const ClipProps& p, Frame clipDuration);

// Fotogramas del medio que usa un clip de `duration` fotogramas de secuencia.
Frame sourceSpan(const ClipProps& p, Frame duration);
// Inversa: fotogramas de secuencia que ocupan `span` fotogramas del medio.
Frame durationForSpan(const ClipProps& p, Frame span);

const char* effectName(EffectKind k);            // identificador estable para guardar
bool effectFromName(const std::string& s, EffectKind& out);

}  // namespace nexo
