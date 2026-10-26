# Compilación

## En la nube (camino normal)
Workflow: `.github/workflows/windows-build.yml`. Se lanza con *Run workflow*, en cada push a `main` y en cada pull request.

Pasos: instalar Qt 6.8 (MSVC 2022) → configurar con CMake/Ninja → compilar → pruebas (`ctest`) → empaquetar con `windeployqt` → ZIP → prueba de arranque del ZIP → subir artefacto. El artefacto solo se sube si todo lo anterior pasa.

## Límites de GitHub Actions (consultados en la documentación de GitHub)
- Repositorio público: ejecutores estándar gratuitos; Windows con 4 vCPU, 16 GB de RAM y 14 GB de disco.
- Repositorio privado (plan Free): 2.000 minutos al mes, 500 MB de artefactos, 2 vCPU y 8 GB de RAM.
- Máximo 6 horas por job. Artefactos: 90 días por defecto (aquí se configuran 30).
- El tiempo real de compilación y el tamaño del ZIP **aún no están medidos**.

## Local (opcional, solo para desarrolladores)
Núcleo y pruebas, sin Qt:
```
cmake -S . -B build -G Ninja -DNEXO_BUILD_APP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```
