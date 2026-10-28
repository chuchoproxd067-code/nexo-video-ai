source lab/common.sh
mkclips
E=evidence
vd() { ffmpeg -hide_banner -i "$1" -af "$2volumedetect" -vn -f null - 2>&1 | grep -o 'mean_volume: [-0-9.]*' | cut -d' ' -f2; }
# fuente silenciosa (sine a -30 dB) y fuente con ruido
ffmpeg -hide_banner -v error -y -f lavfi -i "sine=frequency=440:duration=4,volume=0.03" -f lavfi -i "color=c=black:s=320x180:d=4:r=30" -c:v libx264 -c:a aac -shortest $E/quiet.mp4
ffmpeg -hide_banner -v error -y -f lavfi -i "sine=frequency=440:duration=4,volume=0.3[a];anoisesrc=d=4:c=white:a=0.05[n];[a][n]amix=inputs=2:normalize=0" -f lavfi -i "color=c=black:s=320x180:d=4:r=30" -c:v libx264 -c:a aac -shortest $E/noisy.mp4
note "fuentes" "quiet mean=$(vd $E/quiet.mp4 '') noisy mean=$(vd $E/noisy.mp4 '') noisy hf=$(vd $E/noisy.mp4 'highpass=f=3000,')"
t() { # nombre fuente args...
  local n="$1" src="$2"; shift 2
  melt $E/$src.mp4 "$@" -consumer avformat:$E/au_$n.mp4 vcodec=libx264 acodec=aac > $E/au_$n.log 2>&1
  if [ -s $E/au_$n.mp4 ]; then note "audio [$n]" "mean=$(vd $E/au_$n.mp4 '') hf=$(vd $E/au_$n.mp4 'highpass=f=3000,') dur=$(ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 $E/au_$n.mp4)"; else note "audio [$n]" "FALLÓ $(grep -v -i ladspa $E/au_$n.log | tail -2 | tr '\n' ' ')"; fi
}
t quiet_none quiet
t quiet_loudnorm quiet -filter avfilter.loudnorm av.I=-16 av.TP=-1.5 av.LRA=11
t quiet_dynaudnorm quiet -filter avfilter.dynaudnorm av.f=150 av.g=15 av.p=0.9
t quiet_speechnorm quiet -filter avfilter.speechnorm
t quiet_dynamic_loudness quiet -filter dynamic_loudness
t noisy_none noisy
t noisy_afftdn noisy -filter avfilter.afftdn av.nr=30 av.nf=-40
t noisy_afftdn_tn noisy -filter avfilter.afftdn av.nr=30 av.nf=-40 av.tn=1
t noisy_anlmdn noisy -filter avfilter.anlmdn av.s=0.001
t noisy_arnndn noisy -filter avfilter.arnndn av.m=dummy
t noisy_rnnoise noisy -filter rnnoise
t noisy_lowpass noisy -filter avfilter.lowpass av.f=2000
for p in rubberband sox.pitch avfilter.rubberband avfilter.asetrate avfilter.atempo; do
  melt -query filter=$p 2>&1 | grep -E 'identifier' | sed 's/^ *- *identifier: //' | tr '\n' ' ' > $E/q_$p.txt; note "props filtro $p" "$(cut -c1-300 $E/q_$p.txt)"
done
t pitch_rb quiet -filter rubberband pitchscale=1.5
t pitch_rb2 quiet -filter rubberband pitch=1.5
t pitch_av noisy -filter avfilter.rubberband av.pitch=1.5
t pitch_sox quiet -filter sox effect=pitch 600
# frecuencia dominante estimada por cruces por cero
zc() { ffmpeg -hide_banner -v error -i "$1" -af "aformat=channel_layouts=mono,astats=metadata=1:reset=0" -vn -f null - 2>&1 | head -0; ffmpeg -hide_banner -v error -i "$1" -vn -ac 1 -ar 8000 -f f32le - 2>/dev/null | python3 -c "import sys,struct
d=sys.stdin.buffer.read();import array;a=array.array('f');a.frombytes(d[:len(d)//4*4]);z=sum(1 for i in range(1,len(a)) if a[i-1]<0<=a[i]);print(round(z/(len(a)/8000.0)))" 2>/dev/null; }
for n in quiet_none pitch_rb pitch_rb2 pitch_sox; do [ -s $E/au_$n.mp4 ] && note "frecuencia $n" "$(zc $E/au_$n.mp4) Hz aprox (440 sin cambio, 660 = +7 semitonos)"; done
