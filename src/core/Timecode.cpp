#include "core/Timecode.h"

#include <cmath>
#include <cstdio>

namespace nexo {

Frame secondsToFrames(double seconds, FrameRate fps) {
    if (!fps.valid()) return 0;
    return static_cast<Frame>(std::llround(seconds * fps.num / fps.den));
}

double framesToSeconds(Frame frames, FrameRate fps) {
    if (!fps.valid()) return 0.0;
    return static_cast<double>(frames) * fps.den / fps.num;
}

std::string toTimecode(Frame frames, FrameRate fps) {
    if (!fps.valid() || frames < 0) return "--:--:--:--";
    // Tasa entera de visualización (29.97 -> 30). No es drop-frame.
    const Frame nominal = (fps.num + fps.den / 2) / fps.den;
    const Frame ff = frames % nominal;
    Frame total = frames / nominal;
    const Frame ss = total % 60;
    total /= 60;
    const Frame mm = total % 60;
    const Frame hh = total / 60;
    char buf[48];
    std::snprintf(buf, sizeof buf, "%02lld:%02lld:%02lld:%02lld",
                  static_cast<long long>(hh), static_cast<long long>(mm),
                  static_cast<long long>(ss), static_cast<long long>(ff));
    return buf;
}

}  // namespace nexo
