#include "core/Timeline.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace nexo {

std::size_t Timeline::addTrack(std::string name, TrackKind kind) {
    tracks_.push_back(Track{std::move(name), kind, {}});
    return tracks_.size() - 1;
}

Frame Timeline::duration() const {
    Frame end = 0;
    for (const Track& t : tracks_)
        for (const Clip& c : t.clips) end = std::max(end, c.timelineEnd());
    return end;
}

bool Timeline::find(ClipId id, Location& out) const {
    for (std::size_t t = 0; t < tracks_.size(); ++t)
        for (std::size_t i = 0; i < tracks_[t].clips.size(); ++i)
            if (tracks_[t].clips[i].id == id) {
                out = {t, i};
                return true;
            }
    return false;
}

bool Timeline::overlaps(std::size_t track, Frame start, Frame end, ClipId ignore) const {
    for (const Clip& c : tracks_[track].clips) {
        if (c.id == ignore) continue;
        if (start < c.timelineEnd() && c.timelineStart < end) return true;
    }
    return false;
}

void Timeline::sortTrack(std::size_t track) {
    auto& clips = tracks_[track].clips;
    std::sort(clips.begin(), clips.end(),
              [](const Clip& a, const Clip& b) { return a.timelineStart < b.timelineStart; });
}

std::string Timeline::transitionProblem(std::size_t track) const {
    const auto& cl = tracks_[track].clips;
    for (std::size_t i = 0; i < cl.size(); ++i) {
        const Clip& c = cl[i];
        if (c.transitionIn < 0) return "Transición negativa.";
        for (std::size_t j = 0; j + 1 < i; ++j)
            if (cl[j].timelineEnd() > c.timelineStart) return "Clips solapados en la pista " + std::to_string(track) + ".";
        if (i > 0 && cl[i - 1].timelineEnd() > c.timelineStart) {
            if (c.transitionIn == 0 || cl[i - 1].timelineEnd() - c.timelineStart != c.transitionIn)
                return "Clips solapados en la pista " + std::to_string(track) + ".";
        }
        if (c.transitionIn > 0) {
            if (i == 0) return "Una transición necesita un clip anterior.";
            const Clip& prev = cl[i - 1];
            if (prev.timelineEnd() - c.timelineStart != c.transitionIn) return "La transición no coincide con el solape.";
            if (prev.duration < prev.transitionIn + c.transitionIn) return "La transición es más larga que el clip anterior.";
        }
        const Frame nextN = i + 1 < cl.size() ? cl[i + 1].transitionIn : 0;
        if (c.duration < c.transitionIn + nextN) return "Las transiciones del clip son más largas que el clip.";
    }
    return {};
}

Result Timeline::addClip(std::size_t track, Clip clip) {
    clip.id = nextId_;
    Result r = insertClip(track, std::move(clip));
    return r;
}

Result Timeline::insertClip(std::size_t track, Clip clip) {
    if (clip.id == 0) return Result::failure("El clip necesita un identificador.");
    Location existing;
    if (find(clip.id, existing)) return Result::failure("El identificador de clip ya está en uso.");
    if (track >= tracks_.size()) return Result::failure("La pista no existe.");
    if (clip.duration <= 0) return Result::failure("La duración del clip debe ser mayor que 0.");
    if (clip.sourceIn < 0) return Result::failure("El punto de entrada no puede ser negativo.");
    if (clip.timelineStart < 0) return Result::failure("La posición no puede ser negativa.");
    if (clip.kind == ClipKind::Media && clip.mediaPath.empty()) return Result::failure("El clip no tiene ruta de medio.");
    if (clip.kind == ClipKind::Text && clip.text.text.empty()) return Result::failure("El texto no puede estar vacío.");
    if (tracks_[track].locked) return Result::failure("La pista está bloqueada.");
    {
        const std::string pp = validateProps(clip.props, clip.duration);
        if (!pp.empty()) return Result::failure(pp);
    }
    if (clip.transitionIn < 0) return Result::failure("La transición no puede ser negativa.");
    if (clip.transitionIn == 0 && overlaps(track, clip.timelineStart, clip.timelineEnd(), 0))
        return Result::failure("El clip se solapa con otro en la pista.");
    const std::vector<Clip> backup = tracks_[track].clips;
    const ClipId backupNext = nextId_;
    if (clip.id >= nextId_) nextId_ = clip.id + 1;
    tracks_[track].clips.push_back(clip);
    sortTrack(track);
    if (clip.transitionIn > 0) {   // solo se admite el solape de una transición bien formada
        const std::string problem = transitionProblem(track);
        if (!problem.empty()) {
            tracks_[track].clips = backup;
            nextId_ = backupNext;
            return Result::failure(problem);
        }
    }
    return Result::success(clip.id);
}

Result Timeline::moveClip(ClipId id, std::size_t toTrack, Frame newStart) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (toTrack >= tracks_.size()) return Result::failure("La pista de destino no existe.");
    if (newStart < 0) return Result::failure("La posición no puede ser negativa.");
    if (tracks_[toTrack].kind != tracks_[loc.track].kind)
        return Result::failure("No se puede mover un clip entre pistas de distinto tipo.");
    if (tracks_[loc.track].locked || tracks_[toTrack].locked) return Result::failure("La pista está bloqueada.");
    {
        const auto& cl = tracks_[loc.track].clips;
        if (cl[loc.index].transitionIn > 0 || (loc.index + 1 < cl.size() && cl[loc.index + 1].transitionIn > 0))
            return Result::failure("Quita la transición de este clip antes de moverlo.");
    }
    Clip clip = tracks_[loc.track].clips[loc.index];
    if (overlaps(toTrack, newStart, newStart + clip.duration, id))
        return Result::failure("El clip se solaparía con otro en la pista de destino.");
    tracks_[loc.track].clips.erase(tracks_[loc.track].clips.begin() + static_cast<std::ptrdiff_t>(loc.index));
    clip.timelineStart = newStart;
    tracks_[toTrack].clips.push_back(clip);
    sortTrack(toTrack);
    return Result::success(id);
}

namespace {
// Tras recortar, los fundidos no pueden pasar de la duración del clip.
void clampFades(Clip& c) {
    c.fadeIn = std::min(c.fadeIn, c.duration);
    c.fadeOut = std::min(c.fadeOut, c.duration - c.fadeIn);
}

// Los fotogramas clave que quedan fuera del clip tras recortar se descartan.
void clampKeyframes(Clip& c) {
    auto& k = c.props.keyframes;
    k.erase(std::remove_if(k.begin(), k.end(), [&](const Keyframe& x) { return x.frame < 0 || x.frame >= c.duration; }), k.end());
}

Frame scaled(Frame delta, double speed) { return static_cast<Frame>(std::llround(static_cast<double>(delta) * speed)); }
}  // namespace

bool Timeline::trackLocked(ClipId id) const {
    Location loc;
    return find(id, loc) && tracks_[loc.track].locked;
}

Result Timeline::setTransition(ClipId id, Frame frames) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (frames < 0) return Result::failure("La transición no puede ser negativa.");
    if (tracks_[loc.track].locked) return Result::failure("La pista está bloqueada.");
    auto& clips = tracks_[loc.track].clips;
    if (loc.index == 0) return Result::failure("La transición necesita un clip anterior en la pista.");
    const Clip& prev = clips[loc.index - 1];
    Clip& c = clips[loc.index];
    if (frames == c.transitionIn) return Result::success(id);
    if (c.transitionIn == 0 && prev.timelineEnd() != c.timelineStart)
        return Result::failure("Los clips deben estar pegados (sin hueco) para poner una transición.");
    const std::vector<Clip> backup = clips;
    const Frame newStart = prev.timelineEnd() - frames;
    const Frame delta = newStart - c.timelineStart;
    for (std::size_t i = loc.index; i < clips.size(); ++i) clips[i].timelineStart += delta;
    clips[loc.index].transitionIn = frames;
    std::string problem = transitionProblem(loc.track);
    if (problem.empty() && clips[loc.index].timelineStart < 0) problem = "La posición no puede ser negativa.";
    if (!problem.empty()) { clips = backup; return Result::failure(problem); }
    return Result::success(id);
}

namespace {
std::string textProblem(const TextProps& t) {
    if (t.text.empty()) return "El texto no puede estar vacío.";
    if (t.font.empty()) return "Falta la fuente.";
    if (!(t.size >= 1 && t.size <= 50)) return "El tamaño del texto debe estar entre 1 % y 50 % de la altura.";
    if (!(t.posX >= 0 && t.posX <= 100) || !(t.posY >= 0 && t.posY <= 100)) return "La posición del texto debe estar dentro del cuadro (0-100 %).";
    if (t.color > 0xFFFFFF) return "Color de texto inválido.";
    return {};
}
}  // namespace

Result Timeline::setClipProps(ClipId id, const ClipProps& requested) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (tracks_[loc.track].locked) return Result::failure("La pista está bloqueada.");
    auto& clips = tracks_[loc.track].clips;
    const std::vector<Clip> backup = clips;
    Clip& c = clips[loc.index];
    ClipProps np = requested;
    const bool timeChanged = np.speed != c.props.speed || np.reverse != c.props.reverse || np.freeze != c.props.freeze;
    if (c.kind == ClipKind::Text && (np.speed != 1.0 || np.reverse || np.freeze || np.pitch != 0))
        return Result::failure("Un texto no admite cambios de velocidad.");
    Frame newDur = c.duration;
    if (c.kind == ClipKind::Media && timeChanged && !c.props.freeze && !np.freeze)
        newDur = durationForSpan(np, sourceSpan(c.props, c.duration));   // mismo tramo del medio a otra velocidad
    if (newDur != c.duration && c.duration > 0)                         // los fotogramas clave siguen a la imagen
        for (Keyframe& k : np.keyframes)
            k.frame = static_cast<Frame>(std::llround(static_cast<double>(k.frame) * static_cast<double>(newDur) / static_cast<double>(c.duration)));
    {
        const std::string problem = validateProps(np, newDur);
        if (!problem.empty()) return Result::failure(problem);
    }
    if (newDur > c.duration && overlaps(loc.track, c.timelineEnd(), c.timelineStart + newDur, id))
        return Result::failure("No hay espacio libre para el clip a esta velocidad.");
    c.props = np;
    c.duration = newDur;
    clampFades(c);
    const std::string problem = transitionProblem(loc.track);
    if (!problem.empty()) { clips = backup; return Result::failure(problem); }
    return Result::success(id);
}

Result Timeline::addTextClip(std::size_t track, const TextProps& text, Frame start, Frame duration) {
    if (track >= tracks_.size()) return Result::failure("La pista no existe.");
    if (tracks_[track].kind != TrackKind::Video) return Result::failure("Los textos solo van en pistas de video.");
    const std::string problem = textProblem(text);
    if (!problem.empty()) return Result::failure(problem);
    Clip c;
    c.kind = ClipKind::Text;
    c.text = text;
    c.sourceIn = 0;
    c.timelineStart = start;
    c.duration = duration;
    return addClip(track, c);
}

Result Timeline::setText(ClipId id, const TextProps& text) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (tracks_[loc.track].locked) return Result::failure("La pista está bloqueada.");
    Clip& c = tracks_[loc.track].clips[loc.index];
    if (c.kind != ClipKind::Text) return Result::failure("El clip no es un texto.");
    const std::string problem = textProblem(text);
    if (!problem.empty()) return Result::failure(problem);
    c.text = text;
    return Result::success(id);
}

Result Timeline::removeClipRipple(ClipId id) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (tracks_[loc.track].locked) return Result::failure("La pista está bloqueada.");
    auto& clips = tracks_[loc.track].clips;
    const Clip removed = clips[loc.index];
    clips.erase(clips.begin() + static_cast<std::ptrdiff_t>(loc.index));
    // Todos los clips siguientes se desplazan lo mismo. Si el siguiente se mezclaba con el eliminado,
    // pierde esa transición y queda pegado al clip anterior.
    Frame shift = removed.duration - removed.transitionIn;
    if (loc.index < clips.size() && clips[loc.index].transitionIn > 0) {
        shift -= clips[loc.index].transitionIn;
        clips[loc.index].transitionIn = 0;
    }
    for (std::size_t i = loc.index; i < clips.size(); ++i) clips[i].timelineStart -= shift;
    return Result::success(id);
}

Result Timeline::detachAudio(ClipId id, std::size_t audioTrack) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (audioTrack >= tracks_.size() || tracks_[audioTrack].kind != TrackKind::Audio)
        return Result::failure("Elige una pista de audio de destino.");
    if (tracks_[loc.track].kind != TrackKind::Video) return Result::failure("Solo se separa el audio de clips de video.");
    if (tracks_[loc.track].locked || tracks_[audioTrack].locked) return Result::failure("La pista está bloqueada.");
    const Clip src = tracks_[loc.track].clips[loc.index];
    if (src.kind != ClipKind::Media) return Result::failure("Un texto no tiene audio.");
    if (src.props.muted) return Result::failure("El audio de este clip ya está separado o silenciado.");
    if (src.props.freeze) return Result::failure("Un clip congelado no tiene audio.");
    Clip a;
    a.mediaPath = src.mediaPath;
    a.sourceIn = src.sourceIn;
    a.duration = src.duration;
    a.timelineStart = src.timelineStart;
    a.volume = src.volume;
    a.fadeIn = src.fadeIn;
    a.fadeOut = src.fadeOut;
    a.props.speed = src.props.speed;
    a.props.reverse = src.props.reverse;
    a.props.denoise = src.props.denoise;
    a.props.normalize = src.props.normalize;
    a.props.pitch = src.props.pitch;
    a.props.bass = src.props.bass;
    a.props.treble = src.props.treble;
    const Result r = addClip(audioTrack, a);
    if (!r.ok) return r;
    for (Clip& x : tracks_[loc.track].clips) if (x.id == id) x.props.muted = true;
    return r;
}

Result Timeline::removeTrack(std::size_t track) {
    if (track >= tracks_.size()) return Result::failure("La pista no existe.");
    if (!tracks_[track].clips.empty()) return Result::failure("Solo se pueden quitar pistas vacías.");
    if (tracks_[track].locked) return Result::failure("La pista está bloqueada.");
    tracks_.erase(tracks_.begin() + static_cast<std::ptrdiff_t>(track));
    return Result::success();
}

Result Timeline::setTrackFlags(std::size_t track, bool muted, bool hidden, bool locked) {
    if (track >= tracks_.size()) return Result::failure("La pista no existe.");
    tracks_[track].muted = muted;
    tracks_[track].hidden = hidden;
    tracks_[track].locked = locked;
    return Result::success();
}

Result Timeline::renameTrack(std::size_t track, std::string name) {
    if (track >= tracks_.size()) return Result::failure("La pista no existe.");
    if (name.empty() || name.size() > 60) return Result::failure("El nombre debe tener entre 1 y 60 caracteres.");
    tracks_[track].name = std::move(name);
    return Result::success();
}

Result Timeline::addMarker(Frame frame, std::string name) {
    if (frame < 0) return Result::failure("La posición del marcador no puede ser negativa.");
    if (name.size() > 100) return Result::failure("El nombre del marcador es demasiado largo.");
    for (Marker& m : markers_) if (m.frame == frame) { m.name = std::move(name); return Result::success(); }
    markers_.push_back({frame, std::move(name)});
    std::sort(markers_.begin(), markers_.end(), [](const Marker& a, const Marker& b) { return a.frame < b.frame; });
    return Result::success();
}

Result Timeline::removeMarker(std::size_t index) {
    if (index >= markers_.size()) return Result::failure("El marcador no existe.");
    markers_.erase(markers_.begin() + static_cast<std::ptrdiff_t>(index));
    return Result::success();
}

Result Timeline::setClipFx(ClipId id, double volume, Frame fadeIn, Frame fadeOut) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    Clip& c = tracks_[loc.track].clips[loc.index];
    if (!(volume >= 0.0 && volume <= 4.0)) return Result::failure("El volumen debe estar entre 0 y 4.");
    if (fadeIn < 0 || fadeOut < 0) return Result::failure("Los fundidos no pueden ser negativos.");
    if (fadeIn + fadeOut > c.duration) return Result::failure("Los fundidos suman más que la duración del clip.");
    if (tracks_[loc.track].locked) return Result::failure("La pista está bloqueada.");
    c.volume = volume;
    c.fadeIn = fadeIn;
    c.fadeOut = fadeOut;
    return Result::success(id);
}

Result Timeline::trimStart(ClipId id, Frame delta) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (tracks_[loc.track].locked) return Result::failure("La pista está bloqueada.");
    Clip& c = tracks_[loc.track].clips[loc.index];
    if (c.transitionIn > 0) return Result::failure("Quita la transición de entrada antes de recortar el inicio.");
    const Frame newStart = c.timelineStart + delta;
    const Frame newDur = c.duration - delta;
    // Normal: el inicio del clip es el inicio del tramo del medio. Inversa: es su final (el punto de
    // entrada no cambia). Congelado y texto: no hay avance en el medio.
    const bool moveIn = c.kind == ClipKind::Media && !c.props.reverse && !c.props.freeze;
    const Frame newIn = moveIn ? c.sourceIn + scaled(delta, c.props.speed) : c.sourceIn;
    if (newDur <= 0) return Result::failure("El recorte dejaría el clip sin duración.");
    if (newIn < 0) return Result::failure("No hay material anterior en el medio fuente.");
    if (newStart < 0) return Result::failure("El recorte lo sacaría del inicio de la secuencia.");
    if (delta < 0 && overlaps(loc.track, newStart, c.timelineStart, id))
        return Result::failure("No hay espacio libre para alargar el clip.");
    const Clip before = c;
    c.timelineStart = newStart;
    c.sourceIn = newIn;
    c.duration = newDur;
    for (Keyframe& k : c.props.keyframes) k.frame -= delta;
    clampKeyframes(c);
    clampFades(c);
    sortTrack(loc.track);
    const std::string problem = transitionProblem(loc.track);
    if (!problem.empty()) {
        for (Clip& x : tracks_[loc.track].clips) if (x.id == id) x = before;
        sortTrack(loc.track);
        return Result::failure(problem);
    }
    return Result::success(id);
}

Result Timeline::trimEnd(ClipId id, Frame delta) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (tracks_[loc.track].locked) return Result::failure("La pista está bloqueada.");
    Clip& c = tracks_[loc.track].clips[loc.index];
    if (loc.index + 1 < tracks_[loc.track].clips.size() && tracks_[loc.track].clips[loc.index + 1].transitionIn > 0)
        return Result::failure("Quita la transición del clip siguiente antes de recortar el final.");
    const Frame newDur = c.duration + delta;
    if (newDur <= 0) return Result::failure("El recorte dejaría el clip sin duración.");
    if (delta > 0 && overlaps(loc.track, c.timelineEnd(), c.timelineEnd() + delta, id))
        return Result::failure("No hay espacio libre para alargar el clip.");
    // Inversa: el final del clip es el principio del tramo del medio, así que alargar retrocede el punto de entrada.
    const bool reverseMedia = c.kind == ClipKind::Media && c.props.reverse && !c.props.freeze;
    const Frame newIn = reverseMedia ? c.sourceIn - scaled(delta, c.props.speed) : c.sourceIn;
    if (newIn < 0) return Result::failure("No hay material anterior en el medio fuente.");
    const Clip before = c;
    c.duration = newDur;
    c.sourceIn = newIn;
    clampKeyframes(c);
    clampFades(c);
    const std::string problem = transitionProblem(loc.track);
    if (!problem.empty()) { c = before; return Result::failure(problem); }
    return Result::success(id);
}

Result Timeline::splitClip(ClipId id, Frame at) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (tracks_[loc.track].locked) return Result::failure("La pista está bloqueada.");
    Clip& c = tracks_[loc.track].clips[loc.index];
    const Clip original = c;
    if (at <= c.timelineStart || at >= c.timelineEnd())
        return Result::failure("La posición de corte debe estar dentro del clip.");
    Clip right = c;
    const Frame leftDur = at - c.timelineStart;
    right.id = nextId_++;
    right.timelineStart = at;
    right.duration = c.duration - leftDur;
    // Reparto del tramo del medio entre las dos mitades.
    if (c.kind == ClipKind::Media && !c.props.freeze) {
        const Frame span = sourceSpan(c.props, c.duration);
        const Frame leftSpan = scaled(leftDur, c.props.speed);
        if (c.props.reverse) c.sourceIn = original.sourceIn + span - leftSpan;   // la izquierda muestra el final del tramo
        else right.sourceIn = original.sourceIn + leftSpan;
    }
    // Fotogramas clave: cada mitad se queda con los suyos (los de la derecha, relativos a su nuevo inicio).
    right.props.keyframes.clear();
    for (const Keyframe& k : original.props.keyframes)
        if (k.frame >= leftDur) { Keyframe n = k; n.frame -= leftDur; right.props.keyframes.push_back(n); }
    c.props.keyframes.erase(std::remove_if(c.props.keyframes.begin(), c.props.keyframes.end(),
                                           [&](const Keyframe& k) { return k.frame >= leftDur; }), c.props.keyframes.end());
    c.duration = leftDur;
    c.fadeOut = 0;        // el fundido de salida pasa al clip derecho; el de entrada se queda a la izquierda
    right.fadeIn = 0;
    right.transitionIn = 0;   // la transición de entrada se queda en el clip izquierdo
    clampFades(c);
    clampFades(right);
    const std::size_t track = loc.track;
    tracks_[track].clips.push_back(right);
    sortTrack(track);
    const std::string problem = transitionProblem(track);
    if (!problem.empty()) {   // el corte caería dentro de una zona de transición: se deshace
        auto& clips = tracks_[track].clips;
        clips.erase(std::remove_if(clips.begin(), clips.end(), [&](const Clip& x) { return x.id == right.id; }), clips.end());
        for (Clip& x : clips) if (x.id == id) x = original;
        nextId_ = right.id;
        return Result::failure("No se puede dividir aquí: " + problem);
    }
    return Result::success(right.id);
}

Result Timeline::removeClip(ClipId id) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (tracks_[loc.track].locked) return Result::failure("La pista está bloqueada.");
    auto& clips = tracks_[loc.track].clips;
    clips.erase(clips.begin() + static_cast<std::ptrdiff_t>(loc.index));
    if (loc.index < clips.size()) clips[loc.index].transitionIn = 0;   // su transición era con el clip eliminado
    return Result::success(id);
}

std::string Timeline::validate() const {
    std::set<ClipId> ids;
    for (std::size_t t = 0; t < tracks_.size(); ++t) {
        const auto& clips = tracks_[t].clips;
        for (std::size_t i = 0; i < clips.size(); ++i) {
            const Clip& c = clips[i];
            if (c.duration <= 0) return "Clip con duración no positiva.";
            if (c.sourceIn < 0 || c.timelineStart < 0) return "Clip con posición negativa.";
            if (!(c.volume >= 0.0 && c.volume <= 4.0) || c.fadeIn < 0 || c.fadeOut < 0 || c.fadeIn + c.fadeOut > c.duration)
                return "Clip con volumen o fundidos fuera de rango.";
            if (!ids.insert(c.id).second) return "Identificador de clip duplicado.";
            {
                const std::string pp = validateProps(c.props, c.duration);
                if (!pp.empty()) return pp;
            }
            if (c.kind == ClipKind::Media && c.mediaPath.empty()) return "Clip sin ruta de medio.";
            if (c.kind == ClipKind::Text) {
                if (tracks_[t].kind != TrackKind::Video) return "Los textos solo van en pistas de video.";
                if (c.text.text.empty()) return "Texto vacío.";
                if (!(c.text.size >= 1 && c.text.size <= 50) || !(c.text.posX >= 0 && c.text.posX <= 100) ||
                    !(c.text.posY >= 0 && c.text.posY <= 100) || c.text.color > 0xFFFFFF || c.text.font.empty())
                    return "Texto con parámetros fuera de rango.";
            }
        }
        const std::string p = transitionProblem(t);   // solapes (solo válidos como transición) y transiciones
        if (!p.empty()) return p;
    }
    return {};
}

}  // namespace nexo
