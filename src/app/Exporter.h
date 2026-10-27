#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <memory>

#include "core/MltXml.h"

// Exporta la línea de tiempo: genera el MLT XML y lanza `melt` como proceso aparte
// (DECISIONS/0004). El progreso se lee de la salida de melt.
class Exporter : public QObject {
    Q_OBJECT
public:
    explicit Exporter(QObject* parent = nullptr);

    bool available() const { return !melt_.isEmpty(); }
    bool running() const { return proc_ != nullptr; }
    // Devuelve false (y `error`) si no se pudo empezar; nada se ejecuta en ese caso.
    bool start(const nexo::Timeline& timeline, const nexo::ExportSettings& settings,
               const QString& outPath, QString& error);
    void cancel();

signals:
    void progress(int percent);
    void finished(bool ok, const QString& message);

private:
    void onStderr();

    QString melt_;
    QProcess* proc_ = nullptr;
    std::unique_ptr<QTemporaryDir> tmp_;
    QString out_;
    QString log_;
    nexo::Frame total_ = 0;
    bool cancelled_ = false;
};
