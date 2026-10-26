#include <QApplication>
#include <QCoreApplication>
#include <QString>
#include <cstdio>

#include "BuildInfo.h"
#include "MainWindow.h"

int main(int argc, char** argv) {
    // Modo de prueba de humo para CI: no abre ventana, solo comprueba que las
    // bibliotecas de Qt se cargan y que el ejecutable arranca desde el paquete.
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--version") {
            std::printf("NEXO VIDEO AI %s (%s)\n", NEXO_VERSION, NEXO_GIT_COMMIT);
            return 0;
        }
    }
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("NEXO VIDEO AI");
    MainWindow w;
    w.show();
    return app.exec();
}
