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
for variant in "freeze frame=30 freeze_before=1 freeze_after=1" "freeze frame=30 freeze_before=1"; do
  n="freeze_$(echo $variant | tr ' =' '__')"
  if render $E/clipA.mp4 $E/$n.mp4 out=59 -filter $variant; then
    a=$(yframe $E/$n.mp4 0); b=$(yframe $E/$n.mp4 59)
    note "congelar ($variant)" "frames=$(nframes $E/$n.mp4) YAVG f0=$a f59=$b"
  else note "congelar ($variant)" "FALLÓ"; fi
done


# XML: ¿qué fotograma de la fuente sale con velocidad 2, y con inversa, cuando `in` no es 0?
mkxml() { # nombre resource in out
cat > $E/$1.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="c1"><property name="mlt_service">timewarp</property><property name="resource">$2</property></producer>
  <playlist id="pl"><entry producer="c1" in="$3" out="$4"/></playlist>
  <tractor id="main" in="0" out="$(( $4 - $3 ))"><multitrack><track producer="pl"/></multitrack></tractor>
</mlt>
XML
}
psnr_vs() { ffmpeg -hide_banner -i "$1" -i "$2" -lavfi psnr -f null - 2>&1 | grep -o 'average:[0-9.a-z]*' | cut -d: -f2; }
for k in 60 89 90 119; do ffmpeg -hide_banner -v error -i $E/clipA.mp4 -vf "select=eq(n\,$k)" -frames:v 1 -y $E/s$k.png; done
mkxml sp2 "2:$(pwd -W)/$E/clipA.mp4" 20 39
render $E/sp2.mlt $E/sp2.mp4
ffmpeg -hide_banner -v error -i $E/sp2.mp4 -vf "select=eq(n\,0)" -frames:v 1 -y $E/sp2_0.png
note "XML velocidad 2, in=20" "frames=$(nframes $E/sp2.mp4) psnr con fuente60=$(psnr_vs $E/sp2_0.png $E/s60.png) (si in es de salida: fuente 40, comparar src40)"
ffmpeg -hide_banner -v error -i $E/clipA.mp4 -vf "select=eq(n\,40)" -frames:v 1 -y $E/s40.png
note "XML velocidad 2, in=20 vs fuente40" "$(psnr_vs $E/sp2_0.png $E/s40.png)"
mkxml rv "-1:$(pwd -W)/$E/clipA.mp4" 30 59
render $E/rv.mlt $E/rv.mp4
ffmpeg -hide_banner -v error -i $E/rv.mp4 -vf "select=eq(n\,0)" -frames:v 1 -y $E/rv_0.png
note "XML inversa in=30" "frames=$(nframes $E/rv.mp4) psnr con fuente89=$(psnr_vs $E/rv_0.png $E/s89.png) con fuente90=$(psnr_vs $E/rv_0.png $E/s90.png) con fuente30=$(psnr_vs $E/rv_0.png $E/src_15.png)"
# audio de la velocidad: ¿se conserva el tono (pitch) en timewarp 2?
note "audio velocidad 2" "duración audio=$(ffprobe -v error -select_streams a:0 -show_entries stream=duration -of default=nw=1:nk=1 $E/speed_2.mp4) frecuencia aprox (zero crossings no medidos)"
