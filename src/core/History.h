#pragma once
// Historial de deshacer/rehacer. Tanto el usuario como (en la Fase 5) el asistente
// de IA deben modificar la línea de tiempo a través de apply(), de modo que cada
// cambio quede registrado con su etiqueta y sea reversible.
//
// Estrategia: instantáneas completas de la Timeline antes de cada operación.
// Es simple y robusta para esta fase; si el rendimiento con proyectos grandes lo
// exige, se sustituirá por comandos inversos (ver DECISIONS/0002).
#include <functional>
#include <string>
#include <vector>

#include "core/Timeline.h"

namespace nexo {

class History {
public:
    explicit History(Timeline initial) : current_(std::move(initial)) {}

    const Timeline& timeline() const { return current_; }

    // Ejecuta `op` sobre una copia. Solo si devuelve ok se adopta el resultado y
    // se registra la entrada. Un fallo deja el estado intacto.
    Result apply(const std::string& label, const std::function<Result(Timeline&)>& op);

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool undo();
    bool redo();

    // Etiqueta de la próxima operación a deshacer/rehacer ("" si no hay).
    std::string undoLabel() const { return undo_.empty() ? "" : undo_.back().label; }
    std::string redoLabel() const { return redo_.empty() ? "" : redo_.back().label; }

    // Etiquetas de todo lo aplicado, de la más antigua a la más reciente.
    std::vector<std::string> log() const;

private:
    struct Entry {
        std::string label;
        Timeline before;   // estado previo a la operación
    };
    Timeline current_;
    std::vector<Entry> undo_;
    std::vector<Entry> redo_;   // `before` guarda el estado al que se vuelve al rehacer
};

}  // namespace nexo
