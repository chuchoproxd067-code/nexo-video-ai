source lab/common.sh
E=evidence; mkdir -p $E
W="$(pwd -W)"
# Medios de color sólido: A casi blanco, B oscuro, así una mezcla 50% se distingue (~135).
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=white:s=640x360:d=3:r=30" -c:v libx264 -pix_fmt yuv420p $E/sA.mp4
ffmpeg -hide_banner -v error -y -f lavfi -i "color=c=0x202020:s=640x360:d=3:r=30" -c:v libx264 -pix_fmt yuv420p $E/sB.mp4
R="100:60:270:150"
note "ref" "A=$(yregion $E/sA.mp4 10 $R) B=$(yregion $E/sB.mp4 10 $R)"
mk() { # nombre  props_fg(producer)  transición_xml(opcional)
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
    ${3:-<transition><property name="mlt_service">composite</property><property name="a_track">0</property><property name="b_track">1</property><property name="always_active">1</property><property name="fill">1</property><property name="distort">1</property></transition>}
  </tractor>
</mlt>
XML
}
t() { mk "$1" "$2" "$3"
  if render $E/$1.mlt $E/$1.mp4; then note "$1" "f5=$(yregion $E/$1.mp4 5 $R) f30=$(yregion $E/$1.mp4 30 $R) f55=$(yregion $E/$1.mp4 55 $R)"; else note "$1" "FALLÓ $(grep -i -E 'fail|error' $E/$1.log | head -2 | tr '\n' ' ')"; fi; }
F() { echo "<filter>$1</filter>"; }
P() { echo "<property name=\"$1\">$2</property>"; }
t o_none ""
t o_bright_a05 "$(F "$(P mlt_service brightness)$(P alpha 0.5)")"
t o_bright_a_anim "$(F "$(P mlt_service brightness)$(P alpha '0=1;59=0')")"
t o_bright_a0 "$(F "$(P mlt_service brightness)$(P alpha 0)")"
t o_ccm_aa05 "$(F "$(P mlt_service avfilter.colorchannelmixer)$(P av.aa 0.5)")"
t o_ccm_aa05_rgba "$(F "$(P mlt_service avfilter.format)$(P av.pix_fmts rgba)")$(F "$(P mlt_service avfilter.colorchannelmixer)$(P av.aa 0.5)")"
t o_aff_rect05 "$(F "$(P mlt_service affine)$(P transition.rect '0/0:640x360:0.5')")"
t o_aff_bg "$(F "$(P mlt_service affine)$(P background 'color:#00000000')$(P transition.rect '0/0:640x360:50')")"
t o_aff_mix "$(F "$(P mlt_service affine)$(P transition.rect '0/0:640x360:1')$(P transition.mix 50)")"
t o_comp_geo50 "" "<transition><property name=\"mlt_service\">composite</property>$(P a_track 0)$(P b_track 1)$(P always_active 1)$(P geometry '0/0:100%x100%:50')$(P fill 1)$(P distort 1)</transition>"
t o_comp_geo05 "" "<transition><property name=\"mlt_service\">composite</property>$(P a_track 0)$(P b_track 1)$(P always_active 1)$(P geometry '0/0:100%x100%:0.5')$(P fill 1)$(P distort 1)</transition>"
t o_qt_05 "" "<transition><property name=\"mlt_service\">qtblend</property>$(P a_track 0)$(P b_track 1)$(P always_active 1)$(P rect '0/0:640x360:0.5')</transition>"
t o_qt_50 "" "<transition><property name=\"mlt_service\">qtblend</property>$(P a_track 0)$(P b_track 1)$(P always_active 1)$(P rect '0/0:640x360:50')</transition>"
t o_qt_anim "" "<transition><property name=\"mlt_service\">qtblend</property>$(P a_track 0)$(P b_track 1)$(P always_active 1)$(P rect '0=0/0:640x360:1;59=0/0:640x360:0')</transition>"
t o_mixtr "" "<transition><property name=\"mlt_service\">mix</property>$(P a_track 0)$(P b_track 1)$(P always_active 1)$(P start 0.5)$(P end 0.5)</transition>"
t o_cairo "" "<transition><property name=\"mlt_service\">frei0r.cairoblend</property>$(P a_track 0)$(P b_track 1)$(P always_active 1)$(P 0 0.5)</transition>"
t o_lumaresource "" "<transition><property name=\"mlt_service\">luma</property>$(P a_track 0)$(P b_track 1)$(P always_active 1)$(P in 0)$(P out 59)</transition>"
# alpha del producer por filtro frei0r
t o_alpha0ps "$(F "$(P mlt_service frei0r.alpha0ps)$(P 0 0.5)")"
note "fin" "script 14 completo"
