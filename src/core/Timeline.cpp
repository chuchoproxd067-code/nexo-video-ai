#include "core/Timeline.h"

#include <algorithm>
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
    if (clip.mediaPath.empty()) return Result::failure("El clip no tiene ruta de medio.");
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
}  // namespace

Result Timeline::setTransition(ClipId id, Frame frames) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (frames < 0) return Result::failure("La transición no puede ser negativa.");
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

Result Timeline::setClipFx(ClipId id, double volume, Frame fadeIn, Frame fadeOut) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    Clip& c = tracks_[loc.track].clips[loc.index];
    if (!(volume >= 0.0 && volume <= 4.0)) return Result::failure("El volumen debe estar entre 0 y 4.");
    if (fadeIn < 0 || fadeOut < 0) return Result::failure("Los fundidos no pueden ser negativos.");
    if (fadeIn + fadeOut > c.duration) return Result::failure("Los fundidos suman más que la duración del clip.");
    c.volume = volume;
    c.fadeIn = fadeIn;
    c.fadeOut = fadeOut;
    return Result::success(id);
}

Result Timeline::trimStart(ClipId id, Frame delta) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    Clip& c = tracks_[loc.track].clips[loc.index];
    if (c.transitionIn > 0) return Result::failure("Quita la transición de entrada antes de recortar el inicio.");
    const Frame newStart = c.timelineStart + delta;
    const Frame newIn = c.sourceIn + delta;
    const Frame newDur = c.duration - delta;
    if (newDur <= 0) return Result::failure("El recorte dejaría el clip sin duración.");
    if (newIn < 0) return Result::failure("No hay material anterior en el medio fuente.");
    if (newStart < 0) return Result::failure("El recorte lo sacaría del inicio de la secuencia.");
    if (delta < 0 && overlaps(loc.track, newStart, c.timelineStart, id))
        return Result::failure("No hay espacio libre para alargar el clip.");
    const Clip before = c;
    c.timelineStart = newStart;
    c.sourceIn = newIn;
    c.duration = newDur;
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
    Clip& c = tracks_[loc.track].clips[loc.index];
    if (loc.index + 1 < tracks_[loc.track].clips.size() && tracks_[loc.track].clips[loc.index + 1].transitionIn > 0)
        return Result::failure("Quita la transición del clip siguiente antes de recortar el final.");
    const Frame newDur = c.duration + delta;
    if (newDur <= 0) return Result::failure("El recorte dejaría el clip sin duración.");
    if (delta > 0 && overlaps(loc.track, c.timelineEnd(), c.timelineEnd() + delta, id))
        return Result::failure("No hay espacio libre para alargar el clip.");
    const Clip before = c;
    c.duration = newDur;
    clampFades(c);
    const std::string problem = transitionProblem(loc.track);
    if (!problem.empty()) { c = before; return Result::failure(problem); }
    return Result::success(id);
}

Result Timeline::splitClip(ClipId id, Frame at) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    Clip& c = tracks_[loc.track].clips[loc.index];
    const Clip original = c;
    if (at <= c.timelineStart || at >= c.timelineEnd())
        return Result::failure("La posición de corte debe estar dentro del clip.");
    Clip right = c;
    const Frame leftDur = at - c.timelineStart;
    right.id = nextId_++;
    right.sourceIn = c.sourceIn + leftDur;
    right.timelineStart = at;
    right.duration = c.duration - leftDur;
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
        }
        const std::string p = transitionProblem(t);   // solapes (solo válidos como transición) y transiciones
        if (!p.empty()) return p;
    }
    return {};
}

}  // namespace nexo
