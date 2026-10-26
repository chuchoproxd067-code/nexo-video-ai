# Formato de proyecto (`.nexo`)

JSON UTF-8, esquema versionado. Versión actual: **1**. Implementación: `src/core/Project.cpp`.

```json
{
  "schema": 1,
  "app": "NEXO VIDEO AI",
  "name": "Mi proyecto",
  "fps": { "num": 30, "den": 1 },
  "media": ["C:\\videos\\a.mp4"],
  "tracks": [
    {
      "name": "V1",
      "kind": "video",
      "clips": [
        { "id": 1, "media": "C:\\videos\\a.mp4", "sourceIn": 10, "start": 0, "duration": 100 }
      ]
    }
  ]
}
```

- Todos los tiempos son **fotogramas enteros**. `sourceIn` = posición en el medio fuente; `start` = posición en la secuencia.
- Las rutas se guardan tal cual; los medios originales no se modifican. Hoy son absolutas; las rutas relativas y la relocalización de medios quedan pendientes.
- Reglas de validación al cargar: `schema` entre 1 y la versión soportada; fps > 0; enteros no negativos; duración > 0; ids de clip únicos; sin solapes en una pista; tipo de pista `video` o `audio`. Si algo falla se devuelve un error y no se carga nada.
- Un `schema` mayor que el soportado se rechaza con un mensaje explícito.
- Guardado: se escribe `archivo.nexo.tmp` y luego se renombra, para no dejar el proyecto anterior a medias.
- Migraciones: aún no hay ninguna (solo existe la versión 1). El punto de extensión está marcado en `fromJson`.
