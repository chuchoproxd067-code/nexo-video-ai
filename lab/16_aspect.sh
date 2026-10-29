source lab/common.sh
E=evidence; mkdir -p $E
W="$(pwd -W)"
# Vertical 360x640: rojo con una franja blanca central estrecha (si se estira, la franja llena todo el ancho).
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=red:s=360x640:d=3:r=30,drawbox=x=150:y=0:w=60:h=640:color=white:t=fill" -c:v libx264 -pix_fmt yuv420p $E/vert.mp4
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=red:s=640x480:d=3:r=30,drawbox=x=290:y=0:w=60:h=480:color=white:t=fill" -c:v libx264 -pix_fmt yuv420p $E/four3.mp4
mk() { # nombre archivo transición
cat > $E/$1.mlt <<XML
<?xml version="1.0" encoding="utf-8"?>
<mlt LC_NUMERIC="C" version="7.0.0" producer="main">
  <profile width="640" height="360" progressive="1" sample_aspect_num="1" sample_aspect_den="1" display_aspect_num="16" display_aspect_den="9" frame_rate_num="30" frame_rate_den="1" colorspace="709"/>
  <producer id="bg" in="0" out="29"><property name="resource">black</property><property name="mlt_service">color</property></producer>
  <producer id="fg" in="0" out="29"><property name="resource">$W/$E/$2</property></producer>
  <playlist id="p0"><entry producer="bg" in="0" out="29"/></playlist>
  <playlist id="p1"><entry producer="fg" in="0" out="29"/></playlist>
  <tractor id="main" in="0" out="29">
    <multitrack><track producer="p0"/><track producer="p1"/></multitrack>
    $3
  </tractor>
</mlt>
XML
}
P() { echo "<property name=\"$1\">$2</property>"; }
T() { echo "<transition>$(P mlt_service composite)$(P a_track 0)$(P b_track 1)$(P always_active 1)$1</transition>"; }
probe() { printf "izq=%s centro=%s der=%s ancho_blanco=%s" "$(yregion $1 5 20:100:10:130)" "$(yregion $1 5 20:100:310:130)" "$(yregion $1 5 20:100:610:130)" "$(yregion $1 5 640:6:0:180)"; }
t() { mk $1 $2 "$3"; if render $E/$1.mlt $E/$1.mp4; then note "$1" "$(probe $E/$1.mp4)"; else note "$1" "FALLÓ"; fi; }
for v in vert four3; do
  t ${v}_dist1fill "$v.mp4" "$(T "$(P fill 1)$(P distort 1)")"
  t ${v}_dist0fill "$v.mp4" "$(T "$(P fill 1)$(P distort 0)")"
  t ${v}_dist0 "$v.mp4" "$(T "$(P distort 0)")"
  t ${v}_geo_dist0 "$v.mp4" "$(T "$(P geometry '0/0:100%x100%:100')$(P fill 1)$(P distort 0)")"
  t ${v}_geo_center "$v.mp4" "$(T "$(P geometry '0/0:100%x100%:100')$(P fill 1)$(P distort 0)$(P halign centre)$(P valign middle)")"
  t ${v}_nofill "$v.mp4" "$(T "$(P fill 0)$(P distort 0)$(P halign centre)$(P valign middle)")"
done
note "fin" "script 16 completo"
