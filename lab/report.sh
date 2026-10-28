#!/bin/bash
# Uso: report.sh NN  -> emite los resultados de evidence/results_NN.txt como anotaciones (máx. 10 por paso).
f="evidence/results_$1.txt"
[ -f "$f" ] || { echo "::warning title=LAB $1::sin resultados"; exit 0; }
rm -f evidence/chunk_$1_*
split -b 3000 -d "$f" evidence/chunk_$1_
i=0
for c in evidence/chunk_$1_*; do
  i=$((i+1)); [ $i -gt 10 ] && break
  msg=$(sed 's/%/%25/g' "$c" | awk '{printf "%s%%0A", $0}')
  echo "::notice title=LAB $1 parte $(printf %02d $i)::$msg"
done
echo "resultados $1: $(wc -c < "$f") bytes en $i trozos"
