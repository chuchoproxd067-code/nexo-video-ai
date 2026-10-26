#include "core/Project.h"

#include <algorithm>
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
        for (const Clip& c : timeline.track(t).clips) check(c.mediaPath);
    return out;
}

// ---------- guardado ----------

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
        Value clips = Value::array();
        for (const Clip& c : tr.clips) {
            Value jc = Value::object();
            jc.set("id", Value::number(static_cast<double>(c.id)));
            jc.set("media", Value::string(c.mediaPath));
            jc.set("sourceIn", Value::number(static_cast<double>(c.sourceIn)));
            jc.set("start", Value::number(static_cast<double>(c.timelineStart)));
            jc.set("duration", Value::number(static_cast<double>(c.duration)));
            clips.push(std::move(jc));
        }
        jt.set("clips", std::move(clips));
        tracks.push(std::move(jt));
    }
    root.set("tracks", std::move(tracks));
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
        const json::Value* clips = jt.get("clips");
        if (!clips || !clips->isArray()) { error = "La pista '" + name + "' no tiene lista de clips."; return false; }
        for (const auto& jc : clips->items()) {
            if (!jc.isObject()) { error = "Clip inválido en la pista '" + name + "'."; return false; }
            Clip c;
            std::int64_t id = 0, in = 0, start = 0, dur = 0;
            if (!needInt(jc, "id", id, err) || !needString(jc, "media", c.mediaPath, err) ||
                !needInt(jc, "sourceIn", in, err) || !needInt(jc, "start", start, err) ||
                !needInt(jc, "duration", dur, err)) {
                error = "Clip en '" + name + "': " + err;
                return false;
            }
            if (id <= 0 || id > 0xFFFFFFFFLL) { error = "Identificador de clip fuera de rango."; return false; }
            c.id = static_cast<ClipId>(id);
            c.sourceIn = in;
            c.timelineStart = start;
            c.duration = dur;
            const Result r = tl.insertClip(ti, c);
            if (!r.ok) { error = "Clip " + std::to_string(id) + " en '" + name + "': " + r.error; return false; }
        }
    }
    const std::string problem = tl.validate();
    if (!problem.empty()) { error = "Proyecto inconsistente: " + problem; return false; }

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
