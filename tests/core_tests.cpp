// Pruebas unitarias del núcleo. Sin dependencias externas: cada CHECK que falla
// se imprime y el programa termina con código distinto de 0.
#include <cstdio>
#include <string>

#include "core/History.h"
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

int main() {
    testTimecode();
    testAddAndOverlap();
    testSplit();
    testTrim();
    testMove();
    testHistory();
    std::printf("%d comprobaciones, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
