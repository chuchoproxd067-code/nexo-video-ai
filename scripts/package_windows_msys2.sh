#!/usr/bin/env bash
# Empaqueta NEXO VIDEO AI para Windows desde un shell MSYS2 (UCRT64).
# Uso: package_windows_msys2.sh <nexo-video-ai.exe> <carpeta_salida>
#
# Distribución (la que espera MLT, que es reubicable respecto a bin/):
#   <salida>/bin/        nexo-video-ai.exe, melt.exe, todas las DLL y los plugins de Qt
#   <salida>/lib/mlt/    módulos de MLT
#   <salida>/share/mlt/  perfiles y datos de MLT
#   <salida>/Iniciar NEXO VIDEO AI.cmd
# Funciona en un Windows sin MSYS2 ni herramientas de desarrollo.
set -euo pipefail

EXE="$1"
OUT="$2"
PREFIX="${MSYSTEM_PREFIX:?Ejecuta este script dentro de un shell MSYS2}"
BIN="$OUT/bin"

rm -rf "$OUT"
mkdir -p "$BIN"
cp "$EXE" "$BIN/"
cp "$PREFIX/bin/melt.exe" "$BIN/"

# --- Plugins de Qt (junto al ejecutable, donde Qt los busca) ----------------
QTPLUGINS=""
for d in "$PREFIX/share/qt6/plugins" "$PREFIX/lib/qt6/plugins"; do
  if [ -d "$d" ]; then QTPLUGINS="$d"; break; fi
done
[ -n "$QTPLUGINS" ] || { echo "ERROR: no se encontraron los plugins de Qt en $PREFIX"; exit 1; }
for sub in platforms styles imageformats iconengines tls; do
  if [ -d "$QTPLUGINS/$sub" ]; then
    mkdir -p "$BIN/$sub"
    cp "$QTPLUGINS/$sub/"*.dll "$BIN/$sub/"
  fi
done
[ -f "$BIN/platforms/qwindows.dll" ] || { echo "ERROR: falta platforms/qwindows.dll"; exit 1; }

# --- Módulos y datos de MLT -------------------------------------------------
MLTMOD="$PREFIX/lib/mlt"
MLTDATA="$PREFIX/share/mlt"
[ -d "$MLTMOD" ]  || { echo "ERROR: no existe $MLTMOD"; exit 1; }
[ -d "$MLTDATA" ] || { echo "ERROR: no existe $MLTDATA"; exit 1; }
mkdir -p "$OUT/lib" "$OUT/share"
cp -r "$MLTMOD"  "$OUT/lib/mlt"
cp -r "$MLTDATA" "$OUT/share/mlt"

# --- recursos de ejemplo (sonidos y música CC0 generados por tools/gen_assets.py) ---
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [ -d "$REPO_ROOT/assets" ]; then cp -r "$REPO_ROOT/assets" "$OUT/assets"; fi

# --- DLL de las que depende todo lo anterior --------------------------------
# Se leen las importaciones con objdump y se copia del prefijo de MSYS2 lo que exista
# allí. No se usa ldd: resolvía mal las DLL de los módulos de MLT. Las DLL del sistema
# (kernel32, api-ms-win-*, ...) no están en el prefijo y se ignoran.
declare -A AVAILABLE
for f in "$PREFIX"/bin/*.dll; do
  AVAILABLE["$(basename "$f" | tr 'A-Z' 'a-z')"]="$f"
done

for pass in $(seq 1 15); do
  added=0
  mapfile -t files < <(find "$OUT" -type f \( -name '*.dll' -o -name '*.exe' \))
  for f in "${files[@]}"; do
    mapfile -t names < <(objdump -p "$f" 2>/dev/null | awk '/DLL Name:/ {print $3}')
    for name in "${names[@]}"; do
      [ -n "$name" ] || continue
      key="$(echo "$name" | tr 'A-Z' 'a-z')"
      src="${AVAILABLE[$key]:-}"
      [ -n "$src" ] || continue
      if [ ! -f "$BIN/$(basename "$src")" ]; then
        cp "$src" "$BIN/"
        added=$((added + 1))
      fi
    done
  done
  echo "pasada $pass: $added DLL nuevas"
  [ "$added" -eq 0 ] && break
done

# Módulos de MLT cuyas dependencias no se pudieron resolver: se omiten con aviso, para
# que un módulo opcional roto no impida que MLT cargue los demás.
in_pkg_or_system() {
  local n="$1" low
  low="$(echo "$n" | tr 'A-Z' 'a-z')"
  case "$low" in api-ms-win-*|ext-ms-win-*) return 0;; esac
  if ls "$BIN" | grep -qix -- "$n"; then return 0; fi
  if ls /c/Windows/System32 2>/dev/null | grep -qix -- "$n"; then return 0; fi
  return 1
}
for m in "$OUT"/lib/mlt/*.dll; do
  [ -f "$m" ] || continue
  for n in $(objdump -p "$m" 2>/dev/null | awk '/DLL Name:/ {print $3}'); do
    if ! in_pkg_or_system "$n"; then
      echo "OMITIDO módulo MLT $(basename "$m"): falta $n"
      rm -f "$m"
      break
    fi
  done
done
echo "Módulos de MLT incluidos: $(ls "$OUT"/lib/mlt/*.dll 2>/dev/null | wc -l)"

# --- Lanzador ----------------------------------------------------------------
printf '@echo off\r\nstart "" "%%~dp0bin\\nexo-video-ai.exe"\r\n' > "$OUT/Iniciar NEXO VIDEO AI.cmd"

echo "Paquete listo en $OUT: $(find "$OUT" -type f | wc -l) archivos, $(du -sh "$OUT" | cut -f1)"
