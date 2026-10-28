#include "core/MltXml.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <sstream>

namespace nexo {

namespace {

std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c;
        }
    }
    return out;
}

// MLT acepta '/' en rutas de Windows; evita problemas con las barras invertidas.
std::string normalizePath(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

// ---- utilidades de generación -------------------------------------------------------------------

std::string num(double v) {
    std::ostringstream o;
    o.precision(6);
    o << v;
    return o.str();
}

std::string prop(const std::string& k, const std::string& v) {
    return "      <property name=\"" + k + "\">" + esc(v) + "</property>\n";
}

// <filter> con propiedades. `inOut` = atributos in/out opcionales (posiciones del producer).
std::string filter(const std::string& service, std::initializer_list<std::pair<const char*, std::string>> props,
                   const std::string& inOut = {}) {
    std::string s = "    <filter" + inOut + ">\n" + prop("mlt_service", service);
    for (const auto& kv : props) s += prop(kv.first, kv.second);
    return s + "    </filter>\n";
}

std::string hex6(std::uint32_t rgb) {
    static const char* d = "0123456789abcdef";
    std::string s = "#";
    for (int sh = 20; sh >= 0; sh -= 4) s += d[(rgb >> sh) & 0xF];
    return s;
}

// Fotograma de entrada (en el tiempo del producer) del primer fotograma que muestra el clip.
// Con velocidad, `timewarp` expresa in/out en fotogramas de SALIDA; la inversa cuenta desde el
// final del medio. Los clips congelados y de texto empiezan en 0.
Frame entryIn(const Clip& c, const ExportSettings& st, bool& ok) {
    ok = true;
    if (c.kind == ClipKind::Text || c.props.freeze) return 0;
    const ClipProps& p = c.props;
    if (p.speed == 1.0 && !p.reverse) return c.sourceIn;
    if (!p.reverse) return static_cast<Frame>(std::llround(static_cast<double>(c.sourceIn) / p.speed));
    auto it = st.mediaFrames.find(c.mediaPath);
    if (it == st.mediaFrames.end() || it->second <= 0) { ok = false; return 0; }
    const Frame n = it->second, span = sourceSpan(p, c.duration);
    return std::max<Frame>(0, static_cast<Frame>(std::llround(static_cast<double>(n - (c.sourceIn + span)) / p.speed)));
}

// Animación "pos=valor;pos=valor". Las posiciones de los filtros de un producer son ABSOLUTAS
// del producer (verificado con melt real), no relativas a la entrada de la lista.
std::string fadeKeyframes(const Clip& c, Frame ein, double level) {
    const Frame s = ein, e = ein + c.duration - 1;
    std::ostringstream k;
    if (c.fadeIn > 0) k << s << "=0;" << s + c.fadeIn << "=" << level;
    else k << s << "=" << level;
    if (c.fadeOut > 0) k << ";" << std::max(e - c.fadeOut, s + c.fadeIn) << "=" << level << ";" << e << "=0";
    return k.str();
}

std::string effectFilter(const Effect& e) {
    const double a = std::clamp(e.amount, 0.0, 1.0);
    switch (e.kind) {
        case EffectKind::Blur: return filter("avfilter.gblur", {{"av.sigma", num(0.5 + a * 24)}});
        case EffectKind::Sharpen:
            return filter("avfilter.unsharp", {{"av.luma_amount", num(0.3 + a * 2.7)}, {"av.luma_msize_x", "7"}, {"av.luma_msize_y", "7"}});
        case EffectKind::Vignette: return filter("avfilter.vignette", {{"av.angle", num(1.2 - a * 0.9)}});
        case EffectKind::Mosaic: {
            const int px = 4 + static_cast<int>(std::lround(a * 60));
            return filter("avfilter.pixelize", {{"av.width", std::to_string(px)}, {"av.height", std::to_string(px)}});
        }
        case EffectKind::Grayscale: return filter("greyscale", {});
        case EffectKind::Sepia:
            return filter("avfilter.colorchannelmixer",
                          {{"av.rr", "0.393"}, {"av.rg", "0.769"}, {"av.rb", "0.189"}, {"av.gr", "0.349"}, {"av.gg", "0.686"},
                           {"av.gb", "0.168"}, {"av.br", "0.272"}, {"av.bg", "0.534"}, {"av.bb", "0.131"}});
        case EffectKind::Invert: return filter("avfilter.negate", {});
        case EffectKind::Mirror: return filter("mirror", {{"mirror", "horizontal"}});
    }
    return {};
}

// Transformación de video: affine como filtro del producer (verificado: posición, escala,
// rotación estática y animada, fotogramas clave absolutos del producer).
std::string transformFilters(const Clip& c, Frame ein, const ExportSettings& st) {
    const ClipProps& p = c.props;
    const double W = st.width, H = st.height;
    auto rect = [&](double px, double py, double sc, double op) {
        const double w = W * sc / 100.0, h = H * sc / 100.0;
        const double x = (W - w) / 2 + px / 100.0 * W, y = (H - h) / 2 + py / 100.0 * H;
        return num(std::round(x * 10) / 10) + "/" + num(std::round(y * 10) / 10) + ":" + num(std::round(w * 10) / 10) + "x" +
               num(std::round(h * 10) / 10) + ":" + num(op);
    };
    std::string rectAnim, rotAnim;
    bool needAffine = p.posX != 0 || p.posY != 0 || p.scale != 100 || p.rotation != 0 || !p.keyframes.empty();
    if (!needAffine) return {};
    if (p.keyframes.empty()) {
        rectAnim = rect(p.posX, p.posY, p.scale, 1.0);
        rotAnim = num(p.rotation);
    } else {
        for (const Keyframe& k : p.keyframes) {
            const std::string t = std::to_string(ein + k.frame);
            if (!rectAnim.empty()) { rectAnim += ";"; rotAnim += ";"; }
            rectAnim += t + "=" + rect(k.posX, k.posY, k.scale, 1.0);
            rotAnim += t + "=" + num(k.rotation);
        }
    }
    std::string s = "    <filter>\n" + prop("mlt_service", "affine") + prop("transition.rect", rectAnim);
    if (p.rotation != 0 || !p.keyframes.empty()) s += prop("transition.fix_rotate_z", rotAnim);
    return s + "    </filter>\n";
}

// Opacidad (estática o por fotogramas clave) como factor alfa del fotograma.
std::string opacityFilter(const Clip& c, Frame ein) {
    const ClipProps& p = c.props;
    if (p.keyframes.empty()) {
        if (p.opacity >= 1.0) return {};
        return filter("brightness", {{"alpha", num(p.opacity)}});
    }
    bool any = false;
    for (const Keyframe& k : p.keyframes) any = any || k.opacity < 1.0;
    if (!any) return {};
    std::string a;
    for (const Keyframe& k : p.keyframes) {
        if (!a.empty()) a += ";";
        a += std::to_string(ein + k.frame) + "=" + num(k.opacity);
    }
    return filter("brightness", {{"alpha", a}});
}

}  // namespace

const char* formatExtension(ExportFormat f) {
    switch (f) {
        case ExportFormat::Mp4: return "mp4";
        case ExportFormat::Mov: return "mov";
        case ExportFormat::WebM: return "webm";
        case ExportFormat::Gif: return "gif";
        case ExportFormat::Mp3: return "mp3";
        case ExportFormat::Wav: return "wav";
    }
    return "mp4";
}

const char* formatLabel(ExportFormat f) {
    switch (f) {
        case ExportFormat::Mp4: return "MP4 (H.264 + AAC)";
        case ExportFormat::Mov: return "MOV (H.264 + AAC)";
        case ExportFormat::WebM: return "WebM (VP9 + Opus)";
        case ExportFormat::Gif: return "GIF animado (sin audio)";
        case ExportFormat::Mp3: return "MP3 (solo audio)";
        case ExportFormat::Wav: return "WAV (solo audio)";
    }
    return "";
}

bool formatIsAudioOnly(ExportFormat f) { return f == ExportFormat::Mp3 || f == ExportFormat::Wav; }

std::vector<std::string> meltConsumerArgs(const ExportSettings& st, const std::string& out) {
    const int q = std::clamp(st.quality, 0, 2);
    std::vector<std::string> a{"-consumer", "avformat:" + out};
    auto add = [&](std::initializer_list<const char*> l) { for (const char* x : l) a.push_back(x); };
    switch (st.format) {
        case ExportFormat::Mp4:
        case ExportFormat::Mov:
            add({"vcodec=libx264", "acodec=aac"});
            a.push_back("crf=" + std::to_string(q == 0 ? 18 : q == 1 ? 23 : 28));
            add({"preset=veryfast"});
            break;
        case ExportFormat::WebM:
            add({"vcodec=libvpx-vp9", "acodec=libopus", "vb=0", "cpu-used=5"});
            a.push_back("crf=" + std::to_string(q == 0 ? 25 : q == 1 ? 31 : 38));
            break;
        case ExportFormat::Gif: add({"vcodec=gif", "an=1"}); break;
        case ExportFormat::Mp3: add({"vn=1", "acodec=libmp3lame", "ab=192k"}); break;
        case ExportFormat::Wav: add({"vn=1", "acodec=pcm_s16le"}); break;
    }
    return a;
}

XmlResult buildMltXml(const Timeline& tl, const ExportSettings& st) {
    XmlResult r;
    if (st.width <= 0 || st.height <= 0 || st.width % 2 != 0 || st.height % 2 != 0) {
        r.error = "La resolución debe ser positiva y par (los códecs habituales lo exigen).";
        return r;
    }
    const std::string problem = tl.validate();
    if (!problem.empty()) { r.error = "Línea de tiempo inválida: " + problem; return r; }
    const Frame total = tl.duration();
    if (total <= 0) { r.error = "No hay nada que exportar: la línea de tiempo está vacía."; return r; }
    const FrameRate fps = tl.frameRate();
    if (!fps.valid()) { r.error = "Tasa de fotogramas inválida."; return r; }

    const int g = std::gcd(st.width, st.height);
    std::ostringstream x;
    x << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    x << "<mlt LC_NUMERIC=\"C\" version=\"7.0.0\" title=\"NEXO VIDEO AI\" producer=\"main\">\n";
    x << "  <profile description=\"nexo\" width=\"" << st.width << "\" height=\"" << st.height
      << "\" progressive=\"1\" sample_aspect_num=\"1\" sample_aspect_den=\"1\""
      << " display_aspect_num=\"" << st.width / g << "\" display_aspect_den=\"" << st.height / g
      << "\" frame_rate_num=\"" << fps.num << "\" frame_rate_den=\"" << fps.den
      << "\" colorspace=\"709\"/>\n";

    // Fondo negro.
    x << "  <producer id=\"bg\" in=\"0\" out=\"" << total - 1 << "\">\n"
      << "    <property name=\"resource\">black</property>\n"
      << "    <property name=\"mlt_service\">color</property>\n"
      << "  </producer>\n";
    x << "  <playlist id=\"pl_bg\">\n    <entry producer=\"bg\" in=\"0\" out=\"" << total - 1 << "\"/>\n  </playlist>\n";

    // Fotograma de entrada de cada clip (depende de velocidad / inversa).
    std::map<ClipId, Frame> ein;
    for (std::size_t t = 0; t < tl.trackCount(); ++t)
        for (const Clip& c : tl.track(t).clips) {
            bool ok = true;
            const Frame e = entryIn(c, st, ok);
            if (!ok) { r.error = "Falta la duración del medio para invertir el clip: " + c.mediaPath; return r; }
            if (c.kind == ClipKind::Text && !st.textImages.count(c.id)) {
                r.error = "Falta la imagen del clip de texto " + std::to_string(c.id) + ".";
                return r;
            }
            ein[c.id] = e;
        }

    // Un producer por clip (más simple que compartirlos y evita efectos cruzados).
    for (std::size_t t = 0; t < tl.trackCount(); ++t)
        for (const Clip& c : tl.track(t).clips) {
            const bool isVideoTrack = tl.track(t).kind == TrackKind::Video;
            const ClipProps& p = c.props;
            const Frame e0 = ein[c.id];
            x << "  <producer id=\"clip_" << c.id << "\">\n";
            if (c.kind == ClipKind::Text) {
                x << prop("mlt_service", "qimage") << prop("resource", normalizePath(st.textImages.at(c.id)))
                  << prop("length", std::to_string(c.duration));
            } else if (p.freeze) {
                x << prop("mlt_service", "hold") << prop("resource", normalizePath(c.mediaPath))
                  << prop("frame", std::to_string(c.sourceIn));
            } else if (p.speed != 1.0 || p.reverse) {
                x << prop("mlt_service", "timewarp")
                  << prop("resource", num(p.reverse ? -p.speed : p.speed) + ":" + normalizePath(c.mediaPath));
            } else {
                x << prop("resource", normalizePath(c.mediaPath));
            }
            x << "";

            // ----- video (solo pistas de video) -----
            if (isVideoTrack) {
                if (p.cropLeft != 0 || p.cropRight != 0 || p.cropTop != 0 || p.cropBottom != 0)
                    x << filter("crop", {{"left", std::to_string(std::lround(st.width * p.cropLeft / 100.0))},
                                         {"right", std::to_string(std::lround(st.width * p.cropRight / 100.0))},
                                         {"top", std::to_string(std::lround(st.height * p.cropTop / 100.0))},
                                         {"bottom", std::to_string(std::lround(st.height * p.cropBottom / 100.0))}});
                if (p.flipH) x << filter("avfilter.hflip", {});
                if (p.flipV) x << filter("avfilter.vflip", {});
                if (p.brightness != 0 || p.contrast != 1 || p.saturation != 1)
                    x << filter("avfilter.eq", {{"av.brightness", num(p.brightness)}, {"av.contrast", num(p.contrast)},
                                                {"av.saturation", num(p.saturation)}});
                if (p.temperature != 6500) x << filter("avfilter.colortemperature", {{"av.temperature", num(p.temperature)}});
                for (const Effect& ef : p.effects) x << effectFilter(ef);
                if (p.chroma.enabled)
                    x << filter("chroma", {{"key", hex6(p.chroma.rgb)}, {"variance", num(p.chroma.similarity)}});
                if (p.stabilize) x << filter("vidstab", {});
                x << transformFilters(c, e0, st) << opacityFilter(c, e0);
            }

            // ----- audio -----
            if (c.kind == ClipKind::Media && !p.freeze) {
                if (p.denoise) x << filter("rnnoise", {});
                if (p.normalize) x << filter("dynamic_loudness", {});
                if (p.bass != 0) x << filter("avfilter.bass", {{"av.g", num(p.bass)}});
                if (p.treble != 0) x << filter("avfilter.treble", {{"av.g", num(p.treble)}});
                if (p.pitch != 0) x << filter("rbpitch", {{"pitchscale", num(std::pow(2.0, p.pitch / 12.0))}});
                // Verificado con melt real: `volume` NO acepta gain animado ("0=0;30=1" deja el audio mudo).
                // Se usa ganancia constante + rampas con in/out (gain -> end) para los fundidos.
                const Frame srcEnd = e0 + c.duration - 1;
                if (p.muted) {
                    x << filter("volume", {{"gain", "0"}});
                } else {
                    if (c.volume != 1.0) x << filter("volume", {{"gain", num(c.volume)}});
                    if (c.fadeIn > 0)
                        x << filter("volume", {{"gain", "0"}, {"end", "1"}},
                                    " in=\"" + std::to_string(e0) + "\" out=\"" + std::to_string(e0 + c.fadeIn - 1) + "\"");
                    if (c.fadeOut > 0)
                        x << filter("volume", {{"gain", "1"}, {"end", "0"}},
                                    " in=\"" + std::to_string(srcEnd - c.fadeOut + 1) + "\" out=\"" + std::to_string(srcEnd) + "\"");
                }
            }
            if ((c.fadeIn > 0 || c.fadeOut > 0) && isVideoTrack)
                x << filter("brightness", {{"level", fadeKeyframes(c, e0, 1.0)}});
            x << "  </producer>\n";
        }

    // Fundidos cruzados: un mini-tractor por transición con la cola del clip anterior y la cabeza del
    // clip nuevo; `luma` sin recurso es un disolver de video y `mix` es el fundido cruzado de audio.
    for (std::size_t t = 0; t < tl.trackCount(); ++t) {
        const auto& clips = tl.track(t).clips;
        for (std::size_t i = 1; i < clips.size(); ++i) {
            const Clip& a = clips[i - 1];
            const Clip& b = clips[i];
            const Frame n = b.transitionIn;
            if (n <= 0) continue;
            const Frame ea = ein[a.id], eb = ein[b.id];
            x << "  <playlist id=\"mixa_" << b.id << "\">\n    <entry producer=\"clip_" << a.id << "\" in=\""
              << ea + a.duration - n << "\" out=\"" << ea + a.duration - 1 << "\"/>\n  </playlist>\n";
            x << "  <playlist id=\"mixb_" << b.id << "\">\n    <entry producer=\"clip_" << b.id << "\" in=\""
              << eb << "\" out=\"" << eb + n - 1 << "\"/>\n  </playlist>\n";
            x << "  <tractor id=\"mix_" << b.id << "\" in=\"0\" out=\"" << n - 1 << "\">\n"
              << "    <multitrack>\n      <track producer=\"mixa_" << b.id << "\"/>\n      <track producer=\"mixb_" << b.id << "\"/>\n    </multitrack>\n";
            if (tl.track(t).kind == TrackKind::Video)
                x << "    <transition>\n      <property name=\"mlt_service\">luma</property>\n"
                  << "      <property name=\"in\">0</property>\n      <property name=\"out\">" << n - 1 << "</property>\n"
                  << "      <property name=\"a_track\">0</property>\n      <property name=\"b_track\">1</property>\n    </transition>\n";
            x << "    <transition>\n      <property name=\"mlt_service\">mix</property>\n"
              << "      <property name=\"in\">0</property>\n      <property name=\"out\">" << n - 1 << "</property>\n"
              << "      <property name=\"a_track\">0</property>\n      <property name=\"b_track\">1</property>\n    </transition>\n"
              << "  </tractor>\n";
        }
    }

    for (std::size_t t = 0; t < tl.trackCount(); ++t) {
        x << "  <playlist id=\"pl_" << t << "\">\n";
        const auto& clips = tl.track(t).clips;   // ordenados por posición
        Frame cursor = 0;
        for (std::size_t i = 0; i < clips.size(); ++i) {
            const Clip& c = clips[i];
            const Frame nextN = i + 1 < clips.size() ? clips[i + 1].transitionIn : 0;
            if (c.transitionIn > 0) {   // la zona de mezcla con el clip anterior
                x << "    <entry producer=\"mix_" << c.id << "\" in=\"0\" out=\"" << c.transitionIn - 1 << "\"/>\n";
                cursor += c.transitionIn;
            }
            const Frame startTl = c.timelineStart + c.transitionIn;
            const Frame len = c.duration - c.transitionIn - nextN;
            if (len > 0) {
                if (startTl > cursor) x << "    <blank length=\"" << startTl - cursor << "\"/>\n";
                const Frame e0 = ein[c.id];
                x << "    <entry producer=\"clip_" << c.id << "\" in=\"" << e0 + c.transitionIn << "\" out=\""
                  << e0 + c.transitionIn + len - 1 << "\"/>\n";
                cursor = startTl + len;
            }
        }
        x << "  </playlist>\n";
    }

    x << "  <tractor id=\"main\" in=\"0\" out=\"" << total - 1 << "\">\n";
    x << "    <multitrack>\n      <track producer=\"pl_bg\"/>\n";
    for (std::size_t t = 0; t < tl.trackCount(); ++t) {
        const Track& tr = tl.track(t);
        const bool hideVideo = tr.kind == TrackKind::Audio || tr.hidden;
        const bool hideAudio = tr.muted;
        x << "      <track producer=\"pl_" << t << "\"";
        if (hideVideo && hideAudio) x << " hide=\"both\"";
        else if (hideVideo) x << " hide=\"video\"";
        else if (hideAudio) x << " hide=\"audio\"";
        x << "/>\n";
    }
    x << "    </multitrack>\n";
    for (std::size_t t = 0; t < tl.trackCount(); ++t) {
        const std::size_t b = t + 1;
        x << "    <transition>\n      <property name=\"mlt_service\">mix</property>\n"
          << "      <property name=\"a_track\">0</property>\n      <property name=\"b_track\">" << b << "</property>\n"
          << "      <property name=\"always_active\">1</property>\n      <property name=\"sum\">1</property>\n"
          << "    </transition>\n";
        if (tl.track(t).kind == TrackKind::Video)
            x << "    <transition>\n      <property name=\"mlt_service\">composite</property>\n"
              << "      <property name=\"a_track\">0</property>\n      <property name=\"b_track\">" << b << "</property>\n"
              << "      <property name=\"always_active\">1</property>\n      <property name=\"fill\">1</property>\n"
              << "      <property name=\"distort\">1</property>\n"
              << "    </transition>\n";
    }
    x << "  </tractor>\n</mlt>\n";

    r.ok = true;
    r.xml = x.str();
    return r;
}

}  // namespace nexo
