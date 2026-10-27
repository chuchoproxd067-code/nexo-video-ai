#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "core/MediaProbe.h"

// Lee duración, tamaño y pistas de los medios lanzando `melt` (uno a la vez, sin bloquear
// la interfaz) e interpretando su salida con nexo::parseMeltProbeXml.
class MediaImporter : public QObject {
    Q_OBJECT
public:
    explicit MediaImporter(QObject* parent = nullptr);

    bool available() const { return !melt_.isEmpty(); }
    bool busy() const { return proc_ != nullptr || !queue_.isEmpty(); }
    void setProjectFps(nexo::FrameRate fps) { fps_ = fps; }
    void enqueue(const QStringList& files);

signals:
    void probed(const QString& path, const nexo::MediaInfo& info);
    void failed(const QString& path, const QString& message);
    void idle();

private:
    void next();

    QString melt_;
    nexo::FrameRate fps_{30, 1};
    QStringList queue_;
    QProcess* proc_ = nullptr;
    QString current_;
};
