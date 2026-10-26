#pragma once
// Conversión entre fotogramas, segundos y timecode.
// Convención: todo el modelo trabaja en fotogramas enteros (Frame) para evitar
// errores de redondeo acumulados. Los segundos solo existen en los bordes (UI/IO).
#include <cstdint>
#include <string>

namespace nexo {

using Frame = std::int64_t;

struct FrameRate {
    int num = 30;
    int den = 1;
    bool valid() const { return num > 0 && den > 0; }
};

// Redondea al fotograma más cercano. Devuelve 0 si la tasa no es válida.
Frame secondsToFrames(double seconds, FrameRate fps);
double framesToSeconds(Frame frames, FrameRate fps);

// "HH:MM:SS:FF" con la tasa entera más cercana (sin drop-frame).
std::string toTimecode(Frame frames, FrameRate fps);

}  // namespace nexo
