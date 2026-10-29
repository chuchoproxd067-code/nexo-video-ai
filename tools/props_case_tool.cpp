// Herramienta de CI: genera el MLT XML de un caso de prueba de propiedades de clip.
//   props_case_tool <caso> <salida.mlt> <clipA> <clipB> <extra>
// `extra` es un video verde (caso chroma) o un PNG RGBA a tamaño completo (caso text).
// El workflow renderiza cada caso con melt y mide el resultado (ver tools/ci_props_check.sh).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "core/MltXml.h"

using namespace nexo;

int main(int argc, char** argv) {
    if (argc != 6) { std::fprintf(stderr, "uso: %s caso salida.mlt A B extra\n", argv[0]); return 2; }
    const std::string cs = argv[1], A = argv[3], B = argv[4], X = argv[5];
    Timeline tl({30, 1});
    ExportSettings st;
    st.width = 640; st.height = 360;
    st.mediaFrames[A] = 120;
    auto clip = [](const std::string& path, Frame in, Frame dur) {
        Clip c; c.mediaPath = path; c.sourceIn = in; c.duration = dur; c.timelineStart = 0; return c;
    };
    // Dos pistas: B abajo, A (con propiedades) encima.
    auto over = [&](const ClipProps& p, bool hidden = false) {
        const auto v0 = tl.addTrack("V1", TrackKind::Video);
        const auto v1 = tl.addTrack("V2", TrackKind::Video);
        tl.addClip(v0, clip(B, 0, 60));
        const Result r = tl.addClip(v1, clip(A, 0, 60));
        if (!r.ok || !tl.setClipProps(r.clip, p).ok) { std::fprintf(stderr, "setClipProps falló\n"); std::exit(1); }
        if (hidden) tl.setTrackFlags(v1, false, true, false);
    };
    auto solo = [&](const ClipProps& p, Frame in = 0, Frame dur = 60) {
        const auto v = tl.addTrack("V1", TrackKind::Video);
        const Result r = tl.addClip(v, clip(A, in, dur));
        if (!r.ok || !tl.setClipProps(r.clip, p).ok) { std::fprintf(stderr, "setClipProps falló\n"); std::exit(1); }
    };
    ClipProps p;
    if (cs == "speed2") { p.speed = 2; solo(p, 0, 60); }
    else if (cs == "speed05") { p.speed = 0.5; solo(p, 0, 60); }
    else if (cs == "reverse") { p.reverse = true; solo(p); }
    else if (cs == "freeze") { p.freeze = true; solo(p, 40, 60); }
    else if (cs == "scale50") { p.scale = 50; over(p); }
    else if (cs == "scale60") { p.scale = 60; over(p); }
    else if (cs == "pos50") { p.posX = 50; over(p); }
    else if (cs == "rot45") { p.scale = 60; p.rotation = 45; over(p); }
    else if (cs == "opacity50") { p.opacity = 0.5; over(p); }
    else if (cs == "kf") {
        p.keyframes = {{0, 0, 0, 100, 0, 1.0}, {59, 0, 0, 30, 0, 0.2}};
        over(p);
    }
    else if (cs == "crop") { p.cropLeft = 25; over(p); }
    else if (cs == "flip") { p.flipH = true; solo(p); }
    else if (cs == "bright") { p.brightness = 0.3; solo(p); }
    else if (cs == "contrast") { p.contrast = 1.8; solo(p); }
    else if (cs == "sat0") { p.saturation = 0; solo(p); }
    else if (cs == "temp") { p.temperature = 3000; solo(p); }
    else if (cs.rfind("eff_", 0) == 0) {
        EffectKind k;
        if (!effectFromName(cs.substr(4), k)) { std::fprintf(stderr, "efecto desconocido\n"); return 2; }
        p.effects = {{k, 1.0}};
        solo(p);
    }
    else if (cs == "chroma") {
        p.chroma.enabled = true; p.chroma.rgb = 0x00FF00; p.chroma.similarity = 0.3;
        const auto v0 = tl.addTrack("V1", TrackKind::Video);
        const auto v1 = tl.addTrack("V2", TrackKind::Video);
        tl.addClip(v0, clip(B, 0, 60));
        const Result r = tl.addClip(v1, clip(X, 0, 60));
        tl.setClipProps(r.clip, p);
    }
    else if (cs == "mute") { p.muted = true; solo(p); }
    else if (cs == "audiofx") { p.denoise = true; p.normalize = true; p.bass = 6; p.treble = -6; solo(p); }
    else if (cs == "pitch") { p.pitch = 7; solo(p); }
    else if (cs == "stabilize") { p.stabilize = true; solo(p); }
    else if (cs == "text") {
        const auto v0 = tl.addTrack("V1", TrackKind::Video);
        const auto v1 = tl.addTrack("V2", TrackKind::Video);
        tl.addClip(v0, clip(B, 0, 60));
        TextProps tp; tp.text = "NEXO";
        const Result r = tl.addTextClip(v1, tp, 0, 60);
        st.textImages[r.clip] = X;
    }
    else if (cs == "hidden") { over(p, true); }
    else if (cs == "trackmute") {
        const auto v = tl.addTrack("V1", TrackKind::Video);
        tl.addClip(v, clip(A, 0, 60));
        tl.setTrackFlags(v, true, false, false);
    }
    else { std::fprintf(stderr, "caso desconocido: %s\n", cs.c_str()); return 2; }

    const XmlResult r = buildMltXml(tl, st);
    if (!r.ok) { std::fprintf(stderr, "error: %s\n", r.error.c_str()); return 1; }
    std::ofstream f(argv[2], std::ios::binary);
    f << r.xml;
    return f ? 0 : 1;
}
