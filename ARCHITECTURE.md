# Arquitectura actual

```
src/core   Modelo del proyecto (C++17 puro, sin Qt ni MLT): Timecode, Timeline, History
src/app    Interfaz Qt Widgets: MainWindow, TimelineWidget, Theme
tests      Pruebas del núcleo
```

Reglas:
- `core` no depende de nada externo. Todo cambio de la línea de tiempo pasa por `History::apply`, de modo que el usuario y, en la Fase 5, el asistente de IA usan el mismo mecanismo reversible.
- Las posiciones se expresan en fotogramas enteros. `sourceIn` es la posición en el medio fuente; `timelineStart`, la posición en la secuencia.
- El motor multimedia (MLT, pendiente de validar) irá detrás de un adaptador que será el único código que lo conozca.
