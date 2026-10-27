// Herramienta de CI: construye una línea de tiempo de ejemplo con dos clips de
// video y un clip de audio, y escribe su MLT XML. El workflow la renderiza con
// `melt` y comprueba el resultado con ffprobe.
//   mlt_xml_tool <salida.mlt> <clipA> <clipB>
// Se asume que ambos clips tienen al menos 120 fotogramas a 30 fps (4 s).
#include <cstdio>
#include <fstream>
#include <string>

#include "core/MltXml.h"

int main(int argc, char** argv) {
    const bool fx = argc == 5 && std::string(argv[4]) == "--fx";
    if (argc != 4 && !fx) {
        std::fprintf(stderr, "uso: %s <salida.mlt> <clipA> <clipB> [--fx]\n", argv[0]);
        return 2;
    }
    nexo::Timeline tl({30, 1});
    if (fx) {   // un solo clip de 90 fotogramas (desde el 30 del medio) con volumen 0.5 y fundidos de 30 fotogramas
        const auto v = tl.addTrack("V1", nexo::TrackKind::Video);
        nexo::Clip c;
        c.mediaPath = argv[2]; c.sourceIn = 30; c.duration = 90; c.timelineStart = 0;
        c.volume = 0.5; c.fadeIn = 30; c.fadeOut = 30;
        tl.addClip(v, c);
        nexo::ExportSettings st; st.width = 640; st.height = 360;
        const nexo::XmlResult r = nexo::buildMltXml(tl, st);
        if (!r.ok) { std::fprintf(stderr, "error: %s\n", r.error.c_str()); return 1; }
        std::ofstream f(argv[1], std::ios::binary);
        f << r.xml;
        return f ? 0 : 1;
    }
    const auto v1 = tl.addTrack("V1", nexo::TrackKind::Video);
    const auto a1 = tl.addTrack("A1", nexo::TrackKind::Audio);
    nexo::Clip c;
    c.mediaPath = argv[2]; c.sourceIn = 0; c.duration = 60; c.timelineStart = 0;
    tl.addClip(v1, c);
    c.mediaPath = argv[3]; c.sourceIn = 30; c.duration = 60; c.timelineStart = 60;
    tl.addClip(v1, c);
    c.mediaPath = argv[2]; c.sourceIn = 0; c.duration = 120; c.timelineStart = 0;
    tl.addClip(a1, c);          // el audio de A suena durante toda la secuencia

    nexo::ExportSettings st;
    st.width = 640; st.height = 360;
    const nexo::XmlResult r = nexo::buildMltXml(tl, st);
    if (!r.ok) { std::fprintf(stderr, "error: %s\n", r.error.c_str()); return 1; }
    std::ofstream f(argv[1], std::ios::binary);
    f << r.xml;
    return f ? 0 : 1;
}
