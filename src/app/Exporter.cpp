#include "Exporter.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include "MeltRunner.h"
#include "TextRender.h"

Exporter::Exporter(QObject* parent) : QObject(parent), melt_(meltrunner::meltPath()) {}

bool Exporter::start(const nexo::Timeline& timeline, const nexo::ExportSettings& settings,
                     const QString& outPath, QString& error) {
    if (!available()) { error = "Esta compilación no incluye melt: no se puede exportar."; return false; }
    if (running()) { error = "Ya hay una exportación en curso."; return false; }
    if (outPath.isEmpty()) { error = "No se indicó el archivo de salida."; return false; }

    tmp_ = std::make_unique<QTemporaryDir>();
    if (!tmp_->isValid()) { error = "No se pudo crear una carpeta temporal."; return false; }
    nexo::ExportSettings st = settings;
    st.mediaFrames = mediaFrames_;
    if (!textrender::prepare(timeline, st, tmp_->path(), error)) return false;
    const nexo::XmlResult xml = nexo::buildMltXml(timeline, st);
    if (!xml.ok) { error = QString::fromStdString(xml.error); return false; }
    const QString xmlPath = tmp_->filePath("proyecto.mlt");
    QFile f(xmlPath);
    if (!f.open(QIODevice::WriteOnly) || f.write(xml.xml.data(), qint64(xml.xml.size())) != qint64(xml.xml.size())) {
        error = "No se pudo escribir el proyecto temporal.";
        return false;
    }
    f.close();

    // Se borra la salida anterior para poder comprobar que este render genera un archivo nuevo.
    if (QFile::exists(outPath) && !QFile::remove(outPath)) {
        error = "No se puede sobrescribir " + outPath + " (¿está abierto en otro programa?).";
        return false;
    }

    out_ = outPath;
    log_.clear();
    total_ = timeline.duration();
    cancelled_ = false;

    proc_ = new QProcess(this);
    meltrunner::prepare(*proc_, melt_);
    connect(proc_, &QProcess::readyReadStandardError, this, &Exporter::onStderr);
    connect(proc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        proc_->deleteLater();
        proc_ = nullptr;
        emit finished(false, "No se pudo ejecutar melt.");
    });
    connect(proc_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
        onStderr();
        const bool crashed = st == QProcess::CrashExit;
        QProcess* p = proc_;
        proc_ = nullptr;
        p->deleteLater();
        const QFileInfo fi(out_);
        if (cancelled_) {
            QFile::remove(out_);
            emit finished(false, "Exportación cancelada.");
        } else if (!crashed && code == 0 && fi.exists() && fi.size() > 0) {
            emit progress(100);
            emit finished(true, out_);
        } else {
            emit finished(false, "melt terminó con error (código " + QString::number(code) + ").\n" +
                                     meltrunner::lastUsefulLines(log_, 8));
        }
    });
    QStringList args{xmlPath, "-progress2"};
    for (const std::string& a : nexo::meltConsumerArgs(settings, outPath.toStdString())) args << QString::fromStdString(a);
    proc_->start(melt_, args);
    return true;
}

void Exporter::cancel() {
    if (!proc_) return;
    cancelled_ = true;
    proc_->kill();
}

void Exporter::onStderr() {
    if (!proc_) return;
    const QString chunk = QString::fromLocal8Bit(proc_->readAllStandardError());
    log_ += chunk;
    if (log_.size() > 20000) log_ = log_.right(20000);
    // Dos formatos posibles de melt: "percentage: N" o "Current Position: N" (en fotogramas).
    static const QRegularExpression pct("percentage:\\s*(\\d+)");
    static const QRegularExpression pos("Current Position:\\s*(\\d+)");
    int value = -1;
    auto m = pct.globalMatch(chunk);
    while (m.hasNext()) value = m.next().captured(1).toInt();
    if (value < 0 && total_ > 0) {
        auto mp = pos.globalMatch(chunk);
        int last = -1;
        while (mp.hasNext()) last = mp.next().captured(1).toInt();
        if (last >= 0) value = int(qMin<qint64>(99, qint64(last) * 100 / qint64(total_)));
    }
    if (value >= 0) emit progress(qMin(value, 99));
}
