source lab/common.sh
mkclips
E=evidence
W="$(pwd -W)"
mkxml2() { # nombre pista0 pista1 extra_producer1 transicion
cat > $E/$1.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="bg" in="0" out="59"><property name="resource">$W/$E/clipA.mp4</property></producer>
  $2
  <playlist id="p0"><entry producer="bg" in="0" out="59"/></playlist>
  <playlist id="p1"><entry producer="fg" in="0" out="59"/></playlist>
  <tractor id="main" in="0" out="59">
    <multitrack><track producer="p0"/><track producer="p1"/></multitrack>
    <transition><property name="mlt_service">composite</property><property name="a_track">0</property><property name="b_track">1</property><property name="always_active">1</property><property name="fill">1</property><property name="distort">1</property></transition>
  </tractor>
</mlt>
XML
}
# --- croma ---
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=0x00ff00:s=640x360:d=2:r=30,drawbox=x=220:y=100:w=200:h=160:color=red:t=fill" -c:v libx264 -pix_fmt yuv420p $E/fg_green.mp4
bgc=$(yregion $E/clipA.mp4 10 60:60:5:5)
for variant in "none||" "chroma|<filter><property name=\"mlt_service\">chroma</property><property name=\"key\">0x00ff00</property><property name=\"variance\">0.3</property></filter>" "avchroma|<filter><property name=\"mlt_service\">avfilter.chromakey</property><property name=\"av.color\">0x00ff00</property><property name=\"av.similarity\">0.3</property><property name=\"av.blend\">0.1</property></filter>" "bluescreen|<filter><property name=\"mlt_service\">frei0r.bluescreen0r</property><property name=\"0\">0x00ff00</property></filter>"; do
  n="${variant%%|*}"; f="${variant#*|}"
  mkxml2 croma_$n "<producer id=\"fg\" in=\"0\" out=\"59\"><property name=\"resource\">$W/$E/fg_green.mp4</property>$f</producer>"
  if render $E/croma_$n.mlt $E/croma_$n.mp4; then
    note "croma [$n]" "esquina=$(yregion $E/croma_$n.mp4 10 60:60:5:5) (fondo $bgc; sin croma sería verde) centro rojo=$(yregion $E/croma_$n.mp4 10 60:60:290:150)"
  else note "croma [$n]" "FALLÓ $(grep -i -E 'fail|error' $E/croma_$n.log | head -2 | tr '\n' ' ')"; fi
done
# --- superposición PNG con alfa (así irán los títulos) ---
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=black@0.0:s=640x360,format=rgba,drawbox=x=100:y=100:w=200:h=100:color=red@1:t=fill" -frames:v 1 $E/ov.png
mkxml2 overlay "<producer id=\"fg\" in=\"0\" out=\"59\"><property name=\"mlt_service\">qimage</property><property name=\"resource\">$W/$E/ov.png</property><property name=\"length\">60</property></producer>"
if render $E/overlay.mlt $E/overlay.mp4; then
  note "overlay PNG alfa" "dentro del recuadro=$(yregion $E/overlay.mp4 10 60:40:130:120) fuera=$(yregion $E/overlay.mp4 10 60:40:450:250) fondo igual=$(yregion $E/clipA.mp4 10 60:40:450:250) (rojo puro ~81)"
else note "overlay PNG alfa" "FALLÓ"; fi
# --- texto: qtext y pango como productores ---
for prod in "qtext" "pango" "kdenlivetitle"; do
  melt -query producer=$prod 2>&1 | grep -E 'identifier' | sed 's/^ *- *identifier: //' | tr '\n' ' ' > $E/q_$prod.txt
  note "producer $prod props" "$(cut -c1-700 $E/q_$prod.txt)"
done
if render "qtext:" $E/qt.mp4 out=29 text="Hola NEXO" fgcolour=0xffffffff bgcolour=0x00000000 size=80 family=Arial; then note "qtext" "OK YAVG=$(yframe $E/qt.mp4 5) $(ffprobe -v error -show_entries stream=width,height -of csv=p=0 $E/qt.mp4 | head -1)"; else note "qtext" "FALLÓ $(tail -3 $E/qt.log | tr '\n' ' ')"; fi
mkxml2 qtext_over "<producer id=\"fg\" in=\"0\" out=\"59\"><property name=\"mlt_service\">qtext</property><property name=\"text\">Hola NEXO</property><property name=\"fgcolour\">0xffffffff</property><property name=\"bgcolour\">0x00000000</property><property name=\"size\">90</property><property name=\"family\">Arial</property><property name=\"geometry\">0/0:640x360:100</property></producer>"
if render $E/qtext_over.mlt $E/qtext_over.mp4; then note "qtext sobre video" "OK f10 centro=$(yregion $E/qtext_over.mp4 10 400:80:120:140) fondo=$(yregion $E/clipA.mp4 10 400:80:120:140) esquina=$(yregion $E/qtext_over.mp4 10 60:40:5:5) fondoesq=$(yregion $E/clipA.mp4 10 60:40:5:5)"; else note "qtext sobre video" "FALLÓ"; fi
# --- formatos de exportación (mismos argumentos que meltConsumerArgs) ---
fmt() { # nombre extension args...
  local n="$1" ext="$2"; shift 2
  melt $E/clipA.mp4 out=59 -consumer avformat:$E/exp_$n.$ext "$@" > $E/exp_$n.log 2>&1 || true
  if [ -s $E/exp_$n.$ext ]; then
    note "formato $n" "OK $(stat -c %s $E/exp_$n.$ext) bytes: $(ffprobe -v error -show_entries stream=codec_name,codec_type,width,height -of csv=p=0 $E/exp_$n.$ext | tr '\n' ' ')"
  else note "formato $n" "FALLÓ $(grep -v -i -E 'ladspa|lv2|vst' $E/exp_$n.log | tail -3 | tr '\n' ' ')"; fi
}
fmt mp4 mp4 vcodec=libx264 acodec=aac crf=23 preset=veryfast
fmt mov mov vcodec=libx264 acodec=aac crf=23 preset=veryfast
fmt webm webm vcodec=libvpx-vp9 acodec=libopus vb=0 cpu-used=5 crf=31
fmt gif gif vcodec=gif an=1
fmt mp3 mp3 vn=1 acodec=libmp3lame ab=192k
fmt wav wav vn=1 acodec=pcm_s16le
