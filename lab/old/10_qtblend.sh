source lab/common.sh
mkclips
E=evidence
W="$(pwd -W)"
# transición por clip sobre la pista inferior (B). $1=nombre $2=XML de la transición (sin a/b_track) $3=in $4=out de la transición
mkq() {
cat > $E/$1.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="bg" in="0" out="59"><property name="resource">$W/$E/clipB.mp4</property></producer>
  <producer id="fg" in="0" out="59"><property name="resource">$W/$E/clipA.mp4</property>$5</producer>
  <playlist id="p0"><entry producer="bg" in="0" out="59"/></playlist>
  <playlist id="p1"><entry producer="fg" in="0" out="59"/></playlist>
  <tractor id="main" in="0" out="59">
    <multitrack><track producer="p0"/><track producer="p1"/></multitrack>
    <transition in="${3:-0}" out="${4:-59}"><property name="a_track">0</property><property name="b_track">1</property>$2</transition>
  </tractor>
</mlt>
XML
}
R="100:60:270:150"; CR="50:40:5:5"
note "ref" "A centro=$(yregion $E/clipA.mp4 10 $R) B centro=$(yregion $E/clipB.mp4 10 $R) A esq=$(yregion $E/clipA.mp4 10 $CR) B esq=$(yregion $E/clipB.mp4 10 $CR)"
q() { # nombre props [in out]
  local n="$1"; mkq $n "$2" ${3:-0} ${4:-59}
  if render $E/$n.mlt $E/$n.mp4; then note "$n" "centro=$(yregion $E/$n.mp4 10 $R) esq=$(yregion $E/$n.mp4 10 $CR) f40 centro=$(yregion $E/$n.mp4 40 $R) esq40=$(yregion $E/$n.mp4 40 $CR)"; else note "$n" "FALLÓ $(grep -i -E 'fail|error' $E/$n.log | head -2 | tr '\n' ' ')"; fi
}
Q='<property name="mlt_service">qtblend</property>'
q qt_full "$Q<property name=\"rect\">0/0:640x360:100</property>"
q qt_op50 "$Q<property name=\"rect\">0/0:640x360:50</property>"
q qt_op25 "$Q<property name=\"rect\">0/0:640x360:25</property>"
q qt_scale "$Q<property name=\"rect\">160/90:320x180:100</property>"
q qt_rot "$Q<property name=\"rect\">0/0:640x360:100</property><property name=\"rotation\">30</property>"
q qt_anim "$Q<property name=\"rect\">0=0/0:640x360:100;59=0/0:640x360:0</property>"
q qt_anim_off "$Q<property name=\"rect\">0=0/0:640x360:100;29=0/0:640x360:0</property>" 20 49
q qt_dist "$Q<property name=\"rect\">0/0:320x360:100</property><property name=\"distort\">1</property>"
q qt_nodist "$Q<property name=\"rect\">0/0:320x360:100</property>"
q qt_comp "$Q<property name=\"rect\">0/0:640x360:100</property><property name=\"compositing\">1</property>"
A='<property name="mlt_service">affine</property>'
q af_op50 "$A<property name=\"geometry\">0/0:640x360:50</property>"
q af_scale "$A<property name=\"geometry\">160/90:320x180:100</property>"
q af_rot "$A<property name=\"geometry\">0/0:640x360:100</property><property name=\"fix_rotate_z\">30</property>"
q comp_op50 '<property name="mlt_service">composite</property><property name="geometry">0/0:100%x100%:50</property><property name="fill">1</property><property name="distort">1</property>'
q comp_scale '<property name="mlt_service">composite</property><property name="geometry">25%/25%:50%x50%:100</property><property name="fill">1</property><property name="distort">1</property>'
# fg con filtros previos: crop + chroma + efectos con qtblend
q qt_filters "$Q<property name=\"rect\">0/0:640x360:100</property>" 0 59 '<filter><property name="mlt_service">crop</property><property name="left">160</property><property name="right">160</property></filter>'
# --- alfa correcto del PNG (geq) ---
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=black:s=640x360,format=rgba,geq=r='255':g='0':b='0':a='if(between(X,100,300)*between(Y,100,200),255,if(between(X,400,500)*between(Y,100,200),128,0))'" -frames:v 1 $E/ov2.png
note "ov2.png píxeles (rgba)" "rojo=$(ffmpeg -hide_banner -v error -i $E/ov2.png -vf crop=1:1:150:150 -f rawvideo -pix_fmt rgba - | od -An -tu1 | tr -s ' ') semi=$(ffmpeg -hide_banner -v error -i $E/ov2.png -vf crop=1:1:450:150 -f rawvideo -pix_fmt rgba - | od -An -tu1 | tr -s ' ') fuera=$(ffmpeg -hide_banner -v error -i $E/ov2.png -vf crop=1:1:10:10 -f rawvideo -pix_fmt rgba - | od -An -tu1 | tr -s ' ')"
cat > $E/ovq.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="bg" in="0" out="59"><property name="resource">$W/$E/clipB.mp4</property></producer>
  <producer id="fg" in="0" out="59"><property name="mlt_service">qimage</property><property name="resource">$W/$E/ov2.png</property><property name="length">60</property></producer>
  <playlist id="p0"><entry producer="bg" in="0" out="59"/></playlist>
  <playlist id="p1"><entry producer="fg" in="0" out="59"/></playlist>
  <tractor id="main" in="0" out="59">
    <multitrack><track producer="p0"/><track producer="p1"/></multitrack>
    <transition><property name="mlt_service">TRANS</property><property name="a_track">0</property><property name="b_track">1</property>EXTRA</transition>
  </tractor>
</mlt>
XML
for v in 'composite|<property name="always_active">1</property><property name="fill">1</property><property name="distort">1</property>' 'qtblend|<property name="rect">0/0:640x360:100</property>' 'affine|<property name="geometry">0/0:640x360:100</property>' 'frei0r.cairoblend|'; do
  svc="${v%%|*}"; ex="${v#*|}"; n="ovq_${svc//./_}"
  sed "s#TRANS#$svc#; s#EXTRA#$ex#" $E/ovq.mlt > $E/$n.mlt
  render $E/$n.mlt $E/$n.mp4
  note "PNG alfa con $svc" "rojo=$(yregion $E/$n.mp4 10 60:40:130:120) (B ahí=$(yregion $E/clipB.mp4 10 60:40:130:120)) semi=$(yregion $E/$n.mp4 10 40:40:430:130) (B ahí=$(yregion $E/clipB.mp4 10 40:40:430:130)) fuera=$(yregion $E/$n.mp4 10 60:40:450:250) (B=$(yregion $E/clipB.mp4 10 60:40:450:250))"
done
note "fin" "script 10 completo"
