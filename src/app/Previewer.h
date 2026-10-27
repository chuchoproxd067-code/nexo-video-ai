#pragma once
#include <QImage>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <memory>

#include "core/MltXml.h"

// Vista previa de un fotograma: genera el MLT XML, pide a `melt` un PNG de esa posición y lo
// devuelve. Un solo proceso a la vez; si llegan peticiones nuevas, solo cuenta la última.
class Previewer : public QObject {
    Q_OBJECT
public:
    explicit Previewer(QObject* parent = nullptr);
    bool available() const { return !melt_.isEmpty(); }
    void request(const nexo::Timeline& timeline, nexo::Frame frame);

signals:
    void ready(const QImage& image, nexo::Frame frame);
    void failed(const QString& message);

private:
    void startPending();

    QString melt_;
    QProcess* proc_ = nullptr;
    std::unique_ptr<QTemporaryDir> tmp_;
    bool hasPending_ = false;
    nexo::Timeline pendingTl_;
    nexo::Frame pendingFrame_ = 0;
    nexo::Frame runningFrame_ = 0;
    QString pngPath_;
    QString log_;
};
