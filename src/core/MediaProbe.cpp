#include "core/MediaProbe.h"

#include <cmath>
#include <cstdlib>
#include <map>

namespace nexo {

namespace {

std::string unescape(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            struct E { const char* ent; char ch; };
            static const E table[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
            bool done = false;
            for (const E& e : table) {
                const std::string ent = e.ent;
                if (s.compare(i, ent.size(), ent) == 0) { out += e.ch; i += ent.size() - 1; done = true; break; }
            }
            if (done) continue;
        }
        out += s[i];
    }
    return out;
}

// Valor de un atributo dentro de una etiqueta ("" si no existe).
bool getAttr(const std::string& tag, const std::string& name, std::string& out) {
    const std::string key = " " + name + "=\"";
    const auto p = tag.find(key);
    if (p == std::string::npos) return false;
    const auto start = p + key.size();
    const auto end = tag.find('"', start);
    if (end == std::string::npos) return false;
    out = tag.substr(start, end - start);
    return true;
}

bool toInt(const std::string& s, long long& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtoll(s.c_str(), &end, 10);
    return end && *end == '\0';
}

}  // namespace

ProbeResult parseMeltProbeXml(const std::string& xml, FrameRate projectFps) {
    ProbeResult r;
    if (!projectFps.valid()) { r.error = "Tasa de fotogramas del proyecto inválida."; return r; }

    // Perfil que usó melt.
    const auto pp = xml.find("<profile");
    if (pp == std::string::npos) { r.error = "melt no devolvió información del medio (¿archivo no soportado o dañado?)."; return r; }
    const std::string profileTag = xml.substr(pp, xml.find('>', pp) - pp);
    std::string a, b;
    long long pnum = 0, pden = 0;
    if (!getAttr(profileTag, "frame_rate_num", a) || !getAttr(profileTag, "frame_rate_den", b) ||
        !toInt(a, pnum) || !toInt(b, pden) || pnum <= 0 || pden <= 0) {
        r.error = "No se pudo determinar la tasa de fotogramas del medio.";
        return r;
    }

    // Propiedades del primer producer.
    const auto prod = xml.find("<producer");
    const auto prodEnd = xml.find("</producer>", prod == std::string::npos ? 0 : prod);
    if (prod == std::string::npos || prodEnd == std::string::npos) {
        r.error = "melt no devolvió información del medio (¿archivo no soportado o dañado?).";
        return r;
    }
    const std::string block = xml.substr(prod, prodEnd - prod);
    std::map<std::string, std::string> props;
    for (std::size_t p = 0; (p = block.find("<property name=\"", p)) != std::string::npos;) {
        p += 16;
        const auto q = block.find('"', p);
        if (q == std::string::npos) break;
        const std::string name = block.substr(p, q - p);
        std::size_t v = q + 1;
        if (block.compare(v, 2, "/>") == 0) { props[name] = ""; p = v + 2; continue; }
        if (block.compare(v, 1, ">") != 0) { p = v; continue; }
        ++v;
        const auto e = block.find("</property>", v);
        if (e == std::string::npos) break;
        props[name] = unescape(block.substr(v, e - v));
        p = e;
    }

    auto intProp = [&](const std::string& name, long long def) {
        const auto it = props.find(name);
        long long v = 0;
        return (it != props.end() && toInt(it->second, v)) ? v : def;
    };

    const long long length = intProp("length", 0);
    const std::string service = props.count("mlt_service") ? props["mlt_service"] : "";
    const long long vi = intProp("video_index", -1);
    const long long ai = intProp("audio_index", -1);

    MediaInfo m;
    if (service == "qimage" || service == "pixbuf") {
        m.kind = MediaKind::Image;
        m.hasVideo = true;
        m.width = static_cast<int>(intProp("meta.media.width", 0));
        m.height = static_cast<int>(intProp("meta.media.height", 0));
        m.seconds = kDefaultImageSeconds;
    } else if (vi >= 0) {
        m.kind = MediaKind::Video;
        m.hasVideo = true;
        m.hasAudio = ai >= 0;
        const std::string base = "meta.media." + std::to_string(vi) + ".codec.";
        m.width = static_cast<int>(intProp(base + "width", intProp("meta.media.width", 0)));
        m.height = static_cast<int>(intProp(base + "height", intProp("meta.media.height", 0)));
    } else if (ai >= 0) {
        m.kind = MediaKind::Audio;
        m.hasAudio = true;
    } else {
        r.error = "El archivo no contiene audio ni video reconocibles.";
        return r;
    }

    if (m.kind != MediaKind::Image) {
        if (length <= 0) { r.error = "No se pudo leer la duración del medio."; return r; }
        m.seconds = static_cast<double>(length) * static_cast<double>(pden) / static_cast<double>(pnum);
    }
    m.frames = secondsToFrames(m.seconds, projectFps);
    if (m.frames < 1) m.frames = 1;

    r.ok = true;
    r.info = m;
    return r;
}

}  // namespace nexo
