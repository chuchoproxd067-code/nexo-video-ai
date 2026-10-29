#!/bin/bash
# Verifica con melt y ffmpeg REALES que cada propiedad de clip tiene efecto medible.
# Uso (desde la raíz del repo, en MSYS2): bash tools/ci_props_check.sh <props_case_tool> <dir_evidence>
# Escribe evidence/props_results.txt con líneas "OK|FALLA nombre: detalle". Siempre sale 0.
source lab/common.sh
TOOL="$1"; E="$2"; W="$(pwd -W)"
RES="$E/props_results.txt"; : > "$RES"
A="$W/$E/clipA.mp4"; B="$W/$E/clipB.mp4"
CR="40:30:5:5"; R="100:60:270:150"
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=0x00ff00:s=640x360:d=3:r=30,drawbox=x=220:y=100:w=200:h=160:color=red:t=fill" -c:v libx264 -pix_fmt yuv420p $E/p_green.mp4
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=black:s=640x360,format=rgba,geq=r='255':g='0':b='0':a='if(between(X,100,300)*between(Y,100,200),255,0)'" -frames:v 1 $E/p_text.png
G="$W/$E/p_green.mp4"; T="$W/$E/p_text.png"
ok() { echo "OK $1: $2" >> "$RES"; }
bad() { echo "FALLA $1: $2" >> "$RES"; }
cmp() { # nombre valor "expresión awk con v" detalle
  if awk -v v="$2" "BEGIN{exit !($3)}"; then ok "$1" "$4 [v=$2]"; else bad "$1" "$4 [v=$2 esperado: $3]"; fi; }
mk() { # caso -> $E/p_caso.mlt y .mp4 ; devuelve 0 si renderizó
  "$TOOL" "$1" $E/p_$1.mlt "$A" "$B" "${2:-$A}" || { bad "$1" "no se pudo generar el XML"; return 1; }
  render $E/p_$1.mlt $E/p_$1.mp4 || { bad "$1" "melt no produjo salida: $(grep -i -E 'fail|error' $E/p_$1.log | head -2 | tr '\n' ' ')"; return 1; }
}
png() { ffmpeg -hide_banner -v error -y -i "$1" -vf "select=eq(n\,$2)" -frames:v 1 "$3"; }
dur() { ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 "$1"; }
vmax() { ffmpeg -hide_banner -i "$1" -af volumedetect -vn -f null - 2>&1 | grep -o 'max_volume: [-0-9.]*' | cut -d' ' -f2; }
render $A $E/p_baseA.mp4 out=59; render $B $E/p_baseB.mp4 out=59

if mk speed2; then
  cmp speed2.frames "$(nframes $E/p_speed2.mp4)" "v==30" "velocidad x2 de 60 fotogramas de medio -> 30"
  png $E/clipA.mp4 0 $E/s0.png; png $E/p_speed2.mp4 0 $E/o0.png
  cmp speed2.f0 "$(psnr $E/s0.png $E/o0.png)" "v>22" "primer fotograma = fuente 0"
  png $E/clipA.mp4 30 $E/s30.png; png $E/p_speed2.mp4 15 $E/o15.png
  cmp speed2.f15 "$(psnr $E/s30.png $E/o15.png)" "v>22" "fotograma 15 de salida = fuente 30"
fi
mk speed05 && cmp speed05.frames "$(nframes $E/p_speed05.mp4)" "v==120" "velocidad 0.5 de 60 fotogramas de medio -> 120"
if mk reverse; then
  cmp reverse.frames "$(nframes $E/p_reverse.mp4)" "v==60" "60 fotogramas"
  png $E/clipA.mp4 59 $E/s59.png; png $E/p_reverse.mp4 0 $E/o0.png
  cmp reverse.f0 "$(psnr $E/s59.png $E/o0.png)" "v>22" "primer fotograma = último del tramo (fuente 59)"
  png $E/clipA.mp4 0 $E/s0.png
  cmp reverse.notforward "$(psnr $E/s0.png $E/o0.png)" "v<18" "no es el fotograma 0"
fi
if mk freeze; then
  cmp freeze.frames "$(nframes $E/p_freeze.mp4)" "v==60" "60 fotogramas"
  png $E/clipA.mp4 40 $E/s40.png; png $E/p_freeze.mp4 0 $E/o0.png; png $E/p_freeze.mp4 59 $E/o59.png
  cmp freeze.f0 "$(psnr $E/s40.png $E/o0.png)" "v>22" "congela el fotograma 40"
  cmp freeze.f59 "$(psnr $E/o0.png $E/o59.png)" "v>40" "último fotograma igual al primero"
fi
cornerB=$(yregion $E/clipB.mp4 10 $CR); cornerA=$(yregion $E/clipA.mp4 10 $CR)
note_() { :; }
echo "REF cornerA=$cornerA cornerB=$cornerB" >> "$RES"
mk scale50 && cmp scale50.corner "$(yregion $E/p_scale50.mp4 10 $CR)" "v>$cornerB-15" "escala 50%: el borde deja ver la pista inferior"
mk pos50 && cmp pos50.left "$(yregion $E/p_pos50.mp4 10 $CR)" "v>$cornerB-15" "desplazado 50%: la esquina izquierda muestra la pista inferior"
if mk scale60 && mk rot45; then
  S="10:10:134:76"
  cmp rot45.diff "$(psnr $E/p_scale60.mp4 $E/p_rot45.mp4)" "v<30" "rotar 45 cambia la imagen"
  cmp rot45.cut "$(yregion $E/p_rot45.mp4 10 $S)" "v>$cornerB-40" "la esquina del recuadro rotado deja ver la pista inferior"
fi
mk opacity50 && cmp opacity50.corner "$(yregion $E/p_opacity50.mp4 10 $CR)" "v>$cornerA+40 && v<$cornerB-40" "opacidad 0.5: mezcla de A y B"
if mk kf; then
  cmp kf.f0 "$(yregion $E/p_kf.mp4 1 $CR)" "v<$cornerA+30" "fotograma clave inicial: A opaco a 100%"
  cmp kf.f58 "$(yregion $E/p_kf.mp4 58 $CR)" "v>$cornerB-30" "fotograma clave final: pequeño y casi transparente"
fi
if mk crop; then
  cmp crop.left "$(yregion $E/p_crop.mp4 10 30:40:5:150)" "v<30" "recorte izquierdo 25%: la franja recortada queda en negro"
  cmp crop.keep "$(yregion $E/p_crop.mp4 10 40:40:300:150)" "v>($(yregion $E/p_baseA.mp4 10 40:40:380:150))-6 && v<($(yregion $E/p_baseA.mp4 10 40:40:380:150))+6" "la región conservada sigue visible"
fi
if mk flip; then
  ffmpeg -hide_banner -v error -y -i $E/p_baseA.mp4 -vf hflip $E/flipref.mp4
  png $E/flipref.mp4 10 $E/fr.png; png $E/p_flip.mp4 10 $E/fo.png
  cmp flip.psnr "$(psnr $E/fr.png $E/fo.png)" "v>22" "volteo horizontal = hflip de ffmpeg"
fi
yb=$(yframe $E/p_baseA.mp4 10)
if mk bright; then cmp bright.y "$(yframe $E/p_bright.mp4 10)" "v>$yb+15" "brillo +0.3 sube la luminancia"; fi
if mk contrast; then cmp contrast.psnr "$(psnr $E/p_baseA.mp4 $E/p_contrast.mp4)" "v<25" "contraste 1.8 cambia la imagen"; fi
if mk sat0; then
  sat=$(ffmpeg -hide_banner -v error -i $E/p_sat0.mp4 -vf "select=eq(n\,10),signalstats,metadata=print:key=lavfi.signalstats.SATAVG:file=-" -frames:v 1 -an -f null - 2>/dev/null | grep -o 'SATAVG=[0-9.]*' | head -1 | cut -d= -f2)
  cmp sat0.sat "$sat" "v<3" "saturación 0 = escala de grises"
fi
if mk temp; then cmp temp.psnr "$(psnr $E/p_baseA.mp4 $E/p_temp.mp4)" "v<25" "temperatura 3000 K cambia el color"; fi
for e in blur sharpen vignette mosaic grayscale sepia invert mirror; do
  if mk eff_$e; then cmp eff_$e.psnr "$(psnr $E/p_baseA.mp4 $E/p_eff_$e.mp4)" "v<40" "efecto $e modifica la imagen"; fi
done
if mk chroma "$G"; then
  cmp chroma.key "$(yregion $E/p_chroma.mp4 10 $CR)" "v>$cornerB-12 && v<$cornerB+12" "croma verde: el fondo se vuelve transparente (pista inferior visible)"
  cmp chroma.keep "$(yregion $E/p_chroma.mp4 10 60:40:290:160)" "v<$cornerB-60" "croma: el cuadro rojo se conserva"
fi
if mk mute; then cmp mute.vol "$(vmax $E/p_mute.mp4)" "v<-60" "silenciado"; fi
if mk trackmute; then cmp trackmute.vol "$(vmax $E/p_trackmute.mp4)" "v<-60" "pista silenciada"; fi
if mk audiofx; then cmp audiofx.vol "$(vmax $E/p_audiofx.mp4)" "v>-40" "denoise+normalize+graves/agudos no dejan el audio vacío"; cmp audiofx.frames "$(nframes $E/p_audiofx.mp4)" "v==60" "60 fotogramas"; fi
if mk pitch; then cmp pitch.vol "$(vmax $E/p_pitch.mp4)" "v>-40" "tono +7: audio presente"; fi
if mk stabilize; then cmp stabilize.frames "$(nframes $E/p_stabilize.mp4)" "v==60" "estabilizar renderiza sin romper (efecto no medido)"; fi
if mk text "$T"; then
  cmp text.inside "$(yregion $E/p_text.mp4 10 20:20:150:150)" "v<($(yregion $E/clipB.mp4 10 20:20:150:150))-30 || v>($(yregion $E/clipB.mp4 10 20:20:150:150))+30" "el texto/PNG cubre la pista inferior"
  cmp text.outside "$(yregion $E/p_text.mp4 10 40:30:5:5)" "v>$cornerB-12 && v<$cornerB+12" "fuera del texto se ve la pista inferior"
fi
# Transiciones con patrón: A(0..59) + B con 20 fotogramas de mezcla = 100 fotogramas; la zona de mezcla debe diferir del disolver.
"$TOOL" "xstyle:" $E/p_xd.mlt "$A" "$B" "$W/assets" && render $E/p_xd.mlt $E/p_xd.mp4
for sty in wipe-right wipe-down diagonal circle-open diamond clock blinds-h blinds-v bars-v checker spiral wipe-left wipe-up diagonal-inv circle-close; do
  n=x_$sty
  if "$TOOL" "xstyle:$sty" $E/p_$n.mlt "$A" "$B" "$W/assets" && render $E/p_$n.mlt $E/p_$n.mp4; then
    cmp xstyle.$sty.frames "$(nframes $E/p_$n.mp4)" "v==100" "transición $sty: duración correcta"
    png $E/p_xd.mp4 50 $E/xd.png; png $E/p_$n.mp4 50 $E/xs.png
    cmp xstyle.$sty.mid "$(psnr $E/xd.png $E/xs.png)" "v<25" "transición $sty: la zona de mezcla difiere del disolver"
  else bad xstyle.$sty "no renderizó: $(grep -i -E 'fail|error' $E/p_$n.log 2>/dev/null | head -2 | tr '\n' ' ')"; fi
done
# Ajuste de aspecto: video vertical 360x640 (franja blanca central) sobre la pista B.
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=red:s=360x640:d=3:r=30,drawbox=x=150:y=0:w=60:h=640:color=white:t=fill" -c:v libx264 -pix_fmt yuv420p $E/p_vert.mp4
V="$W/$E/p_vert.mp4"
if mk vertfit "$V"; then
  cmp vertfit.white "$(yregion $E/p_vertfit.mp4 10 10:100:318:130)" "v>200" "vertical: la franja blanca queda en el centro"
  cmp vertfit.bars "$(yregion $E/p_vertfit.mp4 10 20:100:20:130)" "v>140" "vertical: sin estirar (a los lados se ve la pista inferior, no el rojo del video)"
fi
if mk vertscale "$V"; then
  cmp vertscale.center "$(yregion $E/p_vertscale.mp4 10 6:60:317:150)" "v>200" "vertical al 50%: franja blanca central"
  cmp vertscale.side "$(yregion $E/p_vertscale.mp4 10 20:60:200:150)" "v>100" "vertical al 50%: el lateral muestra la pista inferior (imagen estrecha)"
fi
if mk hidden; then cmp hidden.psnr "$(psnr $E/p_baseB.mp4 $E/p_hidden.mp4)" "v>30" "pista oculta = solo la pista inferior"; fi
# Formatos de exportación: cada uno se renderiza con los argumentos reales de la app y se lee con ffprobe.
fmtcheck() { # ext calidad flujos_esperados(video,audio)
  out="$E/p_fmt_$1_$2.$1"
  mapfile -t args < <("$TOOL" consumer $1 $2 "$out")
  melt "$E/clipA.mp4" out=44 "${args[@]}" > "$E/p_fmt_$1_$2.log" 2>&1 < /dev/null
  info=$(ffprobe -v error -show_entries stream=codec_name,codec_type -of csv=p=0 "$out" 2>/dev/null | tr '\n' ' ')
  size=$(stat -c %s "$out" 2>/dev/null || echo 0)
  if [ "$size" -gt 1000 ] && echo "$info" | grep -q "$3"; then ok "format.$1.q$2" "$info ($size bytes)"; else bad "format.$1.q$2" "info=[$info] size=$size esperado: $3"; fi
}
fmtcheck mp4 0 h264; fmtcheck mp4 2 aac; fmtcheck mov 1 h264; fmtcheck webm 1 vp9; fmtcheck webm 1 opus
fmtcheck gif 1 gif; fmtcheck mp3 1 mp3; fmtcheck wav 1 pcm_s16le
cat "$RES"
fails=$(grep -c '^FALLA' "$RES")
echo "props: $(grep -c '^OK' "$RES") OK, $fails FALLA"
exit 0
