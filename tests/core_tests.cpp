// Pruebas unitarias del núcleo. Sin dependencias externas: cada CHECK que falla
// se imprime y el programa termina con código distinto de 0.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/History.h"
#include "core/Json.h"
#include "core/MediaProbe.h"
#include "core/MltXml.h"
#include "core/Project.h"
#include "core/Timeline.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                             \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!(cond)) {                                                          \
            ++g_failures;                                                       \
            std::printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                       \
    } while (0)

using namespace nexo;

static Clip makeClip(Frame start, Frame dur, Frame in = 0, const char* path = "a.mp4") {
    Clip c;
    c.mediaPath = path;
    c.sourceIn = in;
    c.duration = dur;
    c.timelineStart = start;
    return c;
}

static void testTimecode() {
    const FrameRate fps{30, 1};
    CHECK(secondsToFrames(1.0, fps) == 30);
    CHECK(secondsToFrames(0.5, fps) == 15);
    CHECK(framesToSeconds(45, fps) == 1.5);
    CHECK(toTimecode(0, fps) == "00:00:00:00");
    CHECK(toTimecode(30 * 3661 + 5, fps) == "01:01:01:05");
    CHECK(toTimecode(-1, fps) == "--:--:--:--");
    const FrameRate bad{0, 1};
    CHECK(secondsToFrames(1.0, bad) == 0);
    CHECK(toTimecode(10, bad) == "--:--:--:--");
    // 29.97 fps: se muestra con base 30.
    CHECK(toTimecode(30, FrameRate{30000, 1001}) == "00:00:01:00");
}

static void testAddAndOverlap() {
    Timeline tl;
    const auto v = tl.addTrack("V1", TrackKind::Video);
    CHECK(tl.addClip(v, makeClip(0, 100)).ok);
    CHECK(!tl.addClip(v, makeClip(50, 100)).ok);          // solape
    CHECK(tl.addClip(v, makeClip(100, 50)).ok);           // contiguo permitido
    CHECK(!tl.addClip(v, makeClip(200, 0)).ok);           // duración 0
    CHECK(!tl.addClip(v, makeClip(-5, 10)).ok);           // posición negativa
    CHECK(!tl.addClip(9, makeClip(0, 10)).ok);            // pista inexistente
    Clip noPath = makeClip(300, 10);
    noPath.mediaPath.clear();
    CHECK(!tl.addClip(v, noPath).ok);
    CHECK(tl.duration() == 150);
    CHECK(tl.validate().empty());
}

static void testSplit() {
    Timeline tl;
    const auto v = tl.addTrack("V1", TrackKind::Video);
    const ClipId id = tl.addClip(v, makeClip(10, 100, 20)).clip;
    Result r = tl.splitClip(id, 60);
    CHECK(r.ok);
    const auto& clips = tl.track(v).clips;
    CHECK(clips.size() == 2);
    CHECK(clips[0].timelineStart == 10 && clips[0].duration == 50 && clips[0].sourceIn == 20);
    CHECK(clips[1].timelineStart == 60 && clips[1].duration == 50 && clips[1].sourceIn == 70);
    CHECK(clips[0].duration + clips[1].duration == 100);       // se conserva la duración total
    CHECK(clips[0].timelineEnd() == clips[1].timelineStart);   // sin huecos ni solapes
    CHECK(clips[0].id != clips[1].id);
    CHECK(!tl.splitClip(id, 10).ok);                           // en el borde
    CHECK(!tl.splitClip(id, 60).ok);                           // ya no está dentro de `id`
    CHECK(!tl.splitClip(999, 5).ok);                           // clip inexistente
    CHECK(tl.validate().empty());
}

static void testTrim() {
    Timeline tl;
    const auto v = tl.addTrack("V1", TrackKind::Video);
    const ClipId a = tl.addClip(v, makeClip(100, 100, 50)).clip;
    CHECK(tl.trimStart(a, 20).ok);
    CHECK(tl.track(v).clips[0].timelineStart == 120);
    CHECK(tl.track(v).clips[0].sourceIn == 70);
    CHECK(tl.track(v).clips[0].duration == 80);
    CHECK(tl.trimEnd(a, -30).ok);
    CHECK(tl.track(v).clips[0].duration == 50);
    CHECK(!tl.trimEnd(a, -50).ok);                       // dejaría duración 0
    CHECK(!tl.trimStart(a, 50).ok);
    CHECK(!tl.trimStart(a, -200).ok);                    // no hay material previo en la fuente
    // Alargar hacia un vecino
    const ClipId b = tl.addClip(v, makeClip(180, 40)).clip;
    CHECK(!tl.trimEnd(a, 20).ok);                        // a termina en 170; 170+20 pisa a b (180)
    CHECK(tl.trimEnd(a, 10).ok);                         // hasta 180, contiguo
    CHECK(tl.removeClip(b).ok);
    CHECK(tl.trimEnd(a, 100).ok);
    CHECK(tl.validate().empty());
}

static void testMove() {
    Timeline tl;
    const auto v1 = tl.addTrack("V1", TrackKind::Video);
    const auto v2 = tl.addTrack("V2", TrackKind::Video);
    const auto a1 = tl.addTrack("A1", TrackKind::Audio);
    const ClipId a = tl.addClip(v1, makeClip(0, 50)).clip;
    const ClipId b = tl.addClip(v1, makeClip(60, 50)).clip;
    CHECK(!tl.moveClip(a, v1, 40).ok);                   // pisaría a b
    CHECK(tl.moveClip(a, v2, 40).ok);                    // otra pista
    CHECK(tl.track(v1).clips.size() == 1 && tl.track(v2).clips.size() == 1);
    CHECK(!tl.moveClip(a, a1, 0).ok);                    // video -> audio no permitido
    CHECK(!tl.moveClip(a, v2, -1).ok);
    CHECK(!tl.moveClip(999, v2, 0).ok);
    CHECK(tl.moveClip(b, v1, 0).ok);                     // se reordena
    CHECK(tl.track(v1).clips[0].timelineStart == 0);
    CHECK(tl.validate().empty());
}

static void testHistory() {
    Timeline base;
    const auto v = base.addTrack("V1", TrackKind::Video);
    History h(base);
    CHECK(!h.canUndo() && !h.canRedo());

    Result r = h.apply("Añadir clip", [&](Timeline& t) { return t.addClip(v, makeClip(0, 100)); });
    CHECK(r.ok);
    const ClipId id = r.clip;
    CHECK(h.timeline().track(v).clips.size() == 1);

    CHECK(h.apply("Dividir", [&](Timeline& t) { return t.splitClip(id, 40); }).ok);
    CHECK(h.timeline().track(v).clips.size() == 2);

    // Una operación fallida no cambia el estado ni el historial.
    CHECK(!h.apply("Dividir mal", [&](Timeline& t) { return t.splitClip(id, 0); }).ok);
    CHECK(h.timeline().track(v).clips.size() == 2);
    CHECK(h.log().size() == 2);

    CHECK(h.undoLabel() == "Dividir");
    CHECK(h.undo());
    CHECK(h.timeline().track(v).clips.size() == 1);
    CHECK(h.canRedo() && h.redoLabel() == "Dividir");
    CHECK(h.redo());
    CHECK(h.timeline().track(v).clips.size() == 2);

    // Tras deshacer, una nueva operación borra el "rehacer".
    CHECK(h.undo());
    CHECK(h.apply("Eliminar", [&](Timeline& t) { return t.removeClip(id); }).ok);
    CHECK(!h.canRedo());
    CHECK(h.timeline().track(v).clips.empty());
    CHECK(h.undo());
    CHECK(h.timeline().track(v).clips.size() == 1);
    CHECK(h.undo());
    CHECK(h.timeline().track(v).clips.empty());
    CHECK(!h.undo());
}

static void testJson() {
    json::Value v;
    std::string err;
    CHECK(json::parse(R"({"a":[1,2.5,-3e2,true,false,null],"s":"x\n\"\u00e1"})", v, err));
    CHECK(v.isObject() && v.get("a") && v.get("a")->items().size() == 6);
    CHECK(v.get("a")->items()[2].asNumber() == -300.0);
    CHECK(v.get("s")->asString() == "x\n\"\xc3\xa1");
    // Idas y vueltas
    json::Value back;
    CHECK(json::parse(v.dump(2), back, err) && back.dump(-1) == v.dump(-1));
    // Entradas inválidas: todas deben fallar con mensaje, sin colgarse
    const char* bad[] = {"", "{", "[1,]", "{\"a\":}", "tru", "01", "\"abc", "[1 2]",
                         "{\"a\" 1}", "1 2", "\"\\x\"", "-", "[\"\\ud800\"]"};
    for (const char* b : bad) {
        json::Value x;
        std::string e;
        CHECK(!json::parse(b, x, e) && !e.empty());
    }
    std::string deep(200, '[');
    json::Value x;
    CHECK(!json::parse(deep, x, err));   // profundidad limitada
}

static Project sampleProject() {
    Project p;
    p.name = "Mi \"proyecto\" ñ";
    Timeline tl({25, 1});
    const auto v = tl.addTrack("V1", TrackKind::Video);
    const auto a = tl.addTrack("A1", TrackKind::Audio);
    tl.addClip(v, makeClip(0, 100, 10, "C:\\videos\\a.mp4"));
    const ClipId second = tl.addClip(v, makeClip(150, 40, 0, "b.mp4")).clip;
    tl.addClip(a, makeClip(0, 190, 0, "música.wav"));
    tl.removeClip(second);                    // deja un hueco en la numeración de ids
    tl.addClip(v, makeClip(200, 30, 5, "c.mp4"));
    p.timeline = tl;
    p.addMedia("C:\\videos\\a.mp4");
    p.addMedia("C:\\videos\\a.mp4");          // duplicado ignorado
    p.addMedia("b.mp4");
    return p;
}

static void testProjectRoundTrip() {
    Project p = sampleProject();
    CHECK(p.media.size() == 2);
    CHECK(!p.addMedia(""));
    const std::string text = toJson(p);
    Project q;
    std::string err;
    CHECK(fromJson(text, q, err));
    CHECK(q.name == p.name);
    CHECK(q.media == p.media);
    CHECK(q.timeline.frameRate().num == 25);
    CHECK(q.timeline.trackCount() == 2);
    CHECK(q.timeline.track(0).clips.size() == 2);
    CHECK(q.timeline.track(0).clips[0].id == p.timeline.track(0).clips[0].id);
    CHECK(q.timeline.track(0).clips[1].id == p.timeline.track(0).clips[1].id);
    CHECK(q.timeline.track(0).clips[0].sourceIn == 10);
    CHECK(q.timeline.track(1).kind == TrackKind::Audio);
    CHECK(q.timeline.track(1).clips[0].mediaPath == "música.wav");
    CHECK(toJson(q) == text);                  // serialización estable
    // Los ids no se reutilizan tras cargar: el siguiente clip recibe un id nuevo.
    Result r = q.timeline.addClip(0, makeClip(300, 10));
    CHECK(r.ok && r.clip > q.timeline.track(0).clips[1].id);
}

static void testProjectRejectsBadFiles() {
    Project p = sampleProject();
    const std::string good = toJson(p);
    Project out;
    std::string err;
    auto replaceOnce = [&](const std::string& from, const std::string& to) {
        std::string t = good;
        const auto pos = t.find(from);
        CHECK(pos != std::string::npos);
        if (pos != std::string::npos) t.replace(pos, from.size(), to);
        return t;
    };
    CHECK(!fromJson("[]", out, err));
    CHECK(!fromJson("{}", out, err));
    CHECK(!fromJson("no es json", out, err));
    // Esquema futuro: se rechaza con un mensaje que lo explica
    CHECK(!fromJson(replaceOnce("\"schema\": 2", "\"schema\": 99"), out, err));
    CHECK(err.find("más nueva") != std::string::npos);
    CHECK(!fromJson(replaceOnce("\"schema\": 2", "\"schema\": 0"), out, err));
    // Valores inválidos
    CHECK(!fromJson(replaceOnce("\"num\": 25", "\"num\": 0"), out, err));
    CHECK(!fromJson(replaceOnce("\"duration\": 100", "\"duration\": -5"), out, err));
    CHECK(!fromJson(replaceOnce("\"duration\": 100", "\"duration\": 1.5"), out, err));
    CHECK(!fromJson(replaceOnce("\"kind\": \"video\"", "\"kind\": \"otro\""), out, err));
    // Solape entre clips de la misma pista (start 0 dur 100 y start 200 -> mover a 50)
    CHECK(!fromJson(replaceOnce("\"start\": 200", "\"start\": 50"), out, err));
    // Ids duplicados
    {
        Project d = sampleProject();
        const ClipId id0 = d.timeline.track(0).clips[0].id;
        const std::string t = toJson(d);
        const std::string needle = "\"id\": " + std::to_string(d.timeline.track(1).clips[0].id);
        const std::string repl = "\"id\": " + std::to_string(id0);
        std::string t2 = t;
        t2.replace(t2.find(needle), needle.size(), repl);
        CHECK(!fromJson(t2, out, err));
    }
    // Un fallo no toca el proyecto de salida
    Project keep = sampleProject();
    CHECK(!fromJson("{}", keep, err));
    CHECK(keep.timeline.trackCount() == 2);
}

static void testProjectFiles() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "nexo_test_project";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string file = (dir / "p.nexo").string();

    Project p = sampleProject();
    std::string err;
    CHECK(saveProject(p, file, err));
    CHECK(!fs::exists(file + ".tmp"));
    Project q;
    CHECK(loadProject(file, q, err));
    CHECK(toJson(q) == toJson(p));

    // Sobrescribir un proyecto existente
    p.name = "Segunda versión";
    CHECK(saveProject(p, file, err));
    CHECK(loadProject(file, q, err) && q.name == "Segunda versión");

    // Archivo inexistente y directorio inexistente
    CHECK(!loadProject((dir / "no_existe.nexo").string(), q, err) && !err.empty());
    CHECK(!saveProject(p, (dir / "no" / "hay" / "p.nexo").string(), err) && !err.empty());
    CHECK(!saveProject(p, "", err));

    // Archivo truncado/dañado
    {
        std::ofstream f(dir / "roto.nexo", std::ios::binary);
        f << toJson(p).substr(0, 60);
    }
    CHECK(!loadProject((dir / "roto.nexo").string(), q, err));

    // Detección de medios que faltan
    Project m;
    Timeline tl({30, 1});
    const auto v = tl.addTrack("V1", TrackKind::Video);
    const std::string real = (dir / "real.mp4").string();
    { std::ofstream f(real, std::ios::binary); f << "x"; }
    tl.addClip(v, makeClip(0, 10, 0, real.c_str()));
    tl.addClip(v, makeClip(10, 10, 0, (dir / "falta.mp4").string().c_str()));
    m.timeline = tl;
    m.addMedia(real);
    m.addMedia((dir / "falta.mp4").string());     // también está en un clip: no se repite
    m.addMedia((dir / "otra_falta.mov").string());
    const auto missing = m.missingMedia();
    CHECK(missing.size() == 2);
    CHECK(missing.size() == 2 && missing[0] == (dir / "falta.mp4").string());
    CHECK(missing.size() == 2 && missing[1] == (dir / "otra_falta.mov").string());
    fs::remove_all(dir);
}

static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

static void testMltXml() {
    Timeline tl({30, 1});
    const auto v = tl.addTrack("V1", TrackKind::Video);
    const auto a = tl.addTrack("A1", TrackKind::Audio);
    tl.addClip(v, makeClip(10, 50, 20, "C:\\media\\a&b.mp4"));    // hueco inicial de 10
    tl.addClip(v, makeClip(80, 20, 0, "c.mp4"));                   // hueco de 20
    tl.addClip(a, makeClip(0, 100, 0, "m.wav"));
    ExportSettings st;
    st.width = 640; st.height = 360;
    const XmlResult r = buildMltXml(tl, st);
    CHECK(r.ok && r.error.empty());
    CHECK(contains(r.xml, "frame_rate_num=\"30\""));
    CHECK(contains(r.xml, "width=\"640\" height=\"360\""));
    CHECK(contains(r.xml, "display_aspect_num=\"16\" display_aspect_den=\"9\""));
    CHECK(contains(r.xml, "<tractor id=\"main\" in=\"0\" out=\"99\">"));   // 100 fotogramas
    CHECK(contains(r.xml, "<blank length=\"10\"/>"));
    CHECK(contains(r.xml, "in=\"20\" out=\"69\""));                      // 20 + 50 - 1
    CHECK(contains(r.xml, "<blank length=\"20\"/>"));                    // 60 -> 80
    CHECK(contains(r.xml, "C:/media/a&amp;b.mp4"));                      // escape y barras
    CHECK(!contains(r.xml, "a&b"));
    CHECK(contains(r.xml, "hide=\"video\""));                            // pista de audio
    // Una mezcla por pista del proyecto y un composite solo para la de video.
    std::size_t mixes = 0, comps = 0;
    for (std::size_t p = 0; (p = r.xml.find(">mix<", p)) != std::string::npos; ++p) ++mixes;
    for (std::size_t p = 0; (p = r.xml.find(">composite<", p)) != std::string::npos; ++p) ++comps;
    CHECK(mixes == 2 && comps == 1);
    // Cada etiqueta de apertura de los elementos principales tiene su cierre.
    CHECK(contains(r.xml, "</mlt>") && contains(r.xml, "</tractor>") && contains(r.xml, "</multitrack>"));

    // Errores explícitos
    CHECK(!buildMltXml(Timeline({30, 1}), st).ok);                       // vacía
    ExportSettings odd; odd.width = 641; odd.height = 360;
    CHECK(!buildMltXml(tl, odd).ok);
    ExportSettings zero; zero.width = 0;
    CHECK(!buildMltXml(tl, zero).ok);
}

// Los <producer> son los que devolvió MLT 7.42 en CI (resumidos); la línea <profile> se
// reconstruyó con el formato habitual del consumidor xml y se contrasta en CI con la real.
static const char* kProfile30 =
    "<mlt><profile description=\"x\" width=\"640\" height=\"360\" progressive=\"1\" "
    "frame_rate_num=\"30\" frame_rate_den=\"1\" colorspace=\"601\"/>\n";
static const char* kProfile25 =
    "<mlt><profile description=\"PAL\" width=\"720\" height=\"576\" "
    "frame_rate_num=\"25\" frame_rate_den=\"1\"/>\n";

static std::string videoXml() {
    return std::string(kProfile30) +
        "<producer id=\"producer0\" in=\"0\" out=\"0\">\n"
        "<property name=\"length\">120</property>\n"
        "<property name=\"resource\">C:/a &amp; b/clip.mp4</property>\n"
        "<property name=\"meta.media.nb_streams\">2</property>\n"
        "<property name=\"meta.media.0.codec.width\">640</property>\n"
        "<property name=\"meta.media.0.codec.height\">360</property>\n"
        "<property name=\"audio_index\">1</property>\n"
        "<property name=\"video_index\">0</property>\n"
        "<property name=\"mlt_service\">avformat</property>\n"
        "</producer></mlt>";
}

static void testMediaProbe() {
    const FrameRate p30{30, 1};
    ProbeResult v = parseMeltProbeXml(videoXml(), p30);
    CHECK(v.ok && v.info.kind == MediaKind::Video);
    CHECK(v.info.frames == 120 && v.info.width == 640 && v.info.height == 360);
    CHECK(v.info.hasVideo && v.info.hasAudio);
    CHECK(v.info.seconds > 3.999 && v.info.seconds < 4.001);
    // Mismo medio con otro fps de proyecto: se conserva la duración en segundos.
    ProbeResult v24 = parseMeltProbeXml(videoXml(), FrameRate{24, 1});
    CHECK(v24.ok && v24.info.frames == 96);

    // Audio puro: melt usó PAL (25 fps); 50 fotogramas = 2 s = 60 fotogramas a 30 fps.
    const std::string audio = std::string(kProfile25) +
        "<producer id=\"producer0\" in=\"0\" out=\"0\"><property name=\"length\">50</property>"
        "<property name=\"audio_index\">0</property><property name=\"video_index\">-1</property>"
        "<property name=\"mlt_service\">avformat</property></producer></mlt>";
    ProbeResult a = parseMeltProbeXml(audio, p30);
    CHECK(a.ok && a.info.kind == MediaKind::Audio && a.info.frames == 60);
    CHECK(!a.info.hasVideo && a.info.hasAudio && a.info.width == 0);

    // Imagen: length no es una duración útil; se usan 5 s.
    const std::string image = std::string(kProfile25) +
        "<producer id=\"producer0\" in=\"0\" out=\"0\"><property name=\"length\">15000</property>"
        "<property name=\"meta.media.width\">320</property><property name=\"meta.media.height\">240</property>"
        "<property name=\"mlt_service\">qimage</property></producer></mlt>";
    ProbeResult im = parseMeltProbeXml(image, p30);
    CHECK(im.ok && im.info.kind == MediaKind::Image && im.info.frames == 150);
    CHECK(im.info.width == 320 && im.info.height == 240);

    // Casos de error, todos con mensaje
    CHECK(!parseMeltProbeXml("", p30).ok);
    CHECK(!parseMeltProbeXml("basura", p30).ok);
    CHECK(!parseMeltProbeXml("<mlt></mlt>", p30).ok);
    CHECK(!parseMeltProbeXml(std::string(kProfile30) + "</mlt>", p30).ok);                 // sin producer
    CHECK(!parseMeltProbeXml("<mlt><producer><property name=\"length\">5</property></producer></mlt>", p30).ok);  // sin perfil
    CHECK(!parseMeltProbeXml(std::string(kProfile30) + "<producer><property name=\"length\">5</property></producer></mlt>", p30).ok);  // sin streams
    std::string zero = videoXml();
    zero.replace(zero.find(">120<"), 5, ">0<");
    CHECK(!parseMeltProbeXml(zero, p30).ok);
    CHECK(!parseMeltProbeXml(videoXml(), FrameRate{0, 1}).ok);
}

static void testClipFx() {
    Timeline tl({30, 1});
    const auto v = tl.addTrack("V1", TrackKind::Video);
    const auto a = tl.addTrack("A1", TrackKind::Audio);
    const ClipId id = tl.addClip(v, makeClip(0, 100, 10)).clip;
    const ClipId ida = tl.addClip(a, makeClip(0, 100)).clip;
    CHECK(tl.setClipFx(id, 0.5, 20, 30).ok);
    CHECK(!tl.setClipFx(id, 5.0, 0, 0).ok);               // volumen fuera de rango
    CHECK(!tl.setClipFx(id, -0.1, 0, 0).ok);
    CHECK(!tl.setClipFx(id, 1.0, -1, 0).ok);              // fundido negativo
    CHECK(!tl.setClipFx(id, 1.0, 60, 50).ok);             // suman más que el clip
    CHECK(!tl.setClipFx(999, 1.0, 0, 0).ok);
    CHECK(tl.track(v).clips[0].volume == 0.5 && tl.track(v).clips[0].fadeIn == 20 && tl.track(v).clips[0].fadeOut == 30);
    CHECK(tl.validate().empty());

    // Un recorte no deja fundidos mayores que el clip.
    Timeline t2 = tl;
    CHECK(t2.trimEnd(id, -70).ok);                         // duración 30
    const Clip& c2 = t2.track(v).clips[0];
    CHECK(c2.fadeIn + c2.fadeOut <= c2.duration && c2.fadeIn == 20 && c2.fadeOut == 10);
    CHECK(t2.validate().empty());

    // Al dividir: la entrada se queda a la izquierda, la salida pasa a la derecha, el volumen se hereda.
    Timeline t3 = tl;
    CHECK(t3.splitClip(id, 50).ok);
    const Clip& l = t3.track(v).clips[0];
    const Clip& rr = t3.track(v).clips[1];
    CHECK(l.fadeIn == 20 && l.fadeOut == 0 && rr.fadeIn == 0 && rr.fadeOut == 30);
    CHECK(l.volume == 0.5 && rr.volume == 0.5);
    CHECK(t3.validate().empty());

    // XML: filtros con fotogramas del medio fuente (sourceIn = 10, fin = 109).
    ExportSettings st; st.width = 640; st.height = 360;
    Timeline t4 = tl;
    t4.setClipFx(ida, 2.0, 0, 0);
    const XmlResult r = buildMltXml(t4, st);
    CHECK(r.ok);
    CHECK(contains(r.xml, "<property name=\"gain\">0.5</property>"));                         // ganancia constante
    CHECK(contains(r.xml, "<filter in=\"10\" out=\"29\">"));                                  // rampa de entrada
    CHECK(contains(r.xml, "<filter in=\"80\" out=\"109\">"));                                 // rampa de salida
    CHECK(contains(r.xml, "<property name=\"alpha\">10=0;30=1;79=1;109=0</property>"));
    CHECK(contains(r.xml, "<property name=\"gain\">2</property>"));          // audio: solo volumen
    std::size_t brightness = 0;
    for (std::size_t p = 0; (p = r.xml.find(">brightness<", p)) != std::string::npos; ++p) ++brightness;
    CHECK(brightness == 1);                                                    // no en pistas de audio
    // Sin efectos no se emite ningún filtro.
    Timeline plain({30, 1});
    plain.addTrack("V1", TrackKind::Video);
    plain.addClip(0, makeClip(0, 10));
    CHECK(!contains(buildMltXml(plain, st).xml, "<filter>"));

    // Proyecto: ida y vuelta, y compatibilidad con archivos sin los campos nuevos.
    Project p; p.timeline = t4;
    Project q; std::string err;
    CHECK(fromJson(toJson(p), q, err));
    CHECK(q.timeline.track(v).clips[0].volume == 0.5 && q.timeline.track(v).clips[0].fadeIn == 20 && q.timeline.track(v).clips[0].fadeOut == 30);
    CHECK(q.timeline.track(a).clips[0].volume == 2.0);
    Project plainP; plainP.timeline = plain;
    CHECK(!contains(toJson(plainP), "volume"));
    Project q2;
    CHECK(fromJson(toJson(plainP), q2, err) && q2.timeline.track(0).clips[0].volume == 1.0);
}

static void testTransitions() {
    Timeline tl({30, 1});
    const auto v = tl.addTrack("V1", TrackKind::Video);
    const ClipId a = tl.addClip(v, makeClip(0, 60, 0, "a.mp4")).clip;
    const ClipId b = tl.addClip(v, makeClip(60, 60, 0, "b.mp4")).clip;
    const ClipId c = tl.addClip(v, makeClip(120, 60, 0, "c.mp4")).clip;
    CHECK(!tl.setTransition(a, 10).ok);                    // el primero no tiene anterior
    CHECK(!tl.setTransition(b, 61).ok);                    // más larga que el clip anterior
    CHECK(!tl.setTransition(b, -1).ok);
    CHECK(tl.duration() == 180);
    CHECK(tl.setTransition(b, 20).ok);
    CHECK(tl.track(v).clips[1].timelineStart == 40 && tl.track(v).clips[1].transitionIn == 20);
    CHECK(tl.track(v).clips[2].timelineStart == 100);      // el siguiente se desplaza
    CHECK(tl.duration() == 160);                           // 180 - 20
    CHECK(tl.validate().empty());

    // Operaciones protegidas.
    CHECK(!tl.moveClip(b, v, 300).ok);
    CHECK(!tl.trimStart(b, 5).ok);
    CHECK(!tl.trimEnd(a, -5).ok);
    CHECK(!tl.trimEnd(a, 5).ok);
    CHECK(!tl.addClip(v, makeClip(50, 20, 0, "x.mp4")).ok);   // no se puede apilar sobre la zona de mezcla
    {   // dividir dentro de la zona de mezcla: error y nada cambia
        Timeline t2 = tl;
        CHECK(!t2.splitClip(b, 45).ok);
        CHECK(t2.track(v).clips.size() == 3 && t2.track(v).clips[1].duration == 60 && t2.validate().empty());
    }
    {   // dividir fuera de la zona: la transición se queda a la izquierda
        Timeline t2 = tl;
        CHECK(t2.splitClip(b, 70).ok);
        CHECK(t2.track(v).clips.size() == 4 && t2.track(v).clips[1].transitionIn == 20 && t2.track(v).clips[2].transitionIn == 0);
        CHECK(t2.validate().empty());
    }
    {   // eliminar el clip anterior quita la transición del siguiente
        Timeline t2 = tl;
        CHECK(t2.removeClip(a).ok);
        CHECK(t2.track(v).clips[0].transitionIn == 0 && t2.validate().empty());
    }
    {   // transición encadenada: B-C también
        Timeline t2 = tl;
        CHECK(t2.setTransition(c, 30).ok);
        CHECK(t2.validate().empty());
        CHECK(!t2.setTransition(c, 45).ok);                // B (60) no da para 20 + 45
        CHECK(t2.track(v).clips[2].transitionIn == 30);    // sin cambios tras el error
    }

    // XML: A sin su cola, zona de mezcla, B sin su cabeza.
    ExportSettings st; st.width = 640; st.height = 360;
    const XmlResult r = buildMltXml(tl, st);
    CHECK(r.ok);
    CHECK(contains(r.xml, "<tractor id=\"main\" in=\"0\" out=\"159\">"));
    CHECK(contains(r.xml, "<entry producer=\"clip_1\" in=\"0\" out=\"39\"/>"));       // A: 60 - 20
    CHECK(contains(r.xml, "<entry producer=\"mix_2\" in=\"0\" out=\"19\"/>"));
    CHECK(contains(r.xml, "<entry producer=\"clip_2\" in=\"20\" out=\"59\"/>"));      // B tras la mezcla
    CHECK(contains(r.xml, "<entry producer=\"clip_1\" in=\"40\" out=\"59\"/>"));      // cola de A en la mezcla
    CHECK(contains(r.xml, ">luma<"));
    CHECK(!contains(r.xml, "<blank"));                                               // sin huecos

    // Quitarla restablece las posiciones.
    CHECK(tl.setTransition(b, 0).ok);
    CHECK(tl.track(v).clips[1].timelineStart == 60 && tl.track(v).clips[2].timelineStart == 120 && tl.duration() == 180);

    // Proyecto: ida y vuelta.
    Timeline t3 = tl;
    t3.setTransition(b, 15);
    Project p; p.timeline = t3;
    Project q; std::string err;
    CHECK(fromJson(toJson(p), q, err));
    CHECK(q.timeline.track(v).clips[1].transitionIn == 15 && q.timeline.track(v).clips[1].timelineStart == 45);
    CHECK(toJson(q) == toJson(p));
    // Un archivo con solape sin transición se rechaza.
    std::string bad = toJson(p);
    const std::size_t pos = bad.find("\"transitionIn\"");
    CHECK(pos != std::string::npos);
    bad.replace(pos, std::string("\"transitionIn\": 15").size(), "\"transitionIn\": 0");
    Project q3;
    CHECK(!fromJson(bad, q3, err));
}

static void testClipProps() {
    // --- validación ---
    ClipProps d;
    CHECK(validateProps(d, 100).empty() && d.isDefault() && !d.hasTransform());
    ClipProps bad = d; bad.speed = 0.01; CHECK(!validateProps(bad, 100).empty());
    bad = d; bad.speed = 17; CHECK(!validateProps(bad, 100).empty());
    bad = d; bad.opacity = 1.5; CHECK(!validateProps(bad, 100).empty());
    bad = d; bad.scale = 0; CHECK(!validateProps(bad, 100).empty());
    bad = d; bad.cropLeft = 60; bad.cropRight = 40; CHECK(!validateProps(bad, 100).empty());
    bad = d; bad.contrast = std::nan(""); CHECK(!validateProps(bad, 100).empty());
    bad = d; bad.pitch = 3; bad.speed = 2; CHECK(!validateProps(bad, 100).empty());     // tono solo a velocidad normal
    bad = d; bad.keyframes = {{10, 0, 0, 100, 0, 1}, {10, 0, 0, 100, 0, 1}}; CHECK(!validateProps(bad, 100).empty());   // no estrictos
    bad = d; bad.keyframes = {{100, 0, 0, 100, 0, 1}}; CHECK(!validateProps(bad, 100).empty());                      // fuera del clip
    bad = d; bad.effects.assign(9, Effect{}); CHECK(!validateProps(bad, 100).empty());
    CHECK(sourceSpan(d, 90) == 90);
    ClipProps fast = d; fast.speed = 2;
    CHECK(sourceSpan(fast, 60) == 120 && durationForSpan(fast, 120) == 60);
    ClipProps fr = d; fr.freeze = true;
    CHECK(sourceSpan(fr, 60) == 1);
    for (EffectKind k : {EffectKind::Blur, EffectKind::Sharpen, EffectKind::Vignette, EffectKind::Mosaic,
                         EffectKind::Grayscale, EffectKind::Sepia, EffectKind::Invert, EffectKind::Mirror}) {
        EffectKind back = EffectKind::Blur;
        CHECK(effectFromName(effectName(k), back) && back == k);
    }

    // --- velocidad: el clip conserva su tramo del medio y cambia de duración ---
    Timeline tl({30, 1});
    const auto v = tl.addTrack("V1", TrackKind::Video);
    const ClipId c1 = tl.addClip(v, makeClip(0, 120, 30, "m.mp4")).clip;     // 120 fotogramas del medio desde el 30
    const ClipId c2 = tl.addClip(v, makeClip(300, 50, 0, "n.mp4")).clip;
    (void)c2;
    ClipProps p2 = d; p2.speed = 2;
    CHECK(tl.setClipProps(c1, p2).ok);
    CHECK(tl.track(v).clips[0].duration == 60 && tl.track(v).clips[0].sourceIn == 30);
    ClipProps p05 = d; p05.speed = 0.5;
    CHECK(tl.setClipProps(c1, p05).ok);                                      // 120 fotogramas a 0,5x -> 240
    CHECK(tl.track(v).clips[0].duration == 240);
    CHECK(tl.validate().empty());
    {   // más lento choca con el clip siguiente: error y sin cambios
        Timeline t2 = tl;
        ClipProps slow = d; slow.speed = 0.1;
        const Result r = t2.setClipProps(c1, slow);
        CHECK(!r.ok);
        CHECK(t2.track(v).clips[0].props.speed == tl.track(v).clips[0].props.speed);
    }
    // Recorte con velocidad 2x: avanzar 10 fotogramas de secuencia consume 20 del medio.
    {
        Timeline t2({30, 1});
        const auto vv = t2.addTrack("V1", TrackKind::Video);
        const ClipId id = t2.addClip(vv, makeClip(0, 60, 0, "m.mp4")).clip;
        ClipProps f2 = d; f2.speed = 2;
        t2.setClipProps(id, f2);                                             // ahora dura 30 (60 del medio)
        CHECK(t2.track(vv).clips[0].duration == 30);
        CHECK(t2.trimStart(id, 10).ok);
        CHECK(t2.track(vv).clips[0].sourceIn == 20 && t2.track(vv).clips[0].duration == 20 && t2.track(vv).clips[0].timelineStart == 10);
        CHECK(t2.splitClip(id, 20).ok);                                      // izquierda 10 fotogramas -> 20 del medio
        CHECK(t2.track(vv).clips[1].sourceIn == 40 && t2.track(vv).clips[1].duration == 10);
        CHECK(t2.validate().empty());
    }
    // Inversa: el inicio de la secuencia es el final del tramo del medio.
    {
        Timeline t2({30, 1});
        const auto vv = t2.addTrack("V1", TrackKind::Video);
        const ClipId id = t2.addClip(vv, makeClip(0, 60, 100, "m.mp4")).clip;   // medio [100,160)
        ClipProps rv = d; rv.reverse = true;
        CHECK(t2.setClipProps(id, rv).ok && t2.track(vv).clips[0].duration == 60);
        CHECK(t2.trimStart(id, 10).ok);
        CHECK(t2.track(vv).clips[0].sourceIn == 100 && t2.track(vv).clips[0].duration == 50);   // medio [100,150)
        CHECK(t2.splitClip(id, 30).ok);                                          // izquierda 20 fotogramas
        CHECK(t2.track(vv).clips[0].sourceIn == 130 && t2.track(vv).clips[1].sourceIn == 100);    // izq [130,150), der [100,130)
        CHECK(t2.trimEnd(t2.track(vv).clips[1].id, 20).ok);
        CHECK(t2.track(vv).clips[1].sourceIn == 80 && t2.track(vv).clips[1].duration == 50);
        CHECK(!t2.trimEnd(t2.track(vv).clips[1].id, 100).ok);                      // no hay medio anterior
        CHECK(t2.validate().empty());
    }

    // --- fotogramas clave: siguen a la imagen al dividir y recortar ---
    {
        Timeline t2({30, 1});
        const auto vv = t2.addTrack("V1", TrackKind::Video);
        const ClipId id = t2.addClip(vv, makeClip(0, 100, 0, "m.mp4")).clip;
        ClipProps kp = d;
        kp.keyframes = {{0, 0, 0, 100, 0, 1}, {40, 10, 0, 150, 0, 1}, {90, 0, 0, 100, 0, 0.5}};
        CHECK(t2.setClipProps(id, kp).ok);
        CHECK(t2.splitClip(id, 50).ok);
        CHECK(t2.track(vv).clips[0].props.keyframes.size() == 2 && t2.track(vv).clips[1].props.keyframes.size() == 1);
        CHECK(t2.track(vv).clips[1].props.keyframes[0].frame == 40);              // 90 - 50
        CHECK(t2.validate().empty());
        CHECK(t2.trimStart(t2.track(vv).clips[0].id, 10).ok);
        CHECK(t2.track(vv).clips[0].props.keyframes.size() == 1 && t2.track(vv).clips[0].props.keyframes[0].frame == 30);
        CHECK(t2.validate().empty());
    }

    // --- texto ---
    {
        Timeline t2({30, 1});
        const auto vv = t2.addTrack("V1", TrackKind::Video);
        const auto aa = t2.addTrack("A1", TrackKind::Audio);
        TextProps tp; tp.text = "Hola";
        const Result r = t2.addTextClip(vv, tp, 10, 60);
        CHECK(r.ok && t2.track(vv).clips[0].kind == ClipKind::Text && t2.duration() == 70);
        CHECK(!t2.addTextClip(aa, tp, 0, 30).ok);                                  // no en pistas de audio
        TextProps empty; CHECK(!t2.addTextClip(vv, empty, 100, 30).ok);
        TextProps big = tp; big.size = 80; CHECK(!t2.addTextClip(vv, big, 100, 30).ok);
        tp.text = "Adiós"; CHECK(t2.setText(r.clip, tp).ok && t2.track(vv).clips[0].text.text == "Adiós");
        ClipProps sp = d; sp.speed = 2;
        CHECK(!t2.setClipProps(r.clip, sp).ok);                                    // un texto no tiene velocidad
        CHECK(t2.splitClip(r.clip, 40).ok && t2.track(vv).clips[1].kind == ClipKind::Text);
        CHECK(t2.validate().empty());
    }

    // --- audio separado ---
    {
        Timeline t2({30, 1});
        const auto vv = t2.addTrack("V1", TrackKind::Video);
        const auto aa = t2.addTrack("A1", TrackKind::Audio);
        const ClipId id = t2.addClip(vv, makeClip(5, 60, 10, "m.mp4")).clip;
        CHECK(!t2.detachAudio(id, vv).ok);                                         // destino debe ser audio
        const Result r = t2.detachAudio(id, aa);
        CHECK(r.ok && t2.track(aa).clips.size() == 1);
        const Clip& ac = t2.track(aa).clips[0];
        CHECK(ac.mediaPath == "m.mp4" && ac.sourceIn == 10 && ac.timelineStart == 5 && ac.duration == 60);
        CHECK(t2.track(vv).clips[0].props.muted);
        CHECK(!t2.detachAudio(id, aa).ok);                                         // ya separado
        CHECK(t2.validate().empty());
    }

    // --- ripple, pistas y marcadores ---
    {
        Timeline t2({30, 1});
        const auto vv = t2.addTrack("V1", TrackKind::Video);
        t2.addClip(vv, makeClip(0, 30, 0, "a.mp4"));
        const ClipId b = t2.addClip(vv, makeClip(30, 30, 0, "b.mp4")).clip;
        t2.addClip(vv, makeClip(60, 30, 0, "c.mp4"));
        CHECK(t2.removeClipRipple(b).ok);
        CHECK(t2.track(vv).clips.size() == 2 && t2.track(vv).clips[1].timelineStart == 30 && t2.duration() == 60);
        CHECK(t2.validate().empty());
        // con transición: el siguiente se queda pegado al anterior
        Timeline t3({30, 1});
        const auto v3 = t3.addTrack("V1", TrackKind::Video);
        const ClipId x = t3.addClip(v3, makeClip(0, 60, 0, "a.mp4")).clip;
        const ClipId y = t3.addClip(v3, makeClip(60, 60, 0, "b.mp4")).clip;
        const ClipId z = t3.addClip(v3, makeClip(120, 60, 0, "c.mp4")).clip;
        CHECK(t3.setTransition(y, 20).ok && t3.setTransition(z, 10).ok);
        (void)x;
        CHECK(t3.removeClipRipple(y).ok);
        CHECK(t3.track(v3).clips.size() == 2 && t3.track(v3).clips[1].transitionIn == 0 && t3.track(v3).clips[1].timelineStart == 60);
        CHECK(t3.validate().empty());

        // pistas
        CHECK(!t2.removeTrack(vv).ok);                                             // no vacía
        const auto extra = t2.addTrack("V2", TrackKind::Video);
        CHECK(t2.setTrackFlags(extra, true, true, false).ok && t2.track(extra).muted && t2.track(extra).hidden);
        CHECK(t2.renameTrack(extra, "Títulos").ok && t2.track(extra).name == "Títulos");
        CHECK(!t2.renameTrack(extra, "").ok);
        CHECK(t2.removeTrack(extra).ok && t2.trackCount() == 1);
        // bloqueo: ninguna operación modifica la pista
        const ClipId first = t2.track(vv).clips[0].id;
        t2.setTrackFlags(vv, false, false, true);
        CHECK(!t2.moveClip(first, vv, 500).ok && !t2.trimEnd(first, -5).ok && !t2.trimStart(first, 5).ok);
        CHECK(!t2.splitClip(first, 10).ok && !t2.removeClip(first).ok && !t2.removeClipRipple(first).ok);
        CHECK(!t2.setClipProps(first, d).ok && !t2.setClipFx(first, 1.0, 0, 0).ok && !t2.addClip(vv, makeClip(500, 10)).ok);
        t2.setTrackFlags(vv, false, false, false);
        CHECK(t2.splitClip(first, 10).ok);
        // marcadores
        CHECK(t2.addMarker(90, "fin").ok && t2.addMarker(10, "inicio").ok && t2.addMarker(10, "otra").ok);
        CHECK(t2.markers().size() == 2 && t2.markers()[0].frame == 10 && t2.markers()[0].name == "otra");
        CHECK(!t2.addMarker(-1, "x").ok && !t2.removeMarker(5).ok && t2.removeMarker(0).ok && t2.markers().size() == 1);
    }

    // --- guardado: todo ida y vuelta, y un proyecto de esquema 1 sigue abriendo ---
    {
        Timeline t2({25, 1});
        const auto vv = t2.addTrack("V1", TrackKind::Video);
        const auto aa = t2.addTrack("A1", TrackKind::Audio);
        const ClipId id = t2.addClip(vv, makeClip(0, 100, 5, "m.mp4")).clip;
        ClipProps full = d;
        full.speed = 1.5; full.reverse = true; full.posX = 10; full.posY = -20; full.scale = 80; full.rotation = 15; full.opacity = 0.7;
        full.cropLeft = 5; full.cropTop = 2.5; full.flipH = true; full.brightness = 0.1; full.contrast = 1.2; full.saturation = 0.8;
        full.temperature = 5500; full.effects = {{EffectKind::Blur, 0.3}, {EffectKind::Sepia, 1.0}};
        full.chroma = {true, 0x00FF00, 0.4, 0.2};
        full.keyframes = {{0, 0, 0, 100, 0, 1}, {30, 5, 5, 120, 10, 0.9}};
        full.stabilize = true; full.muted = true; full.denoise = true; full.normalize = true; full.bass = 3; full.treble = -2;
        CHECK(t2.setClipProps(id, full).ok);
        TextProps tp; tp.text = "Título \"con comillas\""; tp.bold = true; tp.color = 0xFFCC00; tp.size = 12;
        CHECK(t2.addTextClip(vv, tp, 200, 40).ok);
        t2.addClip(aa, makeClip(0, 50, 0, "s.wav"));
        t2.setTrackFlags(aa, true, false, true);
        t2.addMarker(33, "mitad");
        Project p; p.timeline = t2;
        const std::string text = toJson(p);
        Project q; std::string err;
        CHECK(fromJson(text, q, err));
        CHECK(q.timeline.track(vv).clips[0].props == t2.track(vv).clips[0].props);
        CHECK(q.timeline.track(vv).clips[1].kind == ClipKind::Text && q.timeline.track(vv).clips[1].text == tp);
        CHECK(q.timeline.track(aa).muted && q.timeline.track(aa).locked && !q.timeline.track(aa).hidden);
        CHECK(q.timeline.markers().size() == 1 && q.timeline.markers()[0].name == "mitad");
        CHECK(toJson(q) == text);
        CHECK(p.missingMedia().size() == 2);                                       // m.mp4 y s.wav; el texto no cuenta
        std::string old = text;                                                    // un archivo de esquema 1 sin los campos nuevos
        const std::size_t sp = old.find("\"schema\": 2");
        CHECK(sp != std::string::npos);
        old.replace(sp, std::string("\"schema\": 2").size(), "\"schema\": 1");
        Project q1;
        CHECK(fromJson(old, q1, err));
        // datos corruptos
        std::string corrupt = text;
        const std::size_t pp = corrupt.find("\"speed\": 1.5");
        CHECK(pp != std::string::npos);
        corrupt.replace(pp, std::string("\"speed\": 1.5").size(), "\"speed\": 50");
        Project q2;
        CHECK(!fromJson(corrupt, q2, err));
        std::string badKind = text;
        const std::size_t kp = badKind.find("\"effects\"");
        CHECK(kp != std::string::npos);
        const std::size_t bp = badKind.find("\"blur\"", kp);
        badKind.replace(bp, 6, "\"nope\"");
        CHECK(!fromJson(badKind, q2, err));
    }
}

static void testPropsXml() {
    Timeline tl({30, 1});
    const std::size_t v = tl.addTrack("V", TrackKind::Video);
    const std::size_t a = tl.addTrack("A", TrackKind::Audio);
    ExportSettings st; st.width = 640; st.height = 360;
    auto xmlOf = [&](const Timeline& t) { const XmlResult r = buildMltXml(t, st); CHECK(r.ok); return r.xml; };

    Clip c = makeClip(0, 60, 0, "m.mp4");
    const Result r1 = tl.addClip(v, c);
    CHECK(r1.ok);
    CHECK(!contains(xmlOf(tl), "affine") && !contains(xmlOf(tl), "timewarp"));   // sin propiedades: XML limpio

    ClipProps p;
    p.speed = 2; p.scale = 50; p.posX = 10; p.rotation = 20; p.flipH = true; p.brightness = 0.2;
    p.denoise = true; p.normalize = true; p.bass = 5; p.chroma.enabled = true; p.chroma.rgb = 0x00FF00;
    p.effects = {{EffectKind::Blur, 0.5}, {EffectKind::Sepia, 1}};
    CHECK(tl.setClipProps(r1.clip, p).ok);
    std::string x = xmlOf(tl);
    CHECK(contains(x, ">timewarp<") && contains(x, "2:m.mp4"));
    CHECK(contains(x, "transition.rect") && contains(x, "transition.fix_rotate_z\">20<"));
    CHECK(contains(x, "avfilter.hflip") && contains(x, "avfilter.eq") && contains(x, "avfilter.gblur"));
    CHECK(contains(x, ">rnnoise<") && contains(x, ">dynamic_loudness<") && contains(x, "avfilter.bass"));
    CHECK(contains(x, "#00ff00"));
    // escala 50 % centrada con desplazamiento +10 % de 640: x = 160 + 64 = 224, y = 90
    CHECK(contains(x, "224/90:320x180"));

    // inversa: necesita la duración del medio
    ClipProps rv; rv.reverse = true;
    Timeline t2({30, 1});
    const std::size_t v2 = t2.addTrack("V", TrackKind::Video);
    Clip c2 = makeClip(0, 30, 30, "r.mp4");
    const Result r2 = t2.addClip(v2, c2);
    CHECK(t2.setClipProps(r2.clip, rv).ok);
    CHECK(!buildMltXml(t2, st).ok);                       // sin mediaFrames
    st.mediaFrames["r.mp4"] = 120;
    x = xmlOf(t2);
    CHECK(contains(x, "-1:r.mp4") && contains(x, "in=\"60\" out=\"89\""));   // 120 - (30+30) = 60

    // congelado: hold con el fotograma
    ClipProps fz; fz.freeze = true;
    Timeline t3({30, 1});
    const std::size_t v3 = t3.addTrack("V", TrackKind::Video);
    const Result r3 = t3.addClip(v3, makeClip(0, 40, 15, "f.mp4"));
    CHECK(t3.setClipProps(r3.clip, fz).ok);
    x = xmlOf(t3);
    CHECK(contains(x, ">hold<") && contains(x, "\"frame\">15<"));

    // fotogramas clave: posiciones absolutas del producer
    ClipProps kf;
    kf.keyframes = {{0, 0, 0, 100, 0, 1}, {20, 10, 0, 50, 45, 0.5}};
    Timeline t4({30, 1});
    const std::size_t v4 = t4.addTrack("V", TrackKind::Video);
    const Result r4 = t4.addClip(v4, makeClip(0, 60, 30, "k.mp4"));
    CHECK(t4.setClipProps(r4.clip, kf).ok);
    x = xmlOf(t4);
    CHECK(contains(x, "30=0/0:640x360") && contains(x, ";50=") && contains(x, "fix_rotate_z\">30=0;50=45<"));

    // texto sin imagen = error; con imagen = qimage
    Timeline t5({30, 1});
    const std::size_t v5 = t5.addTrack("V", TrackKind::Video);
    TextProps tp; tp.text = "Hola";
    const Result r5 = t5.addTextClip(v5, tp, 0, 30);
    CHECK(r5.ok && !buildMltXml(t5, st).ok);
    st.textImages[r5.clip] = "C:\\tmp\\t.png";
    x = xmlOf(t5);
    CHECK(contains(x, ">qimage<") && contains(x, "C:/tmp/t.png"));

    // banderas de pista
    Timeline t6({30, 1});
    const std::size_t v6 = t6.addTrack("V", TrackKind::Video);
    const std::size_t a6 = t6.addTrack("A", TrackKind::Audio);
    t6.addClip(v6, makeClip(0, 30, 0, "a.mp4"));
    t6.addClip(a6, makeClip(0, 30, 0, "a.wav"));
    CHECK(t6.setTrackFlags(v6, true, false, false).ok);
    CHECK(t6.setTrackFlags(a6, true, false, false).ok);
    x = xmlOf(t6);
    CHECK(contains(x, "pl_0\" hide=\"audio\"") && contains(x, "pl_1\" hide=\"both\""));
    CHECK(t6.setTrackFlags(v6, false, true, false).ok);
    CHECK(contains(xmlOf(t6), "pl_0\" hide=\"video\""));
    (void)a;
}

int main() {
    testTimecode();
    testAddAndOverlap();
    testSplit();
    testTrim();
    testMove();
    testHistory();
    testJson();
    testProjectRoundTrip();
    testProjectRejectsBadFiles();
    testProjectFiles();
    testMltXml();
    testClipFx();
    testTransitions();
    testClipProps();
    testPropsXml();
    testMediaProbe();
    std::printf("%d comprobaciones, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
