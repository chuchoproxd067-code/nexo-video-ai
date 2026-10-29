#include "core/Project.h"
#include "core/TransitionStyles.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <system_error>

#include "core/Json.h"

namespace nexo {

namespace fs = std::filesystem;

bool Project::addMedia(const std::string& path) {
    if (path.empty()) return false;
    if (std::find(media.begin(), media.end(), path) == media.end()) media.push_back(path);
    return true;
}

static bool existsOnDisk(const std::string& path) {
    std::error_code ec;
    return fs::exists(fs::u8path(path), ec) && !ec;
}

std::vector<std::string> Project::missingMedia() const {
    std::vector<std::string> out;
    std::set<std::string> seen;
    auto check = [&](const std::string& path) {
        if (!seen.insert(path).second) return;
        if (!existsOnDisk(path)) out.push_back(path);
    };
    for (const auto& m : media) check(m);
    for (std::size_t t = 0; t < timeline.trackCount(); ++t)
        for (const Clip& c : timeline.track(t).clips) if (c.kind == ClipKind::Media) check(c.mediaPath);
    return out;
}

// ---------- guardado ----------

namespace {

using json::Value;

Value propsToJson(const ClipProps& p) {
    Value o = Value::object();
    auto num = [&](const char* k, double v, double def) { if (v != def) o.set(k, Value::number(v)); };
    auto flag = [&](const char* k, bool v) { if (v) o.set(k, Value::boolean(true)); };
    num("speed", p.speed, 1); flag("reverse", p.reverse); flag("freeze", p.freeze);
    num("posX", p.posX, 0); num("posY", p.posY, 0); num("scale", p.scale, 100); num("rotation", p.rotation, 0);
    num("opacity", p.opacity, 1);
    num("cropLeft", p.cropLeft, 0); num("cropRight", p.cropRight, 0); num("cropTop", p.cropTop, 0); num("cropBottom", p.cropBottom, 0);
    flag("flipH", p.flipH); flag("flipV", p.flipV);
    num("brightness", p.brightness, 0); num("contrast", p.contrast, 1); num("saturation", p.saturation, 1);
    num("temperature", p.temperature, 6500);
    if (!p.effects.empty()) {
        Value arr = Value::array();
        for (const Effect& e : p.effects) {
            Value je = Value::object();
            je.set("kind", Value::string(effectName(e.kind)));
            je.set("amount", Value::number(e.amount));
            arr.push(std::move(je));
        }
        o.set("effects", std::move(arr));
    }
    if (p.chroma.enabled) {
        Value c = Value::object();
        c.set("rgb", Value::number(static_cast<double>(p.chroma.rgb)));
        c.set("similarity", Value::number(p.chroma.similarity));
        c.set("blend", Value::number(p.chroma.blend));
        o.set("chroma", std::move(c));
    }
    if (!p.keyframes.empty()) {
        Value arr = Value::array();
        for (const Keyframe& k : p.keyframes) {
            Value jk = Value::object();
            jk.set("frame", Value::number(static_cast<double>(k.frame)));
            jk.set("posX", Value::number(k.posX)); jk.set("posY", Value::number(k.posY));
            jk.set("scale", Value::number(k.scale)); jk.set("rotation", Value::number(k.rotation));
            jk.set("opacity", Value::number(k.opacity));
            arr.push(std::move(jk));
        }
        o.set("keyframes", std::move(arr));
    }
    flag("stabilize", p.stabilize); flag("muted", p.muted); flag("denoise", p.denoise); flag("normalize", p.normalize);
    num("pitch", p.pitch, 0); num("bass", p.bass, 0); num("treble", p.treble, 0);
    return o;
}

bool optNum(const Value& o, const char* key, double& out, std::string& err) {
    const Value* v = o.get(key);
    if (!v) return true;
    if (!v->isNumber()) { err = std::string("No es un número: ") + key; return false; }
    out = v->asNumber();
    return true;
}
bool optBool(const Value& o, const char* key, bool& out, std::string& err) {
    const Value* v = o.get(key);
    if (!v) return true;
    if (v->type() != Value::Type::Bool) { err = std::string("No es verdadero/falso: ") + key; return false; }
    out = v->asBool();
    return true;
}
bool reqNum(const Value& o, const char* key, double& out, std::string& err) {
    if (!o.get(key)) { err = std::string("Falta: ") + key; return false; }
    return optNum(o, key, out, err);
}

bool propsFromJson(const Value& o, ClipProps& p, std::string& err) {
    if (!o.isObject()) { err = "Propiedades inválidas."; return false; }
    bool ok = optNum(o, "speed", p.speed, err) && optBool(o, "reverse", p.reverse, err) && optBool(o, "freeze", p.freeze, err) &&
              optNum(o, "posX", p.posX, err) && optNum(o, "posY", p.posY, err) && optNum(o, "scale", p.scale, err) &&
              optNum(o, "rotation", p.rotation, err) && optNum(o, "opacity", p.opacity, err) &&
              optNum(o, "cropLeft", p.cropLeft, err) && optNum(o, "cropRight", p.cropRight, err) &&
              optNum(o, "cropTop", p.cropTop, err) && optNum(o, "cropBottom", p.cropBottom, err) &&
              optBool(o, "flipH", p.flipH, err) && optBool(o, "flipV", p.flipV, err) &&
              optNum(o, "brightness", p.brightness, err) && optNum(o, "contrast", p.contrast, err) &&
              optNum(o, "saturation", p.saturation, err) && optNum(o, "temperature", p.temperature, err) &&
              optBool(o, "stabilize", p.stabilize, err) && optBool(o, "muted", p.muted, err) &&
              optBool(o, "denoise", p.denoise, err) && optBool(o, "normalize", p.normalize, err) &&
              optNum(o, "pitch", p.pitch, err) && optNum(o, "bass", p.bass, err) && optNum(o, "treble", p.treble, err);
    if (!ok) return false;
    if (const Value* ef = o.get("effects")) {
        if (!ef->isArray()) { err = "Los efectos deben ser una lista."; return false; }
        for (const Value& je : ef->items()) {
            Effect e;
            const Value* k = je.isObject() ? je.get("kind") : nullptr;
            if (!k || !k->isString() || !effectFromName(k->asString(), e.kind)) { err = "Efecto desconocido."; return false; }
            if (!optNum(je, "amount", e.amount, err)) return false;
            p.effects.push_back(e);
        }
    }
    if (const Value* c = o.get("chroma")) {
        if (!c->isObject()) { err = "Croma inválido."; return false; }
        double rgb = 0;
        if (!reqNum(*c, "rgb", rgb, err) || !optNum(*c, "similarity", p.chroma.similarity, err) || !optNum(*c, "blend", p.chroma.blend, err)) return false;
        if (rgb < 0 || rgb > 0xFFFFFF || std::floor(rgb) != rgb) { err = "Color de croma inválido."; return false; }
        p.chroma.enabled = true;
        p.chroma.rgb = static_cast<std::uint32_t>(rgb);
    }
    if (const Value* ks = o.get("keyframes")) {
        if (!ks->isArray()) { err = "Los fotogramas clave deben ser una lista."; return false; }
        for (const Value& jk : ks->items()) {
            if (!jk.isObject()) { err = "Fotograma clave inválido."; return false; }
            Keyframe k;
            double fr = 0;
            if (!reqNum(jk, "frame", fr, err) || !optNum(jk, "posX", k.posX, err) || !optNum(jk, "posY", k.posY, err) ||
                !optNum(jk, "scale", k.scale, err) || !optNum(jk, "rotation", k.rotation, err) || !optNum(jk, "opacity", k.opacity, err)) return false;
            if (fr < 0 || fr > 9e15 || std::floor(fr) != fr) { err = "Fotograma clave fuera de rango."; return false; }
            k.frame = static_cast<Frame>(fr);
            p.keyframes.push_back(k);
        }
    }
    return true;
}

Value textToJson(const TextProps& t) {
    Value o = Value::object();
    o.set("text", Value::string(t.text));
    o.set("font", Value::string(t.font));
    o.set("size", Value::number(t.size));
    o.set("color", Value::number(static_cast<double>(t.color)));
    o.set("posX", Value::number(t.posX));
    o.set("posY", Value::number(t.posY));
    if (t.bold) o.set("bold", Value::boolean(true));
    return o;
}

bool textFromJson(const Value& o, TextProps& t, std::string& err) {
    if (!o.isObject()) { err = "Texto inválido."; return false; }
    const Value* tx = o.get("text");
    if (!tx || !tx->isString()) { err = "Falta el texto."; return false; }
    t.text = tx->asString();
    if (const Value* f = o.get("font")) { if (!f->isString()) { err = "Fuente inválida."; return false; } t.font = f->asString(); }
    double color = t.color;
    if (!optNum(o, "size", t.size, err) || !optNum(o, "color", color, err) || !optNum(o, "posX", t.posX, err) ||
        !optNum(o, "posY", t.posY, err) || !optBool(o, "bold", t.bold, err)) return false;
    if (color < 0 || color > 0xFFFFFF || std::floor(color) != color) { err = "Color de texto inválido."; return false; }
    t.color = static_cast<std::uint32_t>(color);
    return true;
}

}  // namespace

std::string toJson(const Project& p) {
    using json::Value;
    Value root = Value::object();
    root.set("schema", Value::number(kProjectSchemaVersion));
    root.set("app", Value::string("NEXO VIDEO AI"));
    root.set("name", Value::string(p.name));

    Value fps = Value::object();
    fps.set("num", Value::number(p.timeline.frameRate().num));
    fps.set("den", Value::number(p.timeline.frameRate().den));
    root.set("fps", std::move(fps));

    Value media = Value::array();
    for (const auto& m : p.media) media.push(Value::string(m));
    root.set("media", std::move(media));

    Value tracks = Value::array();
    for (std::size_t t = 0; t < p.timeline.trackCount(); ++t) {
        const Track& tr = p.timeline.track(t);
        Value jt = Value::object();
        jt.set("name", Value::string(tr.name));
        jt.set("kind", Value::string(tr.kind == TrackKind::Video ? "video" : "audio"));
        if (tr.muted) jt.set("muted", Value::boolean(true));
        if (tr.hidden) jt.set("hidden", Value::boolean(true));
        if (tr.locked) jt.set("locked", Value::boolean(true));
        Value clips = Value::array();
        for (const Clip& c : tr.clips) {
            Value jc = Value::object();
            jc.set("id", Value::number(static_cast<double>(c.id)));
            if (c.kind == ClipKind::Text) {
                jc.set("kind", Value::string("text"));
                jc.set("text", textToJson(c.text));
            } else {
                jc.set("media", Value::string(c.mediaPath));
            }
            jc.set("sourceIn", Value::number(static_cast<double>(c.sourceIn)));
            jc.set("start", Value::number(static_cast<double>(c.timelineStart)));
            jc.set("duration", Value::number(static_cast<double>(c.duration)));
            if (c.volume != 1.0) jc.set("volume", Value::number(c.volume));
            if (c.fadeIn != 0) jc.set("fadeIn", Value::number(static_cast<double>(c.fadeIn)));
            if (c.transitionIn != 0) jc.set("transitionIn", Value::number(static_cast<double>(c.transitionIn)));
            if (!c.transitionStyle.empty()) jc.set("transitionStyle", Value::string(c.transitionStyle));
            if (c.fadeOut != 0) jc.set("fadeOut", Value::number(static_cast<double>(c.fadeOut)));
            if (!c.props.isDefault()) jc.set("props", propsToJson(c.props));
            clips.push(std::move(jc));
        }
        jt.set("clips", std::move(clips));
        tracks.push(std::move(jt));
    }
    root.set("tracks", std::move(tracks));
    if (!p.timeline.markers().empty()) {
        Value ms = Value::array();
        for (const Marker& m : p.timeline.markers()) {
            Value jm = Value::object();
            jm.set("frame", Value::number(static_cast<double>(m.frame)));
            jm.set("name", Value::string(m.name));
            ms.push(std::move(jm));
        }
        root.set("markers", std::move(ms));
    }
    return root.dump(2) + "\n";
}

// ---------- carga ----------

namespace {

// Los helpers devuelven false y dejan el motivo en `err`.
bool needObject(const json::Value* v, const char* what, std::string& err) {
    if (!v || !v->isObject()) { err = std::string("Falta o es inválido: ") + what; return false; }
    return true;
}
bool needString(const json::Value& obj, const char* key, std::string& out, std::string& err) {
    const json::Value* v = obj.get(key);
    if (!v || !v->isString()) { err = std::string("Falta o no es texto: ") + key; return false; }
    out = v->asString();
    return true;
}
// Entero no negativo y representable con exactitud.
bool needInt(const json::Value& obj, const char* key, std::int64_t& out, std::string& err) {
    const json::Value* v = obj.get(key);
    if (!v || !v->isNumber()) { err = std::string("Falta o no es número: ") + key; return false; }
    const double d = v->asNumber();
    if (std::floor(d) != d || d < 0 || d > 9e15) {
        err = std::string("Debe ser un entero no negativo: ") + key;
        return false;
    }
    out = static_cast<std::int64_t>(d);
    return true;
}

}  // namespace

bool fromJson(const std::string& text, Project& out, std::string& error) {
    json::Value root;
    if (!json::parse(text, root, error)) return false;
    if (!root.isObject()) { error = "El archivo no es un proyecto de NEXO."; return false; }

    std::string err;
    std::int64_t schema = 0;
    if (!needInt(root, "schema", schema, err)) { error = "No es un proyecto de NEXO (" + err + ")."; return false; }
    if (schema > kProjectSchemaVersion) {
        error = "El proyecto se creó con una versión más nueva de NEXO (esquema " + std::to_string(schema) +
                "; esta versión entiende hasta el " + std::to_string(kProjectSchemaVersion) + ").";
        return false;
    }
    if (schema < 1) { error = "Esquema de proyecto no válido."; return false; }
    // Aquí irán las migraciones (esquema N -> N+1) cuando exista una segunda versión.

    Project p;
    if (!needString(root, "name", p.name, err)) { error = err; return false; }

    const json::Value* fpsV = root.get("fps");
    if (!needObject(fpsV, "fps", err)) { error = err; return false; }
    std::int64_t num = 0, den = 0;
    if (!needInt(*fpsV, "num", num, err) || !needInt(*fpsV, "den", den, err)) { error = "fps: " + err; return false; }
    if (num <= 0 || den <= 0 || num > 1000000 || den > 1000000) { error = "fps fuera de rango."; return false; }
    Timeline tl(FrameRate{static_cast<int>(num), static_cast<int>(den)});
    std::vector<std::array<bool, 3>> trackFlags;

    const json::Value* media = root.get("media");
    if (!media || !media->isArray()) { error = "Falta la lista de medios."; return false; }
    for (const auto& m : media->items()) {
        if (!m.isString() || !p.addMedia(m.asString())) { error = "Ruta de medio inválida en la biblioteca."; return false; }
    }

    const json::Value* tracks = root.get("tracks");
    if (!tracks || !tracks->isArray()) { error = "Faltan las pistas."; return false; }
    for (const auto& jt : tracks->items()) {
        if (!jt.isObject()) { error = "Pista inválida."; return false; }
        std::string name, kind;
        if (!needString(jt, "name", name, err) || !needString(jt, "kind", kind, err)) { error = "Pista: " + err; return false; }
        if (kind != "video" && kind != "audio") { error = "Tipo de pista desconocido: " + kind; return false; }
        const std::size_t ti = tl.addTrack(name, kind == "video" ? TrackKind::Video : TrackKind::Audio);
        bool tMuted = false, tHidden = false, tLocked = false;
        if (!optBool(jt, "muted", tMuted, err) || !optBool(jt, "hidden", tHidden, err) || !optBool(jt, "locked", tLocked, err)) {
            error = "Pista '" + name + "': " + err;
            return false;
        }
        trackFlags.push_back({tMuted, tHidden, tLocked});
        const json::Value* clips = jt.get("clips");
        if (!clips || !clips->isArray()) { error = "La pista '" + name + "' no tiene lista de clips."; return false; }
        for (const auto& jc : clips->items()) {
            if (!jc.isObject()) { error = "Clip inválido en la pista '" + name + "'."; return false; }
            Clip c;
            std::int64_t id = 0, in = 0, start = 0, dur = 0;
            const json::Value* kindV = jc.get("kind");
            const bool isText = kindV && kindV->isString() && kindV->asString() == "text";
            if (kindV && !(kindV->isString() && (kindV->asString() == "text" || kindV->asString() == "media"))) {
                error = "Clip en '" + name + "': tipo de clip desconocido.";
                return false;
            }
            if (!needInt(jc, "id", id, err) || (!isText && !needString(jc, "media", c.mediaPath, err)) ||
                !needInt(jc, "sourceIn", in, err) || !needInt(jc, "start", start, err) ||
                !needInt(jc, "duration", dur, err)) {
                error = "Clip en '" + name + "': " + err;
                return false;
            }
            if (isText) {
                const json::Value* tv = jc.get("text");
                if (!tv || !textFromJson(*tv, c.text, err)) { error = "Clip de texto en '" + name + "': " + (tv ? err : "falta el texto."); return false; }
                c.kind = ClipKind::Text;
            }
            if (const json::Value* pv = jc.get("props")) {
                if (!propsFromJson(*pv, c.props, err)) { error = "Clip en '" + name + "': " + err; return false; }
            }
            if (id <= 0 || id > 0xFFFFFFFFLL) { error = "Identificador de clip fuera de rango."; return false; }
            c.id = static_cast<ClipId>(id);
            c.sourceIn = in;
            c.timelineStart = start;
            c.duration = dur;
            // Campos opcionales (proyectos anteriores no los tienen).
            if (const json::Value* v = jc.get("volume")) {
                if (!v->isNumber()) { error = "Clip " + std::to_string(id) + ": volumen inválido."; return false; }
                c.volume = v->asNumber();
            }
            for (const char* key : {"fadeIn", "fadeOut", "transitionIn"}) {
                std::int64_t fv = 0;
                if (jc.get(key) && !needInt(jc, key, fv, err)) { error = "Clip " + std::to_string(id) + ": " + err; return false; }
                (key[4] == 'I' ? c.fadeIn : key[4] == 'O' ? c.fadeOut : c.transitionIn) = fv;
            }
            if (const json::Value* v = jc.get("transitionStyle")) {
                if (!v->isString() || !validTransitionStyle(v->asString())) { error = "Clip " + std::to_string(id) + ": estilo de transición inválido."; return false; }
                c.transitionStyle = v->asString();
            }
            const Result r = tl.insertClip(ti, c);
            if (!r.ok) { error = "Clip " + std::to_string(id) + " en '" + name + "': " + r.error; return false; }
        }
    }
    const std::string problem = tl.validate();
    if (!problem.empty()) { error = "Proyecto inconsistente: " + problem; return false; }
    for (std::size_t ti = 0; ti < trackFlags.size(); ++ti) tl.setTrackFlags(ti, trackFlags[ti][0], trackFlags[ti][1], trackFlags[ti][2]);
    if (const json::Value* ms = root.get("markers")) {
        if (!ms->isArray()) { error = "Los marcadores deben ser una lista."; return false; }
        for (const auto& jm : ms->items()) {
            std::int64_t fr = 0;
            std::string mname;
            if (!jm.isObject() || !needInt(jm, "frame", fr, err) || !needString(jm, "name", mname, err)) { error = "Marcador inválido."; return false; }
            const Result mr = tl.addMarker(fr, mname);
            if (!mr.ok) { error = "Marcador: " + mr.error; return false; }
        }
    }

    p.timeline = std::move(tl);
    out = std::move(p);
    return true;
}

// ---------- archivos ----------

bool saveProject(const Project& p, const std::string& path, std::string& error) {
    if (path.empty()) { error = "Ruta de guardado vacía."; return false; }
    const fs::path target = fs::u8path(path);
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) { error = "No se puede escribir en " + tmp.u8string(); return false; }
        const std::string data = toJson(p);
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        f.flush();
        if (!f) {
            error = "Error al escribir (¿disco lleno?).";
            std::error_code ignore;
            fs::remove(tmp, ignore);
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, target, ec);
    if (ec) {
        error = "No se pudo reemplazar el proyecto: " + ec.message();
        std::error_code ignore;
        fs::remove(tmp, ignore);
        return false;
    }
    return true;
}

bool loadProject(const std::string& path, Project& out, std::string& error) {
    std::ifstream f(fs::u8path(path), std::ios::binary);
    if (!f) { error = "No se puede abrir el archivo: " + path; return false; }
    std::ostringstream ss;
    ss << f.rdbuf();
    return fromJson(ss.str(), out, error);
}

}  // namespace nexo
