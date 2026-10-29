#pragma once
// Estilos de transición entre dos clips de video. Cada estilo (salvo "disolver") es un patrón
// de luminosidad PGM propio (assets/lumas/<id>.pgm, CC0, generado por tools/gen_assets.py) que
// la transición `luma` de MLT usa como máscara de barrido.
#include <string>
#include <vector>

namespace nexo {

struct TransitionStyle {
    const char* id;      // estable, se guarda en el proyecto; "" = disolver
    const char* label;   // para mostrar al usuario
};

const std::vector<TransitionStyle>& transitionStyles();
// "" (disolver) y todos los id de la lista son válidos.
bool validTransitionStyle(const std::string& id);

}  // namespace nexo
