source lab/common.sh
mkclips
E=evidence

root="$(dirname "$(which melt)")/.."
note "pgm en la instalación" "$(find "$root" -iname '*.pgm' 2>/dev/null | head -20 | tr '\n' ' ') | total=$(find "$root" -iname '*.pgm' 2>/dev/null | wc -l)"
note "carpetas share/mlt" "$(ls "$root/share/mlt" | tr '\n' ' ')"
# ---- transiciones luma con patrón y frei0r sleid0r ----
mk_trans_xml() { # salida servicio_y_props...
  local out="$1"; shift
  cat > $out <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="a" in="0" out="59"><property name="resource">$(pwd -W)/evidence/clipA.mp4</property></producer>
  <producer id="b" in="0" out="59"><property name="resource">$(pwd -W)/evidence/clipB.mp4</property></producer>
  <playlist id="pa"><entry producer="a" in="0" out="59"/></playlist>
  <playlist id="pb"><entry producer="b" in="0" out="59"/></playlist>
  <tractor id="main" in="0" out="59">
    <multitrack><track producer="pa"/><track producer="pb"/></multitrack>
    <transition in="0" out="59">$@<property name="a_track">0</property><property name="b_track">1</property></transition>
  </tractor>
</mlt>
XML
}
trans() { # nombre props
  local n="$1"; shift
  mk_trans_xml $E/tr_$n.mlt "$@"
  if render $E/tr_$n.mlt $E/tr_$n.mp4; then
    note "transición $n" "OK frames=$(nframes $E/tr_$n.mp4) mitad izq f30=$(yregion $E/tr_$n.mp4 30 160:300:10:30) der=$(yregion $E/tr_$n.mp4 30 160:300:470:30) | f5 izq=$(yregion $E/tr_$n.mp4 5 160:300:10:30) der=$(yregion $E/tr_$n.mp4 5 160:300:470:30) | f55 izq=$(yregion $E/tr_$n.mp4 55 160:300:10:30) der=$(yregion $E/tr_$n.mp4 55 160:300:470:30)"
  else note "transición $n" "FALLÓ $(grep -i -E 'fail|error|not' $E/tr_$n.log | head -2 | tr '\n' ' ')"; fi
}
note "referencia A/B" "A izq=$(yregion $E/clipA.mp4 10 160:300:10:30) A der=$(yregion $E/clipA.mp4 10 160:300:470:30) B izq=$(yregion $E/clipB.mp4 10 160:300:10:30) B der=$(yregion $E/clipB.mp4 10 160:300:470:30)"
trans dissolve '<property name="mlt_service">luma</property>'
# patrones PGM propios (gris 8 bits): rampa horizontal/vertical, radial, diagonal
mkpgm() { ffmpeg -hide_banner -v error -y -f lavfi -i "nullsrc=s=640x360,geq=lum='$2':cb=128:cr=128,format=gray" -frames:v 1 "$E/$1.pgm"; }
mkpgm ramp_h "X/W*255"
mkpgm ramp_v "Y/H*255"
mkpgm radial "min(255,hypot(X-W/2,Y-H/2)/hypot(W/2,H/2)*255)"
mkpgm diag "(X/W+Y/H)/2*255"
mkpgm bars "mod(floor(X/40),2)*128+X/W*127"
note "pgm propios" "$(ls -l $E/*.pgm | awk '{print $5, $9}' | tr '\n' ' ') cabecera: $(head -c 15 $E/ramp_h.pgm | tr '\n' ' ')"
for lm in ramp_h ramp_v radial diag bars; do
  trans "luma_$lm" '<property name="mlt_service">luma</property><property name="resource">'"$(pwd -W)/$E/$lm.pgm"'</property><property name="softness">0.1</property>'
done
for sl in wipe-left wipe-right wipe-down wipe-circle wipe-barn-door-h slide-left push-left; do
  trans "sleid_$sl" '<property name="mlt_service">frei0r.sleid0r_'"$sl"'</property>'
done
# trans "luma_abs" '<property name="mlt_service">luma</property><property name="resource">'"$(dirname "$(which melt)")"'/../share/mlt/lumas/PAL/luma05.pgm</property>'
