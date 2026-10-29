# Registro de cambios

## Fase 4 — Herramientas profesionales
- Propiedades por clip traducidas a MLT: velocidad, inversa, congelar, posición/escala/rotación/opacidad, fotogramas clave, recorte, volteo, color, 8 efectos, croma, estabilizar, audio (silenciar, ruido, normalizar, graves/agudos, tono).
- Clips de texto (Qt → PNG RGBA → melt) y subtítulos SRT.
- Pistas (añadir/quitar/renombrar, silenciar/ocultar/bloquear), marcadores, zoom, imán y regla de tiempo.
- 15 estilos de transición con patrones PGM propios (CC0) y selector en la app.
- Ajuste de aspecto (barras en vez de estirar), exportación a seis formatos con calidad y resolución.
- Verificación por renders reales en CI (`tools/ci_props_check.sh`) y laboratorio (`lab/`).
- Fundidos de video con `brightness alpha` (transparentes sobre pistas inferiores).

## Fases 1–3
Ver STATUS.md y el historial de git.
