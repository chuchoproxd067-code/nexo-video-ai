# 0001 — Interfaz inicial con Qt Widgets en lugar de QML

**Estado:** propuesta, reversible.

El plan prefería Qt 6/QML. Para la primera compilación en Windows se usa Qt Widgets: el empaquetado con `windeployqt` es más simple (sin módulos QML), y la línea de tiempo personalizada se dibuja de forma natural en C++. Como no se puede compilar Qt en el entorno de desarrollo, se reduce el riesgo del primer intento en CI.

El modelo (`src/core`) es independiente de la interfaz, por lo que migrar a QML más adelante no afecta al núcleo.
