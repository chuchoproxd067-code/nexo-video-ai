source lab/common.sh
mkclips
E=evidence
W="$(pwd -W)"
mkx() { # nombre  filtros_en_fg  [in out]  [servicio_fg_extra]
cat > $E/$1.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="bg" in="0" out="59"><property name="resource">$W/$E/clipB.mp4</property></producer>
  <producer id="fg"><property name="resource">$W/$E/clipA.mp4</property>$2</producer>
  <playlist id="p0"><entry producer="bg" in="0" out="59"/></playlist>
  <playlist id="p1"><entry producer="fg" in="${3:-0}" out="${4:-59}"/></playlist>
  <tractor id="main" in="0" out="59">
    <multitrack><track producer="p0"/><track producer="p1"/></multitrack>
    <transition><property name="mlt_service">composite</property><property name="a_track">0</property><property name="b_track">1</property><property name="always_active">1</property><property name="fill">1</property><property name="distort">1</property></transition>
  </tractor>
</mlt>
XML
}
R="100:60:270:150"; CR="50:40:5:5"
note "ref" "A centro=$(yregion $E/clipA.mp4 10 $R) B centro=$(yregion $E/clipB.mp4 10 $R) A esq=$(yregion $E/clipA.mp4 10 $CR) B esq=$(yregion $E/clipB.mp4 10 $CR)"
AF() { echo "<filter><property name=\"mlt_service\">affine</property>$1</filter>"; }
t() { local n="$1"; mkx $n "$2" ${3:-0} ${4:-59}
  if render $E/$n.mlt $E/$n.mp4; then note "$n" "f10 centro=$(yregion $E/$n.mp4 10 $R) esq=$(yregion $E/$n.mp4 10 $CR) | f50 centro=$(yregion $E/$n.mp4 50 $R) esq=$(yregion $E/$n.mp4 50 $CR)"; else note "$n" "FALLÓ"; fi; }
for op in 1 0.5 0.25 0; do t aff_op$op "$(AF "<property name=\"transition.rect\">0/0:640x360:$op</property>")"; done
t aff_op_anim "$(AF '<property name="transition.rect">0=0/0:640x360:1;59=0/0:640x360:0</property>')"
t aff_scale_op "$(AF '<property name="transition.rect">160/90:320x180:0.5</property>')"
# rotación animada y estática
t aff_rotz_static "$(AF '<property name="transition.rect">0/0:640x360:1</property><property name="transition.fix_rotate_z">25</property>')"
t aff_rotz_anim "$(AF '<property name="transition.rect">0/0:640x360:1</property><property name="transition.rotate_z">0=0;59=90</property>')"
t aff_rotz_anim_fix "$(AF '<property name="transition.rect">0/0:640x360:1</property><property name="transition.fix_rotate_z">0=0;59=90</property>')"
# rotación 2D con escala reducida para ver las 4 esquinas
t aff_rot_small "$(AF '<property name="transition.rect">180/100:280x160:1</property><property name="transition.fix_rotate_z">45</property>')"
# in != 0 con opacidad animada absoluta
t aff_anim_in30 "$(AF '<property name="transition.rect">30=0/0:640x360:1;59=0/0:640x360:0</property>')" 30 59
# posición del cuadro con rotación: ¿rota alrededor del centro del rect o del cuadro?
t aff_off_rot "$(AF '<property name="transition.rect">100/100:320x180:1</property><property name="transition.fix_rotate_z">90</property>')"
# crop + affine + chroma orden
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=0x00ff00:s=640x360:d=2:r=30,drawbox=x=220:y=100:w=200:h=160:color=red:t=fill" -c:v libx264 -pix_fmt yuv420p $E/fg_green.mp4
cat > $E/chk.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="bg" in="0" out="59"><property name="resource">$W/$E/clipB.mp4</property></producer>
  <producer id="fg"><property name="resource">$W/$E/fg_green.mp4</property>
    <filter><property name="mlt_service">chroma</property><property name="key">#00ff00</property><property name="variance">0.3</property></filter>
    <filter><property name="mlt_service">affine</property><property name="transition.rect">80/45:480x270:1</property></filter>
  </producer>
  <playlist id="p0"><entry producer="bg" in="0" out="59"/></playlist>
  <playlist id="p1"><entry producer="fg" in="0" out="59"/></playlist>
  <tractor id="main" in="0" out="59">
    <multitrack><track producer="p0"/><track producer="p1"/></multitrack>
    <transition><property name="mlt_service">composite</property><property name="a_track">0</property><property name="b_track">1</property><property name="always_active">1</property><property name="fill">1</property><property name="distort">1</property></transition>
  </tractor>
</mlt>
XML
render $E/chk.mlt $E/chk.mp4
note "chroma + affine" "esquina=$(yregion $E/chk.mp4 10 $CR) (B=180) cuadro verde interior (no rojo) ~ B: $(yregion $E/chk.mp4 10 40:30:120:70) B=$(yregion $E/clipB.mp4 10 40:30:120:70) rojo=$(yregion $E/chk.mp4 10 60:40:290:160)"
# congelar más allá del final del medio (clipA tiene 120 fotogramas)
for v in freeze_beyond hold_beyond; do
  if [ $v = freeze_beyond ]; then P="<producer id=\"c\"><property name=\"resource\">$W/$E/clipA.mp4</property><filter><property name=\"mlt_service\">freeze</property><property name=\"frame\">100</property><property name=\"freeze_before\">1</property><property name=\"freeze_after\">1</property></filter></producer>"; IN=100; OUT=159
  else P="<producer id=\"c\"><property name=\"mlt_service\">hold</property><property name=\"resource\">$W/$E/clipA.mp4</property><property name=\"frame\">100</property></producer>"; IN=0; OUT=59; fi
  cat > $E/$v.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  $P
  <playlist id="p"><entry producer="c" in="$IN" out="$OUT"/></playlist>
  <tractor id="main" in="0" out="59"><multitrack><track producer="p"/></multitrack></tractor>
</mlt>
XML
  render $E/$v.mlt $E/$v.mp4
  note "$v" "frames=$(nframes $E/$v.mp4) y0=$(yframe $E/$v.mp4 0) y59=$(yframe $E/$v.mp4 59) src100=$(yframe $E/clipA.mp4 100)"
done
note "fin" "script 13 completo"
