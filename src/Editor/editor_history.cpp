#include "editor_history.hpp"

namespace lve::editor {

  void EditorHistory::push(HistoryCommand command) {
    if (cursor < commands.size()) {
      commands.erase(commands.begin() + static_cast<std::ptrdiff_t>(cursor), commands.end());
    }
    commands.push_back(std::move(command));
    cursor = commands.size();
  }

  bool EditorHistory::canUndo() const {
    return cursor > 0;
  }

  bool EditorHistory::canRedo() const {
    return cursor < commands.size();
  }

  bool EditorHistory::undo() {
    if (!canUndo()) return false;
    cursor--;
    if (commands[cursor].undo) {
      commands[cursor].undo();
    }
    return true;
  }

  bool EditorHistory::redo() {
    if (!canRedo()) return false;
    if (commands[cursor].redo) {
      commands[cursor].redo();
    }
    cursor++;
    return true;
  }

  void EditorHistory::clear() {
    commands.clear();
    cursor = 0;
  }

} // namespace lve::editor
