source lab/common.sh
mkclips
E=evidence
W="$(pwd -W)"
mkx() { # nombre  productor_fg_xml  [entry_in entry_out]
cat > $E/$1.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="bg" in="0" out="59"><property name="resource">$W/$E/clipB.mp4</property></producer>
  $2
  <playlist id="p0"><entry producer="bg" in="0" out="59"/></playlist>
  <playlist id="p1"><entry producer="fg" in="${3:-0}" out="${4:-59}"/></playlist>
  <tractor id="main" in="0" out="59">
    <multitrack><track producer="p0"/><track producer="p1"/></multitrack>
    <transition><property name="mlt_service">composite</property><property name="a_track">0</property><property name="b_track">1</property><property name="always_active">1</property><property name="fill">1</property><property name="distort">1</property></transition>
  </tractor>
</mlt>
XML
}
fgA() { echo "<producer id=\"fg\"><property name=\"resource\">$W/$E/clipA.mp4</property>$1</producer>"; }
R="100:60:270:150"
note "ref A/B centro" "A=$(yregion $E/clipA.mp4 10 $R) B=$(yregion $E/clipB.mp4 10 $R)"
# --- A. opacidad con filtro affine sobre otra pista ---
for op in 100 50 25 0; do
  mkx op$op "$(fgA "<filter><property name=\"mlt_service\">affine</property><property name=\"transition.rect\">0/0:640x360:$op</property></filter>")"
  render $E/op$op.mlt $E/op$op.mp4
  note "opacidad $op sobre B" "centro=$(yregion $E/op$op.mp4 10 $R)"
done
# opacidad por brightness alpha
mkx opb "$(fgA "<filter><property name=\"mlt_service\">brightness</property><property name=\"alpha\">0.5</property></filter>")"
render $E/opb.mlt $E/opb.mp4; note "opacidad brightness alpha 0.5" "centro=$(yregion $E/opb.mp4 10 $R)"
# --- B. croma ---
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=0x00ff00:s=640x360:d=2:r=30,drawbox=x=220:y=100:w=200:h=160:color=red:t=fill" -c:v libx264 -pix_fmt yuv420p $E/fg_green.mp4
fgG() { echo "<producer id=\"fg\"><property name=\"resource\">$W/$E/fg_green.mp4</property>$1</producer>"; }
CR="60:60:5:5"
note "croma ref" "B esquina=$(yregion $E/clipB.mp4 10 $CR) (verde puro luma~150)"
i=0
for f in \
 'chroma|<property name="key">#00ff00</property><property name="variance">0.3</property>' \
 'chroma|<property name="key">0x00ff00ff</property><property name="variance">0.3</property>' \
 'chroma|<property name="key">green</property><property name="variance">0.5</property>' \
 'chroma_hold|<property name="key">#00ff00</property><property name="variance">0.3</property>' \
 'avfilter.chromakey|<property name="av.color">0x00ff00</property><property name="av.similarity">0.3</property><property name="av.blend">0.1</property>' \
 'avfilter.chromakey|<property name="av.color">green</property><property name="av.similarity">0.3</property><property name="av.blend">0.1</property>' \
 'avfilter.colorkey|<property name="av.color">0x00ff00</property><property name="av.similarity">0.3</property><property name="av.blend">0.1</property>' \
 'avfilter.hsvkey|<property name="av.hue">120</property><property name="av.sat">1</property><property name="av.val">1</property><property name="av.similarity">0.3</property>' \
 'frei0r.bluescreen0r|<property name="0">0 1 0</property><property name="1">0.3</property>' \
 'frei0r.select0r|<property name="0">0 1 0</property>' \
 'avfilter.despill|<property name="av.type">green</property>' ; do
  svc="${f%%|*}"; props="${f#*|}"; i=$((i+1))
  mkx ck$i "$(fgG "<filter><property name=\"mlt_service\">$svc</property>$props</filter>")"
  if render $E/ck$i.mlt $E/ck$i.mp4; then
    note "croma $i [$svc ${props:0:90}]" "esquina=$(yregion $E/ck$i.mp4 10 $CR) centro rojo=$(yregion $E/ck$i.mp4 10 60:60:290:150)"
  else note "croma $i [$svc]" "FALLÓ"; fi
done
# --- C. PNG con alfa sobre fondo negro y sobre video ---
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=black@0.0:s=640x360,format=rgba,drawbox=x=100:y=100:w=200:h=100:color=red@1:t=fill,drawbox=x=400:y=100:w=100:h=100:color=white@0.5:t=fill" -frames:v 1 $E/ov.png
mkx ovpng "<producer id=\"fg\"><property name=\"mlt_service\">qimage</property><property name=\"resource\">$W/$E/ov.png</property><property name=\"length\">60</property></producer>"
render $E/ovpng.mlt $E/ovpng.mp4
note "PNG alfa sobre B" "rojo opaco=$(yregion $E/ovpng.mp4 10 60:40:130:120) B ahí=$(yregion $E/clipB.mp4 10 60:40:130:120) | blanco 50%=$(yregion $E/ovpng.mp4 10 40:40:430:130) B ahí=$(yregion $E/clipB.mp4 10 40:40:430:130) | fuera=$(yregion $E/ovpng.mp4 10 60:40:450:250) B=$(yregion $E/clipB.mp4 10 60:40:450:250)"
mkx ovpng2 "<producer id=\"fg\"><property name=\"mlt_service\">pixbuf</property><property name=\"resource\">$W/$E/ov.png</property><property name=\"length\">60</property></producer>"
render $E/ovpng2.mlt $E/ovpng2.mp4
note "PNG alfa (pixbuf)" "rojo opaco=$(yregion $E/ovpng2.mp4 10 60:40:130:120) fuera=$(yregion $E/ovpng2.mp4 10 60:40:450:250)"
# --- G. keyframes con entry in != 0: ¿posiciones absolutas o relativas al inicio del clip? ---
# clip con in=30..59; animación: tamaño completo en la posición 30 -> mitad en la 59 (absolutas) o 0 -> 29 (relativas)
for mode in abs rel; do
  if [ $mode = abs ]; then a="30=0/0:640x360:100;59=320/180:320x180:100"; else a="0=0/0:640x360:100;29=320/180:320x180:100"; fi
  mkx kf_$mode "$(fgA "<filter><property name=\"mlt_service\">affine</property><property name=\"transition.rect\">$a</property></filter>")" 30 59
  render $E/kf_$mode.mlt $E/kf_$mode.mp4
  note "keyframes $mode (entry in=30)" "esquina sup-izq f0=$(yregion $E/kf_$mode.mp4 0 40:30:5:5) f25=$(yregion $E/kf_$mode.mp4 25 40:30:5:5) f28=$(yregion $E/kf_$mode.mp4 28 40:30:5:5) (negro=16 = ya se encogió; si sigue ~A no animó)"
done
# --- H. orden de filtros: crop (máscara) + affine escala ---
mkx crop_aff "$(fgA '<filter><property name="mlt_service">crop</property><property name="left">160</property><property name="right">160</property></filter><filter><property name="mlt_service">affine</property><property name="transition.rect">0/0:640x360:100</property></filter>')"
render $E/crop_aff.mlt $E/crop_aff.mp4
note "crop con pista inferior" "borde izq (debería mostrar B si crop deja alfa 0)=$(yregion $E/crop_aff.mp4 10 60:60:20:150) B ahí=$(yregion $E/clipB.mp4 10 60:60:20:150) centro=$(yregion $E/crop_aff.mp4 10 60:60:290:150)"
