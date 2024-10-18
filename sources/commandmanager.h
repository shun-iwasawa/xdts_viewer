#pragma once
#ifndef COMMANDMANAGER_H
#define COMMANDMANAGER_H

#include <QMap>

class QAction;

enum CommandId { Cmd_Cut, Cmd_Copy, Cmd_Paste, Cmd_Delete };

class CommandManager {  // singleton
  QMap<CommandId, QAction*> m_commands;

  CommandManager();

public:
  static CommandManager* instance();
  ~CommandManager();
  void registerAction(CommandId, QAction*);
  QAction* getAction(const CommandId);
};

#endif