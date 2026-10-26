#!/usr/bin/env bash
# Empaqueta NEXO VIDEO AI para Windows desde un shell MSYS2 (UCRT64).
# Uso: package_windows_msys2.sh <nexo-video-ai.exe> <carpeta_salida>
# Copia el ejecutable, melt, los plugins de Qt, los módulos y datos de MLT y todas
# las DLL de las que dependen (resueltas con ldd), para que el paquete funcione en
# un Windows sin MSYS2 ni herramientas de desarrollo.
set -euo pipefail

EXE="$1"
OUT="$2"
PREFIX="${MSYSTEM_PREFIX:?Ejecuta este script dentro de un shell MSYS2}"

rm -rf "$OUT"
mkdir -p "$OUT"
cp "$EXE" "$OUT/"
cp "$PREFIX/bin/melt.exe" "$OUT/"

# --- Plugins de Qt ---------------------------------------------------------
QTPLUGINS=""
for d in "$PREFIX/share/qt6/plugins" "$PREFIX/lib/qt6/plugins"; do
  if [ -d "$d" ]; then QTPLUGINS="$d"; break; fi
done
[ -n "$QTPLUGINS" ] || { echo "ERROR: no se encontraron los plugins de Qt en $PREFIX"; exit 1; }
for sub in platforms styles imageformats iconengines tls; do
  if [ -d "$QTPLUGINS/$sub" ]; then
    mkdir -p "$OUT/$sub"
    cp "$QTPLUGINS/$sub/"*.dll "$OUT/$sub/"
  fi
done
[ -f "$OUT/platforms/qwindows.dll" ] || { echo "ERROR: falta platforms/qwindows.dll"; exit 1; }

# --- Módulos y datos de MLT ------------------------------------------------
MLTMOD="$(find "$PREFIX/lib" -maxdepth 1 -type d -name 'mlt*' | head -n1)"
MLTDATA="$(find "$PREFIX/share" -maxdepth 1 -type d -name 'mlt*' | head -n1)"
[ -n "$MLTMOD" ]  || { echo "ERROR: no se encontraron los módulos de MLT"; exit 1; }
[ -n "$MLTDATA" ] || { echo "ERROR: no se encontraron los datos de MLT"; exit 1; }
mkdir -p "$OUT/lib" "$OUT/share"
cp -r "$MLTMOD"  "$OUT/lib/mlt"
cp -r "$MLTDATA" "$OUT/share/mlt"

# --- DLL de las que depende todo lo anterior -------------------------------
# OJO: se usan arreglos y `< /dev/null` porque ldd lee la entrada estándar y, dentro
# de un `while read` alimentado por find, se comía la lista de archivos tras el primero.
for pass in $(seq 1 12); do
  added=0
  mapfile -t files < <(find "$OUT" -type f \( -name '*.dll' -o -name '*.exe' \))
  for f in "${files[@]}"; do
    mapfile -t deps < <(ldd "$f" < /dev/null 2>/dev/null | awk '/=>/ {print $3}' | grep -i "^$PREFIX/" || true)
    for dep in "${deps[@]}"; do
      [ -n "$dep" ] || continue
      base="$(basename "$dep")"
      if [ ! -f "$OUT/$base" ]; then
        cp "$dep" "$OUT/"
        added=$((added + 1))
      fi
    done
  done
  echo "pasada $pass: $added DLL nuevas"
  [ "$added" -eq 0 ] && break
done

echo "Paquete listo en $OUT: $(find "$OUT" -type f | wc -l) archivos, $(du -sh "$OUT" | cut -f1)"
