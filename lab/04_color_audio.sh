source lab/common.sh
mkclips
E=evidence
# ---- color ----
base=$(yframe $E/clipA.mp4 10)
for v in "avfilter.eq brightness=0.3" "avfilter.eq saturation=0" "brightness level=0.5" "lift_gamma_gain gain_r=1.5" "greyscale" "sepia" "invert" "avfilter.vignette" "avfilter.boxblur luma_radius=8" "avfilter.hue h=90" "avfilter.colorbalance rs=0.5" "frei0r.colorize"; do
  n="col_$(echo $v | tr ' =.' '___')"
  if render $E/clipA.mp4 $E/$n.mp4 out=29 -filter $v; then note "color [$v]" "OK YAVG f10=$(yframe $E/$n.mp4 10) (base $base)"; else note "color [$v]" "FALLÓ $(grep -i -E 'fail|error|not' $E/$n.log | head -2 | tr '\n' ' ')"; fi
done

# ---- audio ----
vd() { ffmpeg -hide_banner -i "$1" -af volumedetect -vn -f null - 2>&1 | grep -o 'mean_volume: [-0-9.]*' | cut -d' ' -f2; }
note "audio base" "mean=$(vd $E/clipA.mp4)"
for v in "avfilter.equalizer f=1000 g=15 w=1" "avfilter.bass g=15" "avfilter.treble g=15" "avfilter.afftdn" "avfilter.loudnorm" "avfilter.highpass f=500" "avfilter.lowpass f=500" "avfilter.acompressor" "avfilter.areverse" "avfilter.aecho" "avfilter.atempo tempo=1.5"; do
  n="aud_$(echo $v | tr ' =.' '___')"
  if render $E/clipA.mp4 $E/$n.mp4 -filter $v; then note "audio [$v]" "OK mean=$(vd $E/$n.mp4) frames=$(nframes $E/$n.mp4)"; else note "audio [$v]" "FALLÓ $(grep -i -E 'fail|error|not' $E/$n.log | head -2 | tr '\n' ' ')"; fi
done
