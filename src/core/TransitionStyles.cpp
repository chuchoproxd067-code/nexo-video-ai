#include "core/TransitionStyles.h"

namespace nexo {

const std::vector<TransitionStyle>& transitionStyles() {
    static const std::vector<TransitionStyle> styles{
        {"", "Disolver (fundido cruzado)"},
        {"wipe-right", "Cortinilla → derecha"},
        {"wipe-left", "Cortinilla ← izquierda"},
        {"wipe-down", "Cortinilla ↓ abajo"},
        {"wipe-up", "Cortinilla ↑ arriba"},
        {"diagonal", "Diagonal"},
        {"diagonal-inv", "Diagonal inversa"},
        {"circle-open", "Iris: abrir desde el centro"},
        {"circle-close", "Iris: cerrar hacia el centro"},
        {"diamond", "Diamante"},
        {"clock", "Reloj (barrido radial)"},
        {"blinds-h", "Persianas horizontales"},
        {"blinds-v", "Persianas verticales"},
        {"bars-v", "Barras verticales escalonadas"},
        {"checker", "Tablero"},
        {"spiral", "Espiral"},
    };
    return styles;
}

bool validTransitionStyle(const std::string& id) {
    for (const TransitionStyle& s : transitionStyles())
        if (id == s.id) return true;
    return false;
}

}  // namespace nexo
