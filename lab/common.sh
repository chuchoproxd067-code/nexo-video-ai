# Utilidades comunes del laboratorio.
note() { echo "::notice title=LAB $1::${2:0:3800}"; }
yall() { ffmpeg -hide_banner -i "$1" -vf signalstats,metadata=print:key=lavfi.signalstats.YAVG -an -f null - 2>&1 | grep -o 'YAVG=[0-9.]*' | cut -d= -f2; }
yframe() { yall "$1" | sed -n "$(( $2 + 1 ))p"; }
yregion() { # archivo, n_frame, w:h:x:y
  ffmpeg -hide_banner -v error -i "$1" -vf "select=eq(n\,$2),crop=$3,signalstats,metadata=print:key=lavfi.signalstats.YAVG" -an -frames:v 1 -f null - 2>&1 | grep -o 'YAVG=[0-9.]*' | cut -d= -f2
}
nframes() { ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of default=nw=1:nk=1 "$1"; }
mkclips() {
  [ -f evidence/clipA.mp4 ] && return
  ffmpeg -hide_banner -loglevel error -y -f lavfi -i testsrc=duration=4:size=640x360:rate=30 -f lavfi -i sine=frequency=440:duration=4 -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest evidence/clipA.mp4
  ffmpeg -hide_banner -loglevel error -y -f lavfi -i smptebars=duration=4:size=640x360:rate=30 -f lavfi -i sine=frequency=880:duration=4 -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest evidence/clipB.mp4
}
render() { # entrada salida [args de melt...]
  local in="$1" out="$2"; shift 2
  melt "$in" "$@" -consumer avformat:"$out" vcodec=libx264 acodec=aac > "${out%.*}.log" 2>&1 || true
  [ -s "$out" ]
}
