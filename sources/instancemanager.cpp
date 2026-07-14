#include "instancemanager.h"
#include "pathutils.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedMemory>
#include <QProcess>
#include <QMap>

#include <cstring>

namespace {
// Fixed identifiers shared by every process of this app so they can find
// each other. Kept in one place rather than as class statics to avoid
// static-initialization-order concerns.
// 各プロセスが互いを見つけられるようにするための固定識別子。
const QString kSharedMemoryKey =
    QStringLiteral("XDTSViewer.SingleInstance.Lock");
const QString kServerName = QStringLiteral("XDTSViewer.SingleInstance.IPC");
}  // namespace

// Internal argv flag marking a process as a worker window spawned by the
// mothership (as opposed to a plain user-facing launch). Not translated /
// not shown to the user.
const QString kWorkerFlag = QStringLiteral("--xdts-worker");

InstanceManager* InstanceManager::instance() {
  static InstanceManager _instance;
  return &_instance;
}

InstanceManager::InstanceManager(QObject* parent) : QObject(parent) {}

InstanceManager::Role InstanceManager::bootstrap(const QStringList& args,
                                                 QString& outPath) {
  // 1. This process was spawned by the mothership as a per-file window.
  int workerFlagIdx = args.indexOf(kWorkerFlag);
  if (workerFlagIdx >= 0) {
    outPath = (workerFlagIdx + 1 < args.size()) ? args.at(workerFlagIdx + 1)
                                                : QString();
    m_role  = Role::Worker;
    connectAsWorker();
    return m_role;
  }

  // Find the first non-flag argument (skip args[0], the executable path).
  QString requestedPath;
  for (int i = 1; i < args.size(); ++i) {
    if (!args.at(i).startsWith(QStringLiteral("--"))) {
      requestedPath = args.at(i);
      break;
    }
  }

  // 2. Try to become the mothership (the single coordinator process).
  if (tryAcquireLock()) {
    startMothership(requestedPath);
    return m_role;
  }

  // 3. Another mothership is already running: forward the request to it and
  // let the caller exit immediately without ever showing a window.
  {
    QLocalSocket socket;
    socket.connectToServer(kServerName);
    if (socket.waitForConnected(1000)) {
      sendFrame(&socket, Command::Open, requestedPath);
      socket.disconnectFromServer();
      m_role = Role::Forwarded;
      return m_role;
    }
  }

  // 4. Rare case: QSharedMemory reported an existing mothership, but it is
  // gone by the time we tried to connect (e.g. it just exited). Retry
  // becoming the mothership once; otherwise fall back to legacy
  // single-window behavior so the app still opens.
  // 稀なケース: QSharedMemoryは既存の母艦の存在を示したが、接続を試みた
  // 時点で既に終了している場合（例:直前に終了した等）。もう一度母艦への
  // 昇格を試み、それも失敗すれば調整なしの単独ウィンドウ動作にフォール
  // バックし、アプリが起動できなくなることを避ける。
  if (tryAcquireLock()) {
    startMothership(requestedPath);
    return m_role;
  }

  outPath = requestedPath;
  m_role  = Role::Standalone;
  return m_role;
}

bool InstanceManager::tryAcquireLock() {
  if (!m_lock) m_lock = new QSharedMemory(kSharedMemoryKey, this);

  if (m_lock->create(1)) return true;

  if (m_lock->error() == QSharedMemory::AlreadyExists) {
    // On Linux/macOS a crashed process can leave a stale segment attached;
    // attach+detach once to release it, then retry. On Windows the OS
    // already releases the segment as soon as the owning process dies, so
    // this branch normally means another mothership is genuinely alive.
    // Linux/macOSではクラッシュしたプロセスが残留セグメントを残すことが
    // あるため、一度attach+detachして解放を試みてから再試行する。Windows
    // ではプロセス終了時にOSが自動解放するため、通常この分岐に入るのは
    // 別の母艦が本当に稼働中の場合のみとなる。
    if (m_lock->attach()) m_lock->detach();
    if (m_lock->create(1)) return true;
  }
  return false;
}

void InstanceManager::startMothership(const QString& initialPath) {
  m_server = new QLocalServer(this);
  // Clear a stale socket file possibly left behind by a crashed mothership.
  QLocalServer::removeServer(kServerName);
  if (!m_server->listen(kServerName)) {
    // Could not start the IPC server; fall back to legacy behavior rather
    // than failing to launch at all.
    m_role = Role::Standalone;
    return;
  }
  connect(m_server, &QLocalServer::newConnection, this,
          &InstanceManager::onNewConnection);
  m_role = Role::Mothership;
  // Treat our own launch argument the same way as a request coming from
  // another process (always spawns a worker, since nothing is registered
  // yet at this point).
  handleOpenRequest(initialPath);
}

void InstanceManager::onNewConnection() {
  while (m_server->hasPendingConnections()) {
    QLocalSocket* socket = m_server->nextPendingConnection();
    m_recvBuffers.insert(socket, QByteArray());
    connect(socket, &QLocalSocket::readyRead, this,
            &InstanceManager::onServerSocketReadyRead);
    connect(socket, &QLocalSocket::disconnected, this,
            &InstanceManager::onServerSocketDisconnected);
  }
}

void InstanceManager::onServerSocketReadyRead() {
  QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
  if (!socket || !m_recvBuffers.contains(socket)) return;

  QByteArray& buffer = m_recvBuffers[socket];
  buffer.append(socket->readAll());

  Command cmd;
  QString path;
  while (tryReadFrame(buffer, cmd, path)) {
    if (cmd == Command::ClearRegistration) {
      // A worker's open-file set is about to be re-sent from scratch
      // (initial registration, or File > Open switched to a different
      // file / pair). Drop this socket's previous entries first so stale
      // paths don't keep pointing at it.
      QMutableMapIterator<QString, QLocalSocket*> it(m_workers);
      while (it.hasNext()) {
        it.next();
        if (it.value() == socket) it.remove();
      }
    } else if (cmd == Command::Register) {
      // Registers one path this worker currently has open. A paired
      // genga/douga sheet sends both paths, so either one resolves to this
      // window.
      m_workers.insert(PathUtils::canonicalizePath(path), socket);
    } else if (cmd == Command::Open) {
      // A short-lived forwarder is relaying an open request; it will
      // disconnect on its own right after this.
      handleOpenRequest(path);
    }
  }
}

void InstanceManager::onServerSocketDisconnected() {
  QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
  if (!socket) return;

  m_recvBuffers.remove(socket);

  // A registered worker window closed (or crashed): drop it from the
  // registry so a later "open" request for the same path spawns a new one.
  QMutableMapIterator<QString, QLocalSocket*> it(m_workers);
  while (it.hasNext()) {
    it.next();
    if (it.value() == socket) it.remove();
  }
  socket->deleteLater();

  // All windows are closed: the mothership has no reason to keep running.
  if (m_workers.isEmpty()) qApp->quit();
}

void InstanceManager::handleOpenRequest(const QString& path) {
  QString canonical = PathUtils::canonicalizePath(path);
  if (!canonical.isEmpty() && m_workers.contains(canonical)) {
    // Already open in a worker window: ask it to reload and come forward
    // instead of starting a new process.
    sendFrame(m_workers.value(canonical), Command::Reload, QString());
    return;
  }
  spawnWorker(path);
}

void InstanceManager::spawnWorker(const QString& path) {
  QStringList args;
  args << kWorkerFlag;
  if (!path.isEmpty()) args << path;
  QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
}

void InstanceManager::connectAsWorker() {
  m_workerSocket = new QLocalSocket(this);
  connect(m_workerSocket, &QLocalSocket::readyRead, this,
          &InstanceManager::onWorkerSocketReadyRead);
  m_workerSocket->connectToServer(kServerName);
  m_workerSocket->waitForConnected(1000);
  // If this fails (the mothership died between spawning us and connecting),
  // later sendFrame() calls (registration, notifyPathChanged) silently no-op;
  // the window still opens and behaves normally, it just won't participate
  // in reload/activate coordination.
  // 接続に失敗した場合（起動直後に母艦が終了した等）、このウィンドウは
  // 以後のreload/activate通知を受け取れなくなるが、ウィンドウ自体は通常
  // 通り開いて動作する。
}

void InstanceManager::notifyPathChanged(const QStringList& paths) {
  if (m_role != Role::Worker || !m_workerSocket) return;

  // Re-send this window's full open-path set from scratch: a paired
  // genga/douga sheet occupies two paths, so a plain "update one path"
  // message would leave the other one stuck pointing at whatever was
  // registered before.
  sendFrame(m_workerSocket, Command::ClearRegistration, QString());
  for (const QString& path : paths) {
    if (!path.isEmpty()) sendFrame(m_workerSocket, Command::Register, path);
  }
}

void InstanceManager::onWorkerSocketReadyRead() {
  m_workerRecvBuffer.append(m_workerSocket->readAll());

  Command cmd;
  QString path;
  while (tryReadFrame(m_workerRecvBuffer, cmd, path)) {
    if (cmd == Command::Reload) emit reloadAndActivateRequested();
  }
}

bool InstanceManager::sendFrame(QLocalSocket* socket, Command cmd,
                                const QString& path) {
  if (!socket) return false;

  // Length-prefixed framing: QLocalSocket is a byte stream with no message
  // boundaries, so a 4-byte size header precedes each payload. This also
  // keeps the protocol safe for paths containing unusual characters, unlike
  // a delimiter-based scheme.
  // QLocalSocketはバイトストリームでありメッセージの区切りを保証しない
  // ため、各ペイロードの前に4バイトの長さヘッダを付与する（フレーミング）。
  // 区切り文字方式と異なり、パスに特殊な文字が含まれていても安全。
  QByteArray payload;
  payload.append(static_cast<char>(cmd));
  payload.append(path.toUtf8());

  quint32 len = static_cast<quint32>(payload.size());
  QByteArray frame;
  frame.append(reinterpret_cast<const char*>(&len), sizeof(len));
  frame.append(payload);

  socket->write(frame);
  return socket->waitForBytesWritten(1000);
}

bool InstanceManager::tryReadFrame(QByteArray& buffer, Command& cmd,
                                   QString& path) {
  if (buffer.size() < static_cast<int>(sizeof(quint32))) return false;

  quint32 len;
  std::memcpy(&len, buffer.constData(), sizeof(len));
  if (buffer.size() < static_cast<int>(sizeof(quint32) + len)) return false;

  QByteArray payload = buffer.mid(sizeof(quint32), len);
  buffer.remove(0, sizeof(quint32) + len);

  if (payload.isEmpty()) return false;  // malformed frame; drop it

  cmd  = static_cast<Command>(static_cast<quint8>(payload.at(0)));
  path = QString::fromUtf8(payload.mid(1));
  return true;
}
