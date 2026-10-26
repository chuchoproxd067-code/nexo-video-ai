# 0003 — Cadena de herramientas: MSYS2 UCRT64 para todo

**Estado:** aceptada (decide el equipo técnico; reversible).

MLT y FFmpeg para Windows están disponibles como paquetes de MSYS2/MinGW, no para MSVC. Mezclar MSVC (interfaz) con MinGW (motor) obligaría a unir compiladores con ABIs de C++ distintos. Se compila todo con GCC en MSYS2 UCRT64 (MINGW64 está marcado como obsoleto por `setup-msys2`), con Qt 6 del mismo repositorio de paquetes. Es la ruta que usa Kdenlive en Windows (MinGW).

Consecuencias: el workflow MSVC original (`windows-build.yml`) se mantiene solo hasta que el nuevo (`windows-msys2.yml`) esté verificado; después se retira. El paquete se arma con `scripts/package_windows_msys2.sh` (resolviendo DLL con `ldd`) en lugar de `windeployqt`.
