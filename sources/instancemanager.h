#pragma once

#ifndef INSTANCEMANAGER_H
#define INSTANCEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QByteArray>

class QLocalServer;
class QLocalSocket;
class QSharedMemory;

// Coordinates single-instance behavior for the app: a single hidden
// "mothership" process arbitrates which XDTS file is open in which
// per-file window ("worker") process, and forwards duplicate open
// requests to the already-open window instead of starting a new one.
// 単一起動の調整を行うクラス。不可視の「母艦(mothership)」プロセスが、
// どのXDTSファイルがどのウィンドウ（"worker"）プロセスで開かれているかを
// 管理し、重複するオープン要求は新規ウィンドウを開かず既存ウィンドウへ
// 転送する。
class InstanceManager : public QObject {
  Q_OBJECT

public:
  enum class Role {
    Standalone,  // IPC unavailable: legacy single-window behavior, no
                 // coordination
    Mothership,  // invisible coordinator; caller must not show a window
    Worker,      // shows a window for outPath; spawned by the mothership
    Forwarded,  // request forwarded to an existing mothership; caller must exit
                // immediately
  };

  static InstanceManager* instance();

  // Determines this process's role from the command-line arguments and
  // performs the corresponding IPC setup (become the mothership / connect
  // to it / forward a request to it). `outPath` receives the XDTS path this
  // process should show, meaningful only for the Worker/Standalone roles.
  // Call once, right after QApplication is constructed.
  // コマンドライン引数からこのプロセスの役割を判定し、対応するIPCの初期化
  // （母艦になる／母艦へ接続する／母艦へ要求を転送する）を行う。outPathには
  // このプロセスが表示すべきXDTSパスが入る（Worker/Standaloneの場合のみ
  // 意味を持つ）。QApplication構築直後に一度だけ呼び出すこと。
  Role bootstrap(const QStringList& args, QString& outPath);

  // Re-registers this worker's currently open path(s) with the mothership
  // (a paired genga/douga sheet occupies two paths at once). No-op unless
  // this process's role is Worker. Call from MyWindow::onLoad() whenever the
  // user opens a different file in this window.
  // このワーカーが現在開いているパス（原画欄／動画欄のペアシートの場合は
  // 2つ）を母艦へ再登録する。役割がWorker以外の場合は何もしない。
  // ユーザーがこのウィンドウで別のファイルを開いた際にMyWindow::onLoad()
  // から呼び出す。
  void notifyPathChanged(const QStringList& paths);

signals:
  // Worker role only: emitted when the mothership asks this window to
  // reload its XDTS data and come to the foreground, because another
  // launch requested the file already open here.
  // Worker側でのみ発火する。別の起動がこのウィンドウで既に開いている
  // ファイルを要求したため、母艦からXDTSデータの再読み込みと前面化を
  // 指示された際にシグナルされる。
  void reloadAndActivateRequested();

private slots:
  void onNewConnection();          // mothership: a new incoming connection
  void onServerSocketReadyRead();  // mothership: data from a worker/forwarder
  void onServerSocketDisconnected();
  void onWorkerSocketReadyRead();  // worker: data pushed from the mothership

private:
  explicit InstanceManager(QObject* parent = nullptr);

  enum class Command : quint8 {
    Register          = 0,
    Open              = 1,
    Reload            = 2,
    ClearRegistration = 3
  };

  bool tryAcquireLock();
  void startMothership(const QString& initialPath);
  void connectAsWorker();
  void handleOpenRequest(const QString& path);
  bool sendFrame(QLocalSocket* socket, Command cmd, const QString& path);
  static bool tryReadFrame(QByteArray& buffer, Command& cmd, QString& path);
  void spawnWorker(const QString& path);

  Role m_role            = Role::Standalone;
  QSharedMemory* m_lock  = nullptr;
  QLocalServer* m_server = nullptr;
  QLocalSocket* m_workerSocket =
      nullptr;  // Worker role: connection to the mothership

  // Mothership-side bookkeeping: normalized path -> connected worker socket,
  // plus a per-socket receive buffer for length-prefixed message framing.
  QMap<QString, QLocalSocket*> m_workers;
  QMap<QLocalSocket*, QByteArray> m_recvBuffers;

  // Worker-side receive buffer for messages pushed by the mothership.
  QByteArray m_workerRecvBuffer;
};

#endif
