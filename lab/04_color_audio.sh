source lab/common.sh
mkclips
E=evidence
render $E/clipA.mp4 $E/base.mp4 out=29
base=$(yframe $E/base.mp4 10)
note "baseline" "YAVG f10=$base frames=$(nframes $E/base.mp4)"
# cada variante: se mide PSNR frente a la base; cerca de 'inf' o >45 = el filtro no hizo nada
try() { # nombre args...
  local n="$1"; shift
  if render $E/clipA.mp4 $E/c_$n.mp4 out=29 "$@"; then
    note "efecto [$n]" "psnr_vs_base=$(psnr $E/c_$n.mp4 $E/base.mp4) YAVG=$(yframe $E/c_$n.mp4 10) satU=$(ffmpeg -hide_banner -v error -i $E/c_$n.mp4 -vf "select=eq(n\,10),signalstats,metadata=print:key=lavfi.signalstats.SATAVG:file=-" -frames:v 1 -an -f null - 2>/dev/null | grep -o 'SATAVG=[0-9.]*' | head -1)"
  else note "efecto [$n]" "FALLÓ $(grep -i -E 'fail|error|not' $E/c_$n.log | head -2 | tr '\n' ' ')"; fi
}
try brightness_mlt -filter brightness level=0.5
try brightness_mlt15 -filter brightness level=1.5
try eq_b -filter avfilter.eq av.brightness=0.3
try eq_c -filter avfilter.eq av.contrast=1.8
try eq_s0 -filter avfilter.eq av.saturation=0
try eq_s2 -filter avfilter.eq av.saturation=2.5
try greyscale -filter greyscale
try grayscale -filter grayscale
try negate -filter avfilter.negate
try colortemp_4000 -filter avfilter.colortemperature av.temperature=3000
try colortemp_9000 -filter avfilter.colortemperature av.temperature=10000
try sepia_mixer -filter avfilter.colorchannelmixer av.rr=0.393 av.rg=0.769 av.rb=0.189 av.gr=0.349 av.gg=0.686 av.gb=0.168 av.br=0.272 av.bg=0.534 av.bb=0.131
try vignette -filter avfilter.vignette
try vignette_a -filter avfilter.vignette av.angle=0.8
try boxblur -filter avfilter.boxblur av.luma_radius=10 av.luma_power=2
try gblur -filter avfilter.gblur av.sigma=8
try unsharp -filter avfilter.unsharp av.luma_amount=2 av.luma_msize_x=7 av.luma_msize_y=7
try pixelize -filter avfilter.pixelize av.width=24 av.height=24
try frei0r_pixeliz0r -filter frei0r.pixeliz0r
try mirror_flip -filter mirror mirror=flip
try mirror_flop -filter mirror mirror=flop
try hflip -filter avfilter.hflip
try vflip -filter avfilter.vflip
try lgg -filter lift_gamma_gain gain_r=1.8 gain_g=0.5 gain_b=0.5
try stabilize_vidstab -filter vidstab
try deshake -filter deshake
try crop_px -filter crop left=160 right=160
try crop_px_center -filter crop left=160 right=160 center=0
try avcrop -filter avfilter.crop av.w=iw*0.5 av.h=ih av.x=iw*0.25 av.y=0

# ---- audio con prefijo av. ----
vd() { ffmpeg -hide_banner -i "$1" -af "$2volumedetect" -vn -f null - 2>&1 | grep -o 'mean_volume: [-0-9.]*' | cut -d' ' -f2; }
render $E/clipA.mp4 $E/abase.mp4
note "audio base" "mean=$(vd $E/abase.mp4 '')"
atry() { local n="$1"; shift
  if render $E/clipA.mp4 $E/a_$n.mp4 "$@"; then note "audio [$n]" "mean=$(vd $E/a_$n.mp4 '') frames=$(nframes $E/a_$n.mp4) lf=$(vd $E/a_$n.mp4 'lowpass=f=300,') hf=$(vd $E/a_$n.mp4 'highpass=f=3000,') (440 Hz: lf alto)"
  else note "audio [$n]" "FALLÓ"; fi
}
atry none
atry bass_p15 -filter avfilter.bass av.g=15 av.f=440
atry bass_m15 -filter avfilter.bass av.g=-15 av.f=440
atry treble_p15 -filter avfilter.treble av.g=15
atry eq_1k -filter avfilter.equalizer av.f=440 av.g=15 av.width_type=h av.width=100
atry afftdn -filter avfilter.afftdn av.nr=20
atry loudnorm -filter avfilter.loudnorm av.I=-16
atry dynaudnorm -filter avfilter.dynaudnorm
atry arnndn -filter avfilter.arnndn
atry rubberband_pitch -filter rubberband pitch=1.5
atry sox_pitch -filter sox.pitch
atry volume0 -filter volume gain=0
