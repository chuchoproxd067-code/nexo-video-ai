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
    if (overlaps(track, clip.timelineStart, clip.timelineEnd(), 0))
        return Result::failure("El clip se solapa con otro en la pista.");
    if (clip.id >= nextId_) nextId_ = clip.id + 1;
    tracks_[track].clips.push_back(clip);
    sortTrack(track);
    return Result::success(clip.id);
}

Result Timeline::moveClip(ClipId id, std::size_t toTrack, Frame newStart) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    if (toTrack >= tracks_.size()) return Result::failure("La pista de destino no existe.");
    if (newStart < 0) return Result::failure("La posición no puede ser negativa.");
    if (tracks_[toTrack].kind != tracks_[loc.track].kind)
        return Result::failure("No se puede mover un clip entre pistas de distinto tipo.");
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
    const Frame newStart = c.timelineStart + delta;
    const Frame newIn = c.sourceIn + delta;
    const Frame newDur = c.duration - delta;
    if (newDur <= 0) return Result::failure("El recorte dejaría el clip sin duración.");
    if (newIn < 0) return Result::failure("No hay material anterior en el medio fuente.");
    if (newStart < 0) return Result::failure("El recorte lo sacaría del inicio de la secuencia.");
    if (delta < 0 && overlaps(loc.track, newStart, c.timelineStart, id))
        return Result::failure("No hay espacio libre para alargar el clip.");
    c.timelineStart = newStart;
    c.sourceIn = newIn;
    c.duration = newDur;
    clampFades(c);
    sortTrack(loc.track);
    return Result::success(id);
}

Result Timeline::trimEnd(ClipId id, Frame delta) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    Clip& c = tracks_[loc.track].clips[loc.index];
    const Frame newDur = c.duration + delta;
    if (newDur <= 0) return Result::failure("El recorte dejaría el clip sin duración.");
    if (delta > 0 && overlaps(loc.track, c.timelineEnd(), c.timelineEnd() + delta, id))
        return Result::failure("No hay espacio libre para alargar el clip.");
    c.duration = newDur;
    clampFades(c);
    return Result::success(id);
}

Result Timeline::splitClip(ClipId id, Frame at) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    Clip& c = tracks_[loc.track].clips[loc.index];
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
    clampFades(c);
    clampFades(right);
    const std::size_t track = loc.track;
    tracks_[track].clips.push_back(right);
    sortTrack(track);
    return Result::success(right.id);
}

Result Timeline::removeClip(ClipId id) {
    Location loc;
    if (!find(id, loc)) return Result::failure("El clip no existe.");
    auto& clips = tracks_[loc.track].clips;
    clips.erase(clips.begin() + static_cast<std::ptrdiff_t>(loc.index));
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
            if (i > 0 && clips[i - 1].timelineEnd() > c.timelineStart)
                return "Clips solapados en la pista " + std::to_string(t) + ".";
        }
    }
    return {};
}

}  // namespace nexo
