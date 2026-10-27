#include "Previewer.h"

#include <QFile>

#include "MeltRunner.h"

Previewer::Previewer(QObject* parent) : QObject(parent), melt_(meltrunner::meltPath()) {}

void Previewer::request(const nexo::Timeline& timeline, nexo::Frame frame) {
    if (!available()) return;
    pendingTl_ = timeline;
    pendingFrame_ = frame;
    hasPending_ = true;
    if (!proc_) startPending();
}

void Previewer::startPending() {
    hasPending_ = false;
    const nexo::Frame dur = pendingTl_.duration();
    if (dur <= 0) { emit ready(QImage(), 0); return; }
    const nexo::Frame frame = pendingFrame_ >= dur ? dur - 1 : (pendingFrame_ < 0 ? 0 : pendingFrame_);

    nexo::ExportSettings st;
    st.width = 640;
    st.height = 360;
    const nexo::XmlResult xml = nexo::buildMltXml(pendingTl_, st);
    if (!xml.ok) { emit failed(QString::fromStdString(xml.error)); return; }

    tmp_ = std::make_unique<QTemporaryDir>();
    if (!tmp_->isValid()) { emit failed("No se pudo crear una carpeta temporal."); return; }
    const QString xmlPath = tmp_->filePath("vista.mlt");
    QFile f(xmlPath);
    if (!f.open(QIODevice::WriteOnly) || f.write(xml.xml.data(), qint64(xml.xml.size())) != qint64(xml.xml.size())) {
        emit failed("No se pudo escribir el proyecto temporal.");
        return;
    }
    f.close();
    pngPath_ = tmp_->filePath("frame.png");
    runningFrame_ = frame;
    log_.clear();

    proc_ = new QProcess(this);
    meltrunner::prepare(*proc_, melt_);
    proc_->setProcessChannelMode(QProcess::MergedChannels);
    connect(proc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        proc_->deleteLater();
        proc_ = nullptr;
        emit failed("No se pudo ejecutar melt.");
    });
    connect(proc_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st2) {
        log_ = QString::fromLocal8Bit(proc_->readAll());
        proc_->deleteLater();
        proc_ = nullptr;
        if (hasPending_) { startPending(); return; }   // ya hay una petición más reciente
        QImage img(pngPath_);
        if (st2 == QProcess::NormalExit && code == 0 && !img.isNull()) emit ready(img, runningFrame_);
        else emit failed("No se pudo generar la vista previa.\n" + meltrunner::lastUsefulLines(log_, 6));
    });
    const QString pos = QString::number(frame);
    proc_->start(melt_, {xmlPath, "in=" + pos, "out=" + pos, "-consumer", "avformat:" + pngPath_, "vcodec=png"});
}
