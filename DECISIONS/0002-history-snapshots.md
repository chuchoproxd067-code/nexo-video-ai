# 0002 — Historial por instantáneas

**Estado:** aceptada para las fases 2–3.

`History` guarda una copia completa de la `Timeline` antes de cada operación. Es simple y evita errores de comandos inversos. El coste de memoria crece con el tamaño del proyecto; si se mide como problema, se sustituirá por comandos inversos o estructuras persistentes sin cambiar la interfaz pública de `History`.
