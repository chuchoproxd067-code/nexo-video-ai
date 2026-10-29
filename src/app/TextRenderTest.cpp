// Prueba (CI): el texto se dibuja como PNG RGBA a tamaño completo, transparente salvo donde hay letras.
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <cstdio>

#include "TextRender.h"

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    int fails = 0;
    auto check = [&](bool ok, const char* what) { std::printf("%s: %s\n", ok ? "OK" : "FALLA", what); if (!ok) ++fails; };

    QTemporaryDir dir;
    check(dir.isValid(), "carpeta temporal");
    nexo::TextProps t;
    t.text = "NEXO\nVIDEO";
    t.size = 14; t.color = 0xFF0000; t.posX = 50; t.posY = 50; t.bold = true;
    const QString path = dir.filePath("t.png");
    check(textrender::renderPng(t, 640, 360, path), "renderPng devuelve true");
    const QImage img(path);
    check(!img.isNull() && img.width() == 640 && img.height() == 360, "PNG de 640x360");
    check(img.hasAlphaChannel(), "tiene canal alfa");
    check(qAlpha(img.pixel(2, 2)) == 0 && qAlpha(img.pixel(637, 357)) == 0, "esquinas transparentes");
    int opaque = 0, red = 0;
    long sx = 0, sy = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QRgb p = img.pixel(x, y);
            if (qAlpha(p) > 200) { ++opaque; sx += x; sy += y; if (qRed(p) > 200 && qGreen(p) < 60) ++red; }
        }
    std::printf("píxeles opacos=%d rojos=%d\n", opaque, red);
    check(opaque > 300, "hay letras dibujadas (fuente disponible)");
    check(red > 100, "el color del texto es el pedido (rojo)");
    if (opaque > 0) {
        const double cx = double(sx) / opaque, cy = double(sy) / opaque;
        std::printf("centro del texto=(%.0f,%.0f)\n", cx, cy);
        check(cx > 280 && cx < 360 && cy > 150 && cy < 210, "texto centrado en (50%,50%)");
    }
    nexo::Timeline tl({30, 1});
    const auto v = tl.addTrack("V", nexo::TrackKind::Video);
    const auto r = tl.addTextClip(v, t, 0, 30);
    nexo::ExportSettings st; st.width = 320; st.height = 180;
    QString err;
    check(r.ok && textrender::prepare(tl, st, dir.path(), err) && st.textImages.count(r.clip) == 1, "prepare rellena textImages");
    return fails == 0 ? 0 : 1;
}
