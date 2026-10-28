source lab/common.sh
mkclips
E=evidence
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


# crop de MLT con tamaño de salida
note "crop ffprobe" "$(ffprobe -v error -show_entries stream=width,height -of csv=p=0 $E/crop_crop_left_100_right_100.mp4 2>/dev/null | head -1)"
# affine con use_normalized (0..1)
render $E/clipA.mp4 $E/aff_norm.mp4 out=29 -filter affine transition.rect="0.25/0.25:0.5x0.5:1" use_normalized=1
note "affine normalizado" "TL=$(yregion $E/aff_norm.mp4 10 100:60:20:20) centro=$(yregion $E/aff_norm.mp4 10 100:60:270:150)"

# affine: rect centrado con escala (x,y = esquina sup-izq en píxeles de salida)
render $E/clipA.mp4 $E/aff_scale50.mp4 out=29 -filter affine transition.rect="160/90:320x180:100"
note "affine escala 50 centrada" "centro=$(yregion $E/aff_scale50.mp4 10 100:60:270:150) esquina=$(yregion $E/aff_scale50.mp4 10 60:40:5:5) (esquina negra=16)"
render $E/clipA.mp4 $E/aff_fix.mp4 out=29 -filter affine transition.rect="0/0:640x360:100" transition.fix_rotate_z=20 transition.distort=0
note "affine rotación z=20" "esquina=$(yregion $E/aff_fix.mp4 10 40:40:0:0) centro=$(yregion $E/aff_fix.mp4 10 100:60:270:150)"
render $E/clipA.mp4 $E/aff_rotx.mp4 out=29 -filter affine transition.rect="0/0:640x360:100" transition.fix_rotate_x=20
note "affine rotación x=20" "esquina=$(yregion $E/aff_rotx.mp4 10 40:40:0:0) centro=$(yregion $E/aff_rotx.mp4 10 100:60:270:150)"
render $E/clipA.mp4 $E/aff_opac25.mp4 out=29 -filter affine transition.rect="0/0:640x360:25"
note "affine opacidad 25%" "centro=$(yregion $E/aff_opac25.mp4 10 100:60:270:150) base=$(yregion $E/clipA.mp4 10 100:60:270:150) (con fondo negro debería ser ~25% de la base+16)"
# affine con background transparente y compuesto sobre el fondo del tractor: opacity en composite? rect desplazado
render $E/clipA.mp4 $E/aff_move.mp4 out=29 -filter affine transition.rect="100/50:640x360:100"
note "affine desplazado (100,50)" "esquina sup-izq=$(yregion $E/aff_move.mp4 10 60:30:5:5) (negro) esquina inf-der=$(yregion $E/aff_move.mp4 10 60:30:575:325)"
# crop: comparar con hipótesis (recorte escalado vs con barras)
ffmpeg -hide_banner -v error -y -i $E/clipA.mp4 -vf "select=eq(n\,10),crop=320:360:160:0,scale=640:360" -frames:v 1 $E/hyp_stretch.png
ffmpeg -hide_banner -v error -y -i $E/clipA.mp4 -vf "select=eq(n\,10),crop=320:360:160:0,scale=320:360,pad=640:360:160:0" -frames:v 1 $E/hyp_bars.png
ffmpeg -hide_banner -v error -y -i $E/aff_scale50.mp4 -vf "select=eq(n\,10)" -frames:v 1 $E/_unused.png
render $E/clipA.mp4 $E/crop_c.mp4 out=29 -filter crop left=160 right=160
ffmpeg -hide_banner -v error -y -i $E/crop_c.mp4 -vf "select=eq(n\,10)" -frames:v 1 $E/crop_c10.png
note "crop 160/160 vs hipótesis" "estirado=$(psnr $E/crop_c10.png $E/hyp_stretch.png) barras=$(psnr $E/crop_c10.png $E/hyp_bars.png)"
