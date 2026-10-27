#include "MeltRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringList>

namespace meltrunner {

QString meltPath() {
#ifdef Q_OS_WIN
    const QString name = "melt.exe";
#else
    const QString name = "melt";
#endif
    const QString p = QCoreApplication::applicationDirPath() + "/" + name;
    return QFileInfo::exists(p) ? p : QString();
}

void prepare(QProcess& p, const QString& melt) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString dir = QFileInfo(melt).absolutePath();
    env.insert("PATH", QDir::toNativeSeparators(dir) + QDir::listSeparator() + env.value("PATH"));
    p.setProcessEnvironment(env);
}

QString lastUsefulLines(const QString& log, int count) {
    QStringList keep;
    const QStringList lines = QString(log).replace('\r', '\n').split('\n', Qt::SkipEmptyParts);
    for (const QString& l : lines) {
        if (l.startsWith("No LADSPA") || l.startsWith("No LV2") || l.startsWith("No VST")) continue;
        if (l.startsWith("Current Frame") || l.startsWith("Current Position")) continue;
        keep << l;
    }
    return keep.mid(qMax(0, int(keep.size()) - count)).join('\n');
}

}  // namespace meltrunner
