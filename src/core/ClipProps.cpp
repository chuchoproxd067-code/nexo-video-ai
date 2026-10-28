#include "core/ClipProps.h"

#include <algorithm>
#include <cmath>

namespace nexo {

namespace {
bool inRange(double v, double lo, double hi) { return v >= lo && v <= hi; }   // false con NaN
}

bool ClipProps::operator==(const ClipProps& o) const {
    if (speed != o.speed || reverse != o.reverse || freeze != o.freeze) return false;
    if (posX != o.posX || posY != o.posY || scale != o.scale || rotation != o.rotation || opacity != o.opacity) return false;
    if (cropLeft != o.cropLeft || cropRight != o.cropRight || cropTop != o.cropTop || cropBottom != o.cropBottom) return false;
    if (flipH != o.flipH || flipV != o.flipV) return false;
    if (brightness != o.brightness || contrast != o.contrast || saturation != o.saturation || temperature != o.temperature) return false;
    if (effects.size() != o.effects.size()) return false;
    for (std::size_t i = 0; i < effects.size(); ++i)
        if (effects[i].kind != o.effects[i].kind || effects[i].amount != o.effects[i].amount) return false;
    if (chroma.enabled != o.chroma.enabled || chroma.rgb != o.chroma.rgb || chroma.similarity != o.chroma.similarity ||
        chroma.blend != o.chroma.blend) return false;
    if (keyframes.size() != o.keyframes.size()) return false;
    for (std::size_t i = 0; i < keyframes.size(); ++i) {
        const Keyframe &a = keyframes[i], &b = o.keyframes[i];
        if (a.frame != b.frame || a.posX != b.posX || a.posY != b.posY || a.scale != b.scale ||
            a.rotation != b.rotation || a.opacity != b.opacity) return false;
    }
    return stabilize == o.stabilize && muted == o.muted && denoise == o.denoise && normalize == o.normalize &&
           pitch == o.pitch && bass == o.bass && treble == o.treble;
}

bool ClipProps::hasTransform() const {
    return posX != 0 || posY != 0 || scale != 100 || rotation != 0 || opacity != 1.0 || cropLeft != 0 ||
           cropRight != 0 || cropTop != 0 || cropBottom != 0 || flipH || flipV || !keyframes.empty();
}

std::string validateProps(const ClipProps& p, Frame clipDuration) {
    if (!inRange(p.speed, 0.1, 16)) return "La velocidad debe estar entre 0,1 y 16.";
    if (!inRange(p.posX, -200, 200) || !inRange(p.posY, -200, 200)) return "La posición debe estar entre -200 % y 200 %.";
    if (!inRange(p.scale, 1, 1000)) return "La escala debe estar entre 1 % y 1000 %.";
    if (!inRange(p.rotation, -360, 360)) return "La rotación debe estar entre -360 y 360 grados.";
    if (!inRange(p.opacity, 0, 1)) return "La opacidad debe estar entre 0 y 1.";
    for (double c : {p.cropLeft, p.cropRight, p.cropTop, p.cropBottom})
        if (!inRange(c, 0, 100)) return "El recorte debe estar entre 0 % y 100 %.";
    if (p.cropLeft + p.cropRight >= 100 || p.cropTop + p.cropBottom >= 100) return "El recorte no puede dejar el clip sin imagen.";
    if (!inRange(p.brightness, -1, 1)) return "El brillo debe estar entre -1 y 1.";
    if (!inRange(p.contrast, 0, 3)) return "El contraste debe estar entre 0 y 3.";
    if (!inRange(p.saturation, 0, 3)) return "La saturación debe estar entre 0 y 3.";
    if (!inRange(p.temperature, 2000, 12000)) return "La temperatura de color debe estar entre 2000 K y 12000 K.";
    if (p.effects.size() > 8) return "Como máximo 8 efectos por clip.";
    for (const Effect& e : p.effects)
        if (!inRange(e.amount, 0, 1)) return "La intensidad de un efecto debe estar entre 0 y 1.";
    if (!inRange(p.chroma.similarity, 0, 1) || !inRange(p.chroma.blend, 0, 1)) return "Los parámetros del croma deben estar entre 0 y 1.";
    if (p.chroma.rgb > 0xFFFFFF) return "Color de croma inválido.";
    Frame last = -1;
    for (const Keyframe& k : p.keyframes) {
        if (k.frame < 0 || k.frame >= clipDuration) return "Un fotograma clave cae fuera del clip.";
        if (k.frame <= last) return "Los fotogramas clave deben estar en orden estricto.";
        last = k.frame;
        if (!inRange(k.posX, -200, 200) || !inRange(k.posY, -200, 200) || !inRange(k.scale, 1, 1000) ||
            !inRange(k.rotation, -360, 360) || !inRange(k.opacity, 0, 1))
            return "Un fotograma clave tiene valores fuera de rango.";
    }
    if (!inRange(p.pitch, -12, 12)) return "El tono debe estar entre -12 y 12 semitonos.";
    if (p.pitch != 0 && (p.speed != 1.0 || p.reverse || p.freeze)) return "El cambio de tono solo funciona a velocidad normal.";
    if (!inRange(p.bass, -20, 20) || !inRange(p.treble, -20, 20)) return "Graves y agudos deben estar entre -20 dB y 20 dB.";
    return {};
}

Frame sourceSpan(const ClipProps& p, Frame duration) {
    if (p.freeze) return 1;
    return std::max<Frame>(1, static_cast<Frame>(std::llround(static_cast<double>(duration) * p.speed)));
}

Frame durationForSpan(const ClipProps& p, Frame span) {
    return std::max<Frame>(1, static_cast<Frame>(std::llround(static_cast<double>(span) / p.speed)));
}

const char* effectName(EffectKind k) {
    switch (k) {
        case EffectKind::Blur: return "blur";
        case EffectKind::Sharpen: return "sharpen";
        case EffectKind::Vignette: return "vignette";
        case EffectKind::Mosaic: return "mosaic";
        case EffectKind::Grayscale: return "grayscale";
        case EffectKind::Sepia: return "sepia";
        case EffectKind::Invert: return "invert";
        case EffectKind::Mirror: return "mirror";
    }
    return "blur";
}

bool effectFromName(const std::string& s, EffectKind& out) {
    for (EffectKind k : {EffectKind::Blur, EffectKind::Sharpen, EffectKind::Vignette, EffectKind::Mosaic,
                         EffectKind::Grayscale, EffectKind::Sepia, EffectKind::Invert, EffectKind::Mirror})
        if (s == effectName(k)) { out = k; return true; }
    return false;
}

}  // namespace nexo
