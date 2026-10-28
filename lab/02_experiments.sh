source lab/common.sh
mkclips
E=evidence

# ---- velocidad ----
for sp in 0.5 2 -1; do
  n="speed_$sp"
  if render "timewarp:$sp:$E/clipA.mp4" $E/$n.mp4; then
    note "velocidad $sp (línea de comandos)" "frames=$(nframes $E/$n.mp4) dur=$(ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 $E/$n.mp4)"
  else note "velocidad $sp" "FALLÓ: $(tail -3 $E/$n.log | tr '\n' ' ')"; fi
done
# XML con producer timewarp + entry in/out (así lo generará NEXO)
cat > $E/speed_xml.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="c1"><property name="mlt_service">timewarp</property><property name="resource">0.5:$(pwd -W)/$E/clipA.mp4</property></producer>
  <playlist id="pl"><entry producer="c1" in="30" out="89"/></playlist>
  <tractor id="main" in="0" out="59"><multitrack><track producer="pl"/></multitrack></tractor>
</mlt>
XML
if render $E/speed_xml.mlt $E/speed_xml.mp4; then note "velocidad XML 0.5 in=30 out=89" "frames=$(nframes $E/speed_xml.mp4); fuente utilizada=fotogramas 15..45 si 'in' es de salida; compara f0 con fuente"; else note "velocidad XML" "FALLÓ $(tail -3 $E/speed_xml.log | tr '\n' ' ')"; fi
# ¿qué fotograma de la fuente es el primero de la salida? PSNR contra fotogramas conocidos de clipA
for k in 0 15 30; do
  ffmpeg -hide_banner -v error -i $E/clipA.mp4 -vf "select=eq(n\,$k)" -frames:v 1 -y $E/src_$k.png
done
ffmpeg -hide_banner -v error -i $E/speed_xml.mp4 -vf "select=eq(n\,0)" -frames:v 1 -y $E/out0.png
for k in 0 15 30; do
  p=$(ffmpeg -hide_banner -i $E/out0.png -i $E/src_$k.png -lavfi psnr -f null - 2>&1 | grep -o 'average:[0-9.a-z]*'); echo "psnr out0 vs src$k = $p"
done | tee $E/psnr_speed.txt
note "velocidad: primer fotograma de la salida vs fuente" "$(tr '\n' '|' < $E/psnr_speed.txt)"

# ---- inversa ----
ffmpeg -hide_banner -v error -i $E/clipA.mp4 -vf "select=eq(n\,119)" -frames:v 1 -y $E/src_last.png
if [ -s $E/speed_-1.mp4 ]; then
  ffmpeg -hide_banner -v error -i $E/speed_-1.mp4 -vf "select=eq(n\,0)" -frames:v 1 -y $E/rev0.png
  note "inversa: PSNR primer fotograma salida vs último fuente" "$(ffmpeg -hide_banner -i $E/rev0.png -i $E/src_last.png -lavfi psnr -f null - 2>&1 | grep -o 'average:[0-9.a-z]*')"
fi

# ---- congelar ----
for variant in "freeze frame=30 freeze_after=1" "freeze frame=30 freeze_before=1 freeze_after=1"; do
  n="freeze_$(echo $variant | tr ' =' '__')"
  if render $E/clipA.mp4 $E/$n.mp4 out=59 -filter $variant; then
    a=$(yframe $E/$n.mp4 0); b=$(yframe $E/$n.mp4 59)
    note "congelar ($variant)" "frames=$(nframes $E/$n.mp4) YAVG f0=$a f59=$b"
  else note "congelar ($variant)" "FALLÓ"; fi
done

# ---- transformaciones (affine) ----
aff() { # nombre rect_extra...
  local n="$1"; shift
  render $E/clipA.mp4 $E/aff_$n.mp4 out=29 -filter affine "$@"
}
aff half transition.rect="0/0:320x180:100"
note "affine mitad sup-izq" "TL=$(yregion $E/aff_half.mp4 10 300:160:10:10) BR=$(yregion $E/aff_half.mp4 10 300:160:330:190) (TL debe ser ~fuente, BR ~16 negro)"
aff opac transition.rect="0/0:640x360:50"
note "affine opacidad 50" "centro=$(yregion $E/aff_opac.mp4 10 100:100:270:130) fuente=$(yregion $E/clipA.mp4 10 100:100:270:130)"
aff rot transition.rect="0/0:640x360:100" transition.fix_rotate_x=30
note "affine rotación 30" "esquina=$(yregion $E/aff_rot.mp4 10 40:40:0:0) (negro si rotó) frames=$(nframes $E/aff_rot.mp4)"
aff zoomcrop transition.rect="-160/0:960x540:100"
note "affine recorte por zoom" "ok=$([ -s $E/aff_zoomcrop.mp4 ] && echo si) centro=$(yregion $E/aff_zoomcrop.mp4 10 100:100:270:130)"
aff anim transition.rect="0=0/0:640x360:100;29=160/90:320x180:0"
note "affine animado (frames)" "f0=$(yframe $E/aff_anim.mp4 0) f28=$(yframe $E/aff_anim.mp4 28)"

# ---- recorte (crop) y volteo ----
for v in "crop left=100 right=100" "crop top=60 bottom=60" ; do
  n="crop_$(echo $v | tr ' =' '__')"
  render $E/clipA.mp4 $E/$n.mp4 out=29 -filter $v
  note "$v" "ok=$([ -s $E/$n.mp4 ] && echo si) $(ffprobe -v error -show_entries stream=width,height -of csv=p=0 $E/$n.mp4 | head -1)"
done
for m in flip flop; do
  render $E/clipA.mp4 $E/mirror_$m.mp4 out=29 -filter mirror mirror=$m
  ffmpeg -hide_banner -v error -i $E/mirror_$m.mp4 -vf "select=eq(n\,5)" -frames:v 1 -y $E/mir_$m.png
  ffmpeg -hide_banner -v error -i $E/clipA.mp4 -vf "select=eq(n\,5),$( [ $m = flop ] && echo hflip || echo vflip )" -frames:v 1 -y $E/ref_$m.png
  note "mirror $m" "PSNR vs referencia = $(ffmpeg -hide_banner -i $E/mir_$m.png -i $E/ref_$m.png -lavfi psnr -f null - 2>&1 | grep -o 'average:[0-9.a-z]*')"
done

# ---- color ----
base=$(yframe $E/clipA.mp4 10)
for v in "avfilter.eq brightness=0.3" "avfilter.eq saturation=0" "brightness level=0.5" "lift_gamma_gain gain_r=1.5" "greyscale" "sepia" "invert" "avfilter.vignette" "avfilter.boxblur luma_radius=8" "avfilter.hue h=90" "avfilter.colorbalance rs=0.5" "frei0r.colorize"; do
  n="col_$(echo $v | tr ' =.' '___')"
  if render $E/clipA.mp4 $E/$n.mp4 out=29 -filter $v; then note "color [$v]" "OK YAVG f10=$(yframe $E/$n.mp4 10) (base $base)"; else note "color [$v]" "FALLÓ $(grep -i -E 'fail|error|not' $E/$n.log | head -2 | tr '\n' ' ')"; fi
done

# ---- audio ----
vd() { ffmpeg -hide_banner -i "$1" -af volumedetect -vn -f null - 2>&1 | grep -o 'mean_volume: [-0-9.]*' | cut -d' ' -f2; }
note "audio base" "mean=$(vd $E/clipA.mp4)"
for v in "avfilter.equalizer f=1000 g=15 w=1" "avfilter.bass g=15" "avfilter.treble g=15" "avfilter.afftdn" "avfilter.loudnorm" "avfilter.highpass f=500" "avfilter.lowpass f=500" "avfilter.acompressor" "avfilter.areverse" "avfilter.aecho" "avfilter.atempo tempo=1.5"; do
  n="aud_$(echo $v | tr ' =.' '___')"
  if render $E/clipA.mp4 $E/$n.mp4 -filter $v; then note "audio [$v]" "OK mean=$(vd $E/$n.mp4) frames=$(nframes $E/$n.mp4)"; else note "audio [$v]" "FALLÓ $(grep -i -E 'fail|error|not' $E/$n.log | head -2 | tr '\n' ' ')"; fi
done
