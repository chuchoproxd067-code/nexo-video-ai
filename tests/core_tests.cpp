// Pruebas unitarias del núcleo. Sin dependencias externas: cada CHECK que falla
// se imprime y el programa termina con código distinto de 0.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/History.h"
#include "core/Json.h"
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
    CHECK(!fromJson(replaceOnce("\"schema\": 1", "\"schema\": 99"), out, err));
    CHECK(err.find("más nueva") != std::string::npos);
    CHECK(!fromJson(replaceOnce("\"schema\": 1", "\"schema\": 0"), out, err));
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
    std::printf("%d comprobaciones, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
