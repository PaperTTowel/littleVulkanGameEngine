#pragma once

#include <functional>
#include <string>
#include <vector>

namespace lve::editor {

  struct HistoryCommand {
    std::string label;
    std::function<void()> undo;
    std::function<void()> redo;
  };

  class EditorHistory {
  public:
    void push(HistoryCommand command);
    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    void clear();
    const std::vector<HistoryCommand> &getCommands() const { return commands; }
    std::size_t getCursor() const { return cursor; }

  private:
    std::vector<HistoryCommand> commands{};
    std::size_t cursor{0};
  };

} // namespace lve::editor
