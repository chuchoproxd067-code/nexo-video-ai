source lab/common.sh
for k in filters transitions producers consumers; do
  melt -query $k 2>/dev/null | sed 's/^ *- *//' | grep -v '^[a-z]*:$' | tr '\n' ' ' > evidence/query_$k.txt
  note "servicios $k" "$(cat evidence/query_$k.txt)"
done
for f in affine crop freeze chroma mirror brightness lift_gamma_gain avfilter.eq avfilter.afftdn avfilter.equalizer avfilter.loudnorm dynamictext volume lumakey; do
  melt -query filter=$f > evidence/q_filter_$f.txt 2>&1 || true
  note "filter $f" "$(grep -E '^ *- [a-z_.]+|title|identifier' evidence/q_filter_$f.txt | sed 's/^ *//' | tr '\n' ' ' | cut -c1-1500)"
done
for p in timewarp qtext color qimage pango; do
  melt -query producer=$p > evidence/q_producer_$p.txt 2>&1 || true
  note "producer $p" "$(grep -E '^ *- [a-z_.]+|identifier' evidence/q_producer_$p.txt | sed 's/^ *//' | tr '\n' ' ' | cut -c1-900)"
done
for t in luma composite affine mix; do
  melt -query transition=$t > evidence/q_trans_$t.txt 2>&1 || true
  note "transition $t" "$(grep -E '^ *- [a-z_.]+|identifier' evidence/q_trans_$t.txt | sed 's/^ *//' | tr '\n' ' ' | cut -c1-900)"
done
L="$(dirname "$(which melt)")/../share/mlt/lumas"
note "lumas" "$(ls -R "$L" | head -80 | tr '\n' ' ')"
note "encoders" "$(ffmpeg -hide_banner -encoders 2>/dev/null | grep -E 'libx264|libx265|libvpx|libopus|libvorbis|libmp3lame|aac|prores|gif|flac|pcm_s16le|libsvtav1|libaom|h264_nvenc|h264_mf|h264_qsv' | awk '{print $2}' | tr '\n' ' ')"
note "muxers" "$(ffmpeg -hide_banner -muxers 2>/dev/null | grep -E ' (mp4|mov|matroska|webm|gif|mp3|wav|ogg|flac|avi|ipod|adts) ' | awk '{print $2}' | tr '\n' ' ')"
