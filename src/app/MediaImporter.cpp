#include "MediaImporter.h"

#include <QFileInfo>
#include <QTimer>

#include "MeltRunner.h"

MediaImporter::MediaImporter(QObject* parent) : QObject(parent), melt_(meltrunner::meltPath()) {}

void MediaImporter::enqueue(const QStringList& files) {
    queue_ << files;
    if (!proc_) next();
}

void MediaImporter::next() {
    if (queue_.isEmpty()) {
        emit idle();
        return;
    }
    current_ = queue_.takeFirst();
    if (!available()) {
        emit failed(current_, "Esta compilación no incluye melt: no se pueden leer medios.");
        next();
        return;
    }
    if (!QFileInfo::exists(current_)) {
        emit failed(current_, "El archivo no existe.");
        next();
        return;
    }
    proc_ = new QProcess(this);
    meltrunner::prepare(*proc_, melt_);
    const QString path = current_;

    // Límite de tiempo: un archivo dañado no debe dejar la cola colgada.
    QTimer::singleShot(30000, proc_, [p = proc_] {
        if (p->state() != QProcess::NotRunning) p->kill();
    });

    connect(proc_, &QProcess::errorOccurred, this, [this, path](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;   // los demás errores llegan por finished()
        emit failed(path, "No se pudo ejecutar melt.");
        proc_->deleteLater();
        proc_ = nullptr;
        next();
    });
    connect(proc_, &QProcess::finished, this, [this, path](int, QProcess::ExitStatus) {
        QProcess* p = proc_;
        const std::string xml = p->readAllStandardOutput().toStdString();
        const nexo::ProbeResult r = nexo::parseMeltProbeXml(xml, fps_);
        if (r.ok) emit probed(path, r.info);
        else emit failed(path, QString::fromStdString(r.error));
        p->deleteLater();
        proc_ = nullptr;
        next();
    });
    proc_->start(melt_, {path, "out=0", "-consumer", "xml"});
}
