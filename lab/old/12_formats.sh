source lab/common.sh
mkclips
E=evidence
fmt() { local n="$1" ext="$2"; shift 2
  echo "== $n" >&2
  timeout 150 melt $E/clipA.mp4 out=59 -consumer avformat:$E/exp_$n.$ext "$@" > $E/exp_$n.log 2>&1 < /dev/null
  rc=$?
  if [ -s $E/exp_$n.$ext ]; then note "formato $n" "rc=$rc OK $(stat -c %s $E/exp_$n.$ext) bytes: $(ffprobe -v error -show_entries stream=codec_name,codec_type,width,height -of csv=p=0 $E/exp_$n.$ext | tr '\n' ' ')"
  else note "formato $n" "rc=$rc FALLÓ $(grep -v -i -E 'ladspa|lv2|vst' $E/exp_$n.log | tail -3 | tr '\n' ' ')"; fi
}
fmt mp4 mp4 vcodec=libx264 acodec=aac crf=23 preset=veryfast
fmt mov mov vcodec=libx264 acodec=aac crf=23 preset=veryfast
fmt webm webm vcodec=libvpx-vp9 acodec=libopus vb=0 cpu-used=5 crf=31
fmt gif gif vcodec=gif an=1
fmt mp3 mp3 vn=1 acodec=libmp3lame ab=192k
fmt wav wav vn=1 acodec=pcm_s16le
fmt mkv mkv vcodec=libx264 acodec=aac
fmt h265 mp4 vcodec=libx265 acodec=aac crf=28
note "fin" "script 12 completo"
