#include "TextRender.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>

namespace textrender {

bool renderPng(const nexo::TextProps& t, int width, int height, const QString& path) {
    if (width <= 0 || height <= 0) return false;
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QFont f(QString::fromStdString(t.font));
        f.setPixelSize(qMax(4, int(height * t.size / 100.0)));
        f.setBold(t.bold);
        const QFontMetricsF fm(f);
        const QStringList lines = QString::fromStdString(t.text).split('\n');
        const double lineH = fm.lineSpacing();
        const double blockH = lineH * lines.size();
        const double cx = width * t.posX / 100.0, cy = height * t.posY / 100.0;
        QPainterPath path;
        for (int i = 0; i < lines.size(); ++i) {
            const double w = fm.horizontalAdvance(lines[i]);
            const double baseline = cy - blockH / 2 + lineH * i + fm.ascent();
            path.addText(cx - w / 2, baseline, f, lines[i]);
        }
        // Contorno oscuro fino para que el texto se lea sobre cualquier fondo.
        QPen pen(QColor(0, 0, 0, 200));
        pen.setWidthF(qMax(1.0, height * 0.004));
        pen.setJoinStyle(Qt::RoundJoin);
        p.strokePath(path, pen);
        p.fillPath(path, QColor((t.color >> 16) & 0xFF, (t.color >> 8) & 0xFF, t.color & 0xFF));
    }
    return img.save(path, "PNG");
}

bool prepare(const nexo::Timeline& tl, nexo::ExportSettings& st, const QString& dir, QString& error) {
    for (std::size_t i = 0; i < tl.trackCount(); ++i)
        for (const nexo::Clip& c : tl.track(i).clips) {
            if (c.kind != nexo::ClipKind::Text) continue;
            const QString file = dir + "/texto_" + QString::number(c.id) + ".png";
            if (!renderPng(c.text, st.width, st.height, file)) {
                error = "No se pudo dibujar el texto del clip " + QString::number(c.id) + ".";
                return false;
            }
            st.textImages[c.id] = file.toStdString();
        }
    return true;
}

}  // namespace textrender
