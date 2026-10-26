#include "core/History.h"

#include <utility>

namespace nexo {

Result History::apply(const std::string& label, const std::function<Result(Timeline&)>& op) {
    Timeline candidate = current_;
    Result r = op(candidate);
    if (!r.ok) return r;
    // Defensa adicional: una operación nunca debe dejar el modelo inválido.
    const std::string problem = candidate.validate();
    if (!problem.empty()) return Result::failure("Operación rechazada: " + problem);
    undo_.push_back({label, std::move(current_)});
    current_ = std::move(candidate);
    redo_.clear();
    return r;
}

bool History::undo() {
    if (undo_.empty()) return false;
    Entry e = std::move(undo_.back());
    undo_.pop_back();
    // Para rehacer guardamos el estado actual (el "después") con la misma etiqueta.
    redo_.push_back({e.label, std::move(current_)});
    current_ = std::move(e.before);
    return true;
}

bool History::redo() {
    if (redo_.empty()) return false;
    Entry e = std::move(redo_.back());
    redo_.pop_back();
    undo_.push_back({e.label, std::move(current_)});
    current_ = std::move(e.before);
    return true;
}

std::vector<std::string> History::log() const {
    std::vector<std::string> out;
    out.reserve(undo_.size());
    for (const Entry& e : undo_) out.push_back(e.label);
    return out;
}

}  // namespace nexo
