source lab/common.sh
mkclips
E=evidence
vd() { ffmpeg -hide_banner -i "$1" -af "$2volumedetect" -vn -f null - 2>&1 | grep -o 'mean_volume: [-0-9.]*' | cut -d' ' -f2; }
ffmpeg -hide_banner -v error -y -f lavfi -i "sine=frequency=440:duration=4,volume=0.03" -f lavfi -i "color=c=black:s=320x180:d=4:r=30" -c:v libx264 -c:a aac -shortest $E/quiet.mp4
zc() { ffmpeg -hide_banner -v error -i "$1" -vn -ac 1 -ar 8000 -f f32le - 2>/dev/null | python3 -c "import sys,array
d=sys.stdin.buffer.read();a=array.array('f');a.frombytes(d[:len(d)//4*4]);z=sum(1 for i in range(1,len(a)) if a[i-1]<0<=a[i]);print(round(z/(len(a)/8000.0)))" 2>/dev/null; }
t() { local n="$1" src="$2"; shift 2
  melt $E/$src.mp4 "$@" -consumer avformat:$E/au2_$n.mp4 vcodec=libx264 acodec=aac > $E/au2_$n.log 2>&1 < /dev/null
  if [ -s $E/au2_$n.mp4 ]; then note "audio [$n]" "mean=$(vd $E/au2_$n.mp4 '') freq=$(zc $E/au2_$n.mp4)Hz"; else note "audio [$n]" "FALLÓ $(grep -v -i ladspa $E/au2_$n.log | tail -2 | tr '\n' ' ')"; fi; }
note "python" "$(which python3 2>&1) $(python3 --version 2>&1)"
t base quiet
t norm1 quiet -filter volume normalise=1
t norm_maxgain quiet -filter volume gain=20
t dynloud quiet -filter dynamic_loudness
t dynloud_t quiet -filter dynamic_loudness target_loudness=-16
t rbpitch15 quiet -filter rbpitch pitchscale=1.5
t rbpitch07 quiet -filter rbpitch pitchscale=0.7
t lvl_pitch quiet -filter pitch pitchscale=1.5
for f in rbpitch dynamic_loudness volume pitch rnnoise; do melt -query filter=$f 2>&1 | grep -E 'identifier' | sed 's/^ *- *identifier: //' | tr '\n' ' ' > $E/q2_$f.txt; note "props filtro $f" "$(cut -c1-300 $E/q2_$f.txt)"; done
note "fin" "script 11 completo"
