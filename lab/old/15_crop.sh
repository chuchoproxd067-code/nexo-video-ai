source lab/common.sh
E=evidence; mkdir -p $E
W="$(pwd -W)"
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=white:s=640x360:d=3:r=30" -c:v libx264 -pix_fmt yuv420p $E/sA.mp4
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=0x202020:s=640x360:d=3:r=30" -c:v libx264 -pix_fmt yuv420p $E/sB.mp4
DEFT='<transition><property name="mlt_service">composite</property><property name="a_track">0</property><property name="b_track">1</property><property name="always_active">1</property><property name="fill">1</property><property name="distort">1</property></transition>'
mk() {
cat > $E/$1.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="bg" in="0" out="59"><property name="resource">$W/$E/sB.mp4</property></producer>
  <producer id="fg" in="0" out="59"><property name="resource">$W/$E/sA.mp4</property>$2</producer>
  <playlist id="p0"><entry producer="bg" in="0" out="59"/></playlist>
  <playlist id="p1"><entry producer="fg" in="0" out="59"/></playlist>
  <tractor id="main" in="0" out="59">
    <multitrack><track producer="p0"/><track producer="p1"/></multitrack>
    $DEFT
  </tractor>
</mlt>
XML
}
strip() { for x in 20 100 200 320 500 600; do printf "x%s=%s " $x "$(yregion $1 10 20:20:$x:170)"; done; for y in 10 60 180 320; do printf "y%s=%s " $y "$(yregion $1 10 20:20:310:$y)"; done; }
P() { echo "<property name=\"$1\">$2</property>"; }
F() { echo "<filter>$1</filter>"; }
t() { mk $1 "$2"; if render $E/$1.mlt $E/$1.mp4; then note "$1" "$(strip $E/$1.mp4)"; else note "$1" "FALLÓ"; fi; }
t c_none ""
t c_left160 "$(F "$(P mlt_service crop)$(P left 160)")"
t c_left160_zeros "$(F "$(P mlt_service crop)$(P left 160)$(P right 0)$(P top 0)$(P bottom 0)")"
t c_lr160 "$(F "$(P mlt_service crop)$(P left 160)$(P right 160)")"
t c_left160_c0 "$(F "$(P mlt_service crop)$(P left 160)$(P center 0)")"
t c_top90 "$(F "$(P mlt_service crop)$(P top 90)")"
t c_right160 "$(F "$(P mlt_service crop)$(P right 160)")"
t c_avcrop "$(F "$(P mlt_service avfilter.crop)$(P av.x 160)$(P av.w 'iw-160')$(P av.keep_aspect 0)")"
# alternativa: affine con rect mayor + recorte con 'rect' fuera del cuadro no recorta; probar affine con rect parcial
t c_aff_half "$(F "$(P mlt_service affine)$(P transition.rect '160/0:480x360:1')$(P transition.distort 0)")"
note "fin" "script 15 completo"
