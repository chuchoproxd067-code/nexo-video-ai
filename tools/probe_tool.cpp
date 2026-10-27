// Herramienta de CI: lee un XML producido por `melt <archivo> out=0 -consumer xml` y
// muestra lo que NEXO entiende de él. El workflow compara contra los valores esperados.
//   probe_tool <probe.mlt> <fps_num> <fps_den>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "core/MediaProbe.h"

int main(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "uso: %s <probe.mlt> <fps_num> <fps_den>\n", argv[0]); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) { std::fprintf(stderr, "no se puede abrir %s\n", argv[1]); return 2; }
    std::ostringstream ss;
    ss << f.rdbuf();
    const nexo::FrameRate fps{std::atoi(argv[2]), std::atoi(argv[3])};
    const nexo::ProbeResult r = nexo::parseMeltProbeXml(ss.str(), fps);
    if (!r.ok) { std::printf("error=%s\n", r.error.c_str()); return 1; }
    const char* kind = r.info.kind == nexo::MediaKind::Video ? "video" : r.info.kind == nexo::MediaKind::Audio ? "audio" : "image";
    std::printf("kind=%s frames=%lld seconds=%.3f width=%d height=%d video=%d audio=%d\n", kind,
                static_cast<long long>(r.info.frames), r.info.seconds, r.info.width, r.info.height,
                r.info.hasVideo ? 1 : 0, r.info.hasAudio ? 1 : 0);
    return 0;
}
