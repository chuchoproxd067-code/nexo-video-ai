#pragma once
// Utilidades para localizar y lanzar el `melt.exe` empaquetado junto a la aplicación.
#include <QProcess>
#include <QString>

namespace meltrunner {

// Ruta de melt junto al ejecutable ("" si esta compilación no lo incluye).
QString meltPath();

// Prepara el proceso: PATH con la carpeta de melt (para que MLT cargue sus DLL).
void prepare(QProcess& p, const QString& melt);

// Quita de un log de melt los avisos de plugins de audio, que no son errores.
QString lastUsefulLines(const QString& log, int count);

}  // namespace meltrunner
