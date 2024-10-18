#include "commandmanager.h"

#include <QAction>

CommandManager::CommandManager() {}

CommandManager* CommandManager::instance() {
  static CommandManager _instance;
  return &_instance;
}

CommandManager::~CommandManager() {
  for (auto action : m_commands.values()) delete action;
}

void CommandManager::registerAction(CommandId id, QAction* action) {
  m_commands.insert(id, action);
}

QAction* CommandManager::getAction(const CommandId id) {
  return m_commands.value(id, nullptr);
}