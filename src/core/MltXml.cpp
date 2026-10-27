#include "core/MltXml.h"

#include <algorithm>
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

// Animación de MLT "pos=valor;pos=valor" en fotogramas del medio fuente (las posiciones de los
// filtros de un producer son las del medio, no las de la secuencia).
std::string fadeKeyframes(const Clip& c, double level) {
    const Frame s = c.sourceIn, e = c.sourceIn + c.duration - 1;
    std::ostringstream k;
    if (c.fadeIn > 0) k << s << "=0;" << s + c.fadeIn << "=" << level;
    else k << s << "=" << level;
    if (c.fadeOut > 0) k << ";" << std::max(e - c.fadeOut, s + c.fadeIn) << "=" << level << ";" << e << "=0";
    return k.str();
}

}  // namespace

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

    // Un producer por clip (más simple que compartirlos y evita efectos cruzados).
    for (std::size_t t = 0; t < tl.trackCount(); ++t)
        for (const Clip& c : tl.track(t).clips)
        {
            x << "  <producer id=\"clip_" << c.id << "\">\n"
              << "    <property name=\"resource\">" << esc(normalizePath(c.mediaPath)) << "</property>\n";
            const bool fades = c.fadeIn > 0 || c.fadeOut > 0;
            // Verificado con melt real: `volume` NO acepta gain animado ("0=0;30=1" deja el audio mudo).
            // Se usa ganancia constante + rampas con in/out (gain -> end) para los fundidos.
            const Frame srcEnd = c.sourceIn + c.duration - 1;
            if (c.volume != 1.0)
                x << "    <filter>\n      <property name=\"mlt_service\">volume</property>\n"
                  << "      <property name=\"gain\">" << c.volume << "</property>\n    </filter>\n";
            if (c.fadeIn > 0)
                x << "    <filter in=\"" << c.sourceIn << "\" out=\"" << c.sourceIn + c.fadeIn - 1 << "\">\n"
                  << "      <property name=\"mlt_service\">volume</property>\n"
                  << "      <property name=\"gain\">0</property>\n      <property name=\"end\">1</property>\n    </filter>\n";
            if (c.fadeOut > 0)
                x << "    <filter in=\"" << srcEnd - c.fadeOut + 1 << "\" out=\"" << srcEnd << "\">\n"
                  << "      <property name=\"mlt_service\">volume</property>\n"
                  << "      <property name=\"gain\">1</property>\n      <property name=\"end\">0</property>\n    </filter>\n";
            if (fades && tl.track(t).kind == TrackKind::Video)
                x << "    <filter>\n      <property name=\"mlt_service\">brightness</property>\n"
                  << "      <property name=\"level\">" << fadeKeyframes(c, 1.0) << "</property>\n    </filter>\n";
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
            x << "  <playlist id=\"mixa_" << b.id << "\">\n    <entry producer=\"clip_" << a.id << "\" in=\""
              << a.sourceIn + a.duration - n << "\" out=\"" << a.sourceIn + a.duration - 1 << "\"/>\n  </playlist>\n";
            x << "  <playlist id=\"mixb_" << b.id << "\">\n    <entry producer=\"clip_" << b.id << "\" in=\""
              << b.sourceIn << "\" out=\"" << b.sourceIn + n - 1 << "\"/>\n  </playlist>\n";
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
                x << "    <entry producer=\"clip_" << c.id << "\" in=\"" << c.sourceIn + c.transitionIn << "\" out=\""
                  << c.sourceIn + c.transitionIn + len - 1 << "\"/>\n";
                cursor = startTl + len;
            }
        }
        x << "  </playlist>\n";
    }

    x << "  <tractor id=\"main\" in=\"0\" out=\"" << total - 1 << "\">\n";
    x << "    <multitrack>\n      <track producer=\"pl_bg\"/>\n";
    for (std::size_t t = 0; t < tl.trackCount(); ++t) {
        x << "      <track producer=\"pl_" << t << "\"";
        if (tl.track(t).kind == TrackKind::Audio) x << " hide=\"video\"";
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
