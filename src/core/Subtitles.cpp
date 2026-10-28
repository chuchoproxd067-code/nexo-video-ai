#include "core/Subtitles.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace nexo {

namespace {

bool parseTime(const std::string& s, double& seconds) {
    // HH:MM:SS,mmm  (también acepta '.')
    int h = 0, m = 0, sec = 0, ms = 0;
    char sep = 0;
    if (std::sscanf(s.c_str(), "%d:%d:%d%c%d", &h, &m, &sec, &sep, &ms) != 5) return false;
    if (sep != ',' && sep != '.') return false;
    if (h < 0 || m < 0 || m > 59 || sec < 0 || sec > 59 || ms < 0 || ms > 999) return false;
    seconds = h * 3600.0 + m * 60.0 + sec + ms / 1000.0;
    return true;
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string formatTime(double seconds) {
    const long long total = std::llround(seconds * 1000.0);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02lld:%02lld:%02lld,%03lld", total / 3600000, (total / 60000) % 60, (total / 1000) % 60, total % 1000);
    return buf;
}

}  // namespace

bool parseSrt(const std::string& srtIn, FrameRate fps, std::vector<SubtitleCue>& out, std::string& error) {
    out.clear();
    if (!fps.valid()) { error = "Tasa de fotogramas inválida."; return false; }
    std::string srt = srtIn;
    if (srt.size() >= 3 && static_cast<unsigned char>(srt[0]) == 0xEF && static_cast<unsigned char>(srt[1]) == 0xBB &&
        static_cast<unsigned char>(srt[2]) == 0xBF) srt.erase(0, 3);
    std::vector<std::string> lines;
    {
        std::string cur;
        for (char c : srt) {
            if (c == '\r') continue;
            if (c == '\n') { lines.push_back(cur); cur.clear(); } else cur += c;
        }
        lines.push_back(cur);
    }
    std::vector<SubtitleCue> cues;
    std::size_t i = 0;
    int block = 0;
    while (i < lines.size()) {
        if (trim(lines[i]).empty()) { ++i; continue; }
        ++block;
        // Línea opcional de número.
        if (lines[i].find("-->") == std::string::npos) ++i;
        if (i >= lines.size() || lines[i].find("-->") == std::string::npos) {
            error = "Bloque " + std::to_string(block) + ": falta la línea de tiempos.";
            return false;
        }
        const std::string& tl = lines[i];
        const std::size_t arrow = tl.find("-->");
        double t0 = 0, t1 = 0;
        // Tras la segunda hora puede haber coordenadas de posición (X1:...) que se ignoran.
        std::string right = trim(tl.substr(arrow + 3));
        const std::size_t sp = right.find(' ');
        if (sp != std::string::npos) right = right.substr(0, sp);
        if (!parseTime(trim(tl.substr(0, arrow)), t0) || !parseTime(right, t1)) {
            error = "Bloque " + std::to_string(block) + ": tiempos inválidos.";
            return false;
        }
        ++i;
        std::string text;
        while (i < lines.size() && !trim(lines[i]).empty()) {
            if (!text.empty()) text += '\n';
            text += lines[i];
            ++i;
        }
        text = trim(text);
        SubtitleCue c;
        c.start = secondsToFrames(t0, fps);
        c.end = secondsToFrames(t1, fps);
        if (c.end <= c.start) c.end = c.start + 1;
        if (text.empty()) { error = "Bloque " + std::to_string(block) + ": sin texto."; return false; }
        c.text = text;
        cues.push_back(std::move(c));
    }
    out = std::move(cues);
    return true;
}

std::string formatSrt(const std::vector<SubtitleCue>& cues, FrameRate fps) {
    std::ostringstream o;
    int n = 0;
    for (const SubtitleCue& c : cues) {
        o << ++n << "\n" << formatTime(framesToSeconds(c.start, fps)) << " --> " << formatTime(framesToSeconds(c.end, fps)) << "\n"
          << c.text << "\n\n";
    }
    return o.str();
}

std::vector<SubtitleCue> cuesFromTrack(const Timeline& tl, std::size_t track) {
    std::vector<SubtitleCue> out;
    if (track >= tl.trackCount()) return out;
    for (const Clip& c : tl.track(track).clips)
        if (c.kind == ClipKind::Text) out.push_back({c.timelineStart, c.timelineEnd(), c.text.text});
    return out;
}

}  // namespace nexo
