#include "instancemanager.h"
#include "pathutils.h"
#include "xdtsio.h"
#include "dialogs.h"
#include "macdockicon.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedMemory>
#include <QProcess>
#include <QMap>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QThread>

#include <cstring>

#ifdef WIN32
#include <windows.h>
#endif

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
      bool sent = sendFrame(&socket, Command::Open, requestedPath);
      // This process exits right after returning (the Forwarded role skips
      // a.exec() entirely in main.cpp), tearing down the pipe handle almost
      // immediately after the write above. On Windows this occasionally
      // races the mothership's asynchronous read of that same data: if the
      // pipe's "closing" notification is dispatched before the "data
      // arrived" one, the write can be silently lost even though it
      // reached the OS-level buffer. A brief pause here gives the
      // mothership's event loop time to actually read the frame before
      // this process's handle goes away.
      // このプロセスはreturn直後に終了する（Forwardedロールはmain.cppで
      // a.exec()自体を呼ばないため）ため、上のwrite直後にほぼ即座にパイプ
      // ハンドルが閉じられる。Windowsでは、母艦側の非同期読み取りとこれが
      // 稀に競合することがある: 「切断」の通知が「データ到着」の通知より
      // 先に配送されると、OSレベルのバッファには届いていた書き込みが
      // 静かに失われることがある。ここで一呼吸置くことで、このプロセスの
      // ハンドルが消える前に母艦のイベントループが確実にフレームを読み
      // 取れるようにする。
      if (sent) QThread::msleep(50);
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

  // The mothership has no MyWindow of its own; its only ever-visible
  // window is a transient CSP sync dialog. Qt's default
  // quitOnLastWindowClosed=true would otherwise quit the whole process
  // (killing the QLocalServer and orphaning every worker) the moment that
  // dialog closes. This process's lifetime is instead driven explicitly by
  // onServerSocketDisconnected() (quits once m_workers is empty).
  // 母艦はMyWindowを持たず、唯一表示されうるのはCSP同期用の一時的な
  // ダイアログのみ。Qtの既定であるquitOnLastWindowClosed=trueのままだと、
  // そのダイアログが閉じた瞬間にプロセス全体が終了してしまい
  // （QLocalServerも消え、全ワーカーが孤立する）。このプロセスの寿命は
  // 代わりにonServerSocketDisconnected()（m_workersが空になったら終了）で
  // 明示的に制御する。
  qApp->setQuitOnLastWindowClosed(false);

  // The mothership has no window, but on macOS every process that starts a
  // Cocoa event loop still gets a Dock icon by default; hide it so the app
  // doesn't show two Dock icons (one for this process, one for the worker
  // window it spawns).
  // 母艦はウィンドウを持たないが、macOSではCocoaのイベントループを開始した
  // 各プロセスは既定でDockアイコンを持つ。起動するワーカーウィンドウの
  // アイコンと並んで2つ表示されないよう、ここで隠す。
#ifdef __MACOS__
  hideDockIcon();
#endif

  // Treat our own launch argument the same way as a request coming from
  // another process (always spawns a worker, since nothing is registered
  // yet at this point).
  handleOpenRequest(initialPath);

  // If the initial request ended without spawning any worker (e.g. the user
  // cancelled the CSP sync dialog above), this invisible process has nothing
  // to manage and must not linger: setQuitOnLastWindowClosed(false) above
  // also disabled the implicit quit that used to end it, and a lingering
  // mothership would keep holding the shared-memory lock and the IPC pipe.
  // quit() called before exec() is ignored, so post it to run as soon as
  // exec() starts instead.
  // 最初の要求がワーカーを1つも起動せずに終わった場合（例:上のCSP同期
  // ダイアログをユーザーがキャンセルした場合）、この不可視プロセスには
  // 管理対象がなく、残留させてはならない。上のsetQuitOnLastWindowClosed
  // (false)により従来の暗黙終了も無効化されており、残留した母艦は共有
  // メモリロックとIPCパイプを握り続けてしまう。exec()前のquit()は無視
  // されるため、exec()開始直後に実行されるよう投函しておく。
  if (!m_spawnedAnyWorker && m_workers.isEmpty())
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
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
      QString registeredPath = PathUtils::canonicalizePath(path);
      m_workers.insert(registeredPath, socket);
      // Covers the case where this socket belongs to a worker just spawned
      // for a CSP link (performCspSync() couldn't notify it directly since
      // it wasn't registered yet at that point).
      // performCspSync()がCSP連携のために起動したばかりのワーカーは、その
      // 時点では未登録のため直接通知できない。そのケースをここでカバー
      // する。
      if (!m_linkedDestPath.isEmpty() && registeredPath == m_linkedDestPath) {
        m_linkedWorkerSocket = socket;
        sendFrame(socket, Command::LinkStatusChanged, QStringLiteral("1"));
      }
    } else if (cmd == Command::Open) {
      // A short-lived forwarder is relaying an open request; it will
      // disconnect on its own right after this. Defer the handling to the
      // event loop instead of calling it here: handleOpenRequest() can show
      // a modal CSP dialog, and that dialog's nested event loop delivers
      // this very socket's disconnected() signal while this slot is still
      // on the stack. onServerSocketDisconnected() then destroys `buffer`
      // and deletes `socket`, so resuming the loop below after the dialog
      // would touch freed memory and hang/crash the mothership (which then
      // silently stops serving IPC while its pipe still accepts connects).
      // 短命の転送プロセスがオープン要求を中継しており、送信直後に自分から
      // 切断してくる。ここで直接処理せず、イベントループへ遅延させる:
      // handleOpenRequest()はモーダルなCSPダイアログを表示することがあり、
      // そのダイアログのネストしたイベントループが、このスロットの実行中に
      // まさにこのソケットのdisconnected()を配送してしまう。すると
      // onServerSocketDisconnected()が`buffer`を破棄し`socket`を削除する
      // ため、ダイアログ後に下のループを再開すると解放済みメモリに触れ、
      // 母艦がハング／クラッシュする（パイプは接続を受け付けたまま、
      // IPCへの応答だけが沈黙する）。
      const QString requestedPath = path;
      QTimer::singleShot(0, this, [this, requestedPath]() {
        handleOpenRequest(requestedPath);
      });
    } else if (cmd == Command::Unlink) {
      // Explicit CSP unlink request from a worker window's menu action.
      if (m_linkedWorkerSocket == socket) {
        m_linkedWorkerSocket = nullptr;
        m_linkedDestPath.clear();
        sendFrame(socket, Command::LinkStatusChanged, QString());
      }
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

  // This window closing also breaks the CSP link if it was the linked one.
  // このウィンドウが閉じることで、それがCSP連携の紐づけ先だった場合は
  // 紐づけも解除される。
  if (m_linkedWorkerSocket == socket) {
    m_linkedWorkerSocket = nullptr;
    m_linkedDestPath.clear();
  }

  socket->deleteLater();

  // All windows are closed: the mothership has no reason to keep running.
  if (m_workers.isEmpty()) qApp->quit();
}

void InstanceManager::handleOpenRequest(const QString& path) {
  if (isCspExchangePath(path)) {
    handleCspSyncRequest(path);
    return;
  }

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
  m_spawnedAnyWorker = true;
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

void InstanceManager::requestUnlink() {
  if (m_role != Role::Worker || !m_workerSocket) return;
  sendFrame(m_workerSocket, Command::Unlink, QString());
}

void InstanceManager::onWorkerSocketReadyRead() {
  m_workerRecvBuffer.append(m_workerSocket->readAll());

  Command cmd;
  QString path;
  while (tryReadFrame(m_workerRecvBuffer, cmd, path)) {
    if (cmd == Command::Reload)
      emit reloadAndActivateRequested();
    else if (cmd == Command::LinkStatusChanged)
      emit cspLinkStatusChanged(!path.isEmpty());
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

  if (socket->write(frame) != frame.size()) return false;

  // write() for a small local-socket message often completes synchronously,
  // leaving nothing buffered to flush. waitForBytesWritten() has nothing to
  // wait for in that case and can return false immediately -- that must not
  // be mistaken for a failure, which is why bytesToWrite() is checked first.
  // write()はローカルソケット向けの小さなメッセージであれば同期的に完了
  // することが多く、その場合フラッシュすべきデータは残っていない。その
  // 状態ではwaitForBytesWritten()には待つべきものがなく即座にfalseを
  // 返しうるが、それを送信失敗と誤認してはいけないため、先に
  // bytesToWrite()を確認する。
  if (socket->bytesToWrite() == 0) return true;
  return socket->waitForBytesWritten(1000) || socket->bytesToWrite() == 0;
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
//-----------------------------------------------------------------------------
// CLIP STUDIO PAINT integration
//-----------------------------------------------------------------------------

namespace {
// Forces a just-shown top-level widget (e.g. a QMessageBox/QDialog) to the
// real foreground. The mothership has no window of its own and isn't the
// active process, so it hits the same Windows foreground-lock restriction
// worked around in MyWindow::reloadAndActivate() (mywindow.cpp): a Z-order
// change via the native HWND's topmost flag isn't subject to that
// restriction, unlike activateWindow()/SetForegroundWindow().
// 母艦プロセスにはウィンドウがなく、アクティブプロセスでもないため、
// mywindow.cppのreloadAndActivate()と同じWindowsフォアグラウンドロックの
// 制限を受ける。activateWindow()/SetForegroundWindowと異なり、ネイティブ
// HWNDの最前面フラグによるZ順序の変更はこの制限を受けない。
void forceToForeground(QWidget* widget) {
#ifdef WIN32
  widget->show();
  HWND hwnd = reinterpret_cast<HWND>(widget->winId());
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#endif
}

// Reads src's raw bytes and writes them to dst as-is (no XDTS
// parse/re-serialize), overwriting dst if it already exists.
// srcの生バイトをそのままdstへ書き込む（XDTSの解析・再シリアライズは
// 行わない）。dstが既に存在する場合は上書きする。
bool copyFileOverwrite(const QString& src, const QString& dst) {
  QFile in(src);
  if (!in.open(QIODevice::ReadOnly)) return false;
  QByteArray data = in.readAll();
  in.close();

  QFile out(dst);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  out.write(data);
  return true;
}
}  // namespace

// CSP always writes its export to this same fixed path (see
// "Windowsデジタルタイムシート連携方法.pdf"). Computed once via
// QDir::tempPath(), which matches the GetTempPath() API the document
// describes.
// CSPは常にこの固定パスへ書き出す（"Windowsデジタルタイムシート連携方法.pdf"
// 参照）。ドキュメントに記載のGetTempPath() APIに対応するQDir::tempPath()で
// 一度だけ計算する。
bool InstanceManager::isCspExchangePath(const QString& path) const {
  static const QString kCspExchangePath = PathUtils::canonicalizePath(
      QDir::tempPath() + "/ExchangeDigitalTimeSheet/animation.xdts");
  return !path.isEmpty() &&
         PathUtils::canonicalizePath(path) == kCspExchangePath;
}

bool InstanceManager::getXdtsMetadata(const QString& path, int& layerCount,
                                      int& duration) const {
  XdtsData data;
  if (!loadXdtsScene(&data, path)) return false;
  if (data.isEmpty() || data.timeTable().isEmpty()) return false;
  layerCount = data.timeTable().getCellHeader().getLayerNames().size();
  duration   = data.timeTable().getDuration();
  return true;
}

// One representative path per distinct open window (a paired genga/douga
// window contributes two entries to m_workers for the same socket).
// 開いているウィンドウ1つにつき代表パスを1つ返す（原画／動画ペアの
// ウィンドウは同じソケットに対しm_workersに2エントリを持つため）。
QStringList InstanceManager::currentlyOpenDestPaths() const {
  QStringList result;
  QSet<QLocalSocket*> seen;
  for (auto it = m_workers.constBegin(); it != m_workers.constEnd(); ++it) {
    if (seen.contains(it.value())) continue;
    seen.insert(it.value());
    result << it.key();
  }
  return result;
}

void InstanceManager::handleCspSyncRequest(const QString& animationXdtsPath) {
  int newLayerCount, newDuration;
  if (!getXdtsMetadata(animationXdtsPath, newLayerCount, newDuration)) {
    QMessageBox box(QMessageBox::Critical, tr("CSP Sync"),
                    tr("Failed to read the Clip Studio Paint export (%1).")
                        .arg(animationXdtsPath),
                    QMessageBox::Ok);
    forceToForeground(&box);
    box.exec();
    return;
  }

  bool hasActiveLink = (m_linkedWorkerSocket != nullptr);

  if (hasActiveLink) {
    int oldLayerCount, oldDuration;
    bool ok = getXdtsMetadata(m_linkedDestPath, oldLayerCount, oldDuration);
    // レイヤー数が2以上、またはdurationが6フレーム以上異なる場合は
    // 別のCSPプロジェクトの可能性があるとみなし、確認を挟む。
    bool differsSignificantly = !ok ||
                                qAbs(newLayerCount - oldLayerCount) >= 2 ||
                                qAbs(newDuration - oldDuration) >= 6;

    if (!differsSignificantly) {
      // Quiet auto-update: content looks like the same clip, no dialog.
      performCspSync(m_linkedDestPath, animationXdtsPath);
      return;
    }

    QMessageBox box(QMessageBox::Question, tr("CSP Sync"),
                    tr("The Clip Studio Paint export looks very different "
                       "from the currently linked file (%1). Continue "
                       "updating it, or choose a different file?")
                        .arg(QFileInfo(m_linkedDestPath).fileName()));
    QPushButton* continueBtn =
        box.addButton(tr("Continue"), QMessageBox::AcceptRole);
    QPushButton* differentBtn =
        box.addButton(tr("Choose Different File"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    forceToForeground(&box);
    box.exec();

    if (box.clickedButton() == continueBtn) {
      performCspSync(m_linkedDestPath, animationXdtsPath);
      return;
    }
    if (box.clickedButton() != differentBtn) return;  // Cancel: do nothing
    if (m_linkedWorkerSocket) {
      sendFrame(m_linkedWorkerSocket, Command::LinkStatusChanged, QString());
    }
    m_linkedWorkerSocket = nullptr;
    m_linkedDestPath.clear();
    // Fall through to the picker below.
  }

  CspLinkChooserDialog dlg(currentlyOpenDestPaths());
  forceToForeground(&dlg);
  if (dlg.exec() != QDialog::Accepted) return;

  QString destPath = dlg.resultPath();
  if (destPath.isEmpty()) return;

  if (dlg.needsOverwriteConfirm()) {
    QMessageBox confirm(
        QMessageBox::Question, tr("CSP Sync"),
        tr("This will overwrite %1 with the Clip Studio Paint export. "
           "Continue?")
            .arg(QFileInfo(destPath).fileName()),
        QMessageBox::Yes | QMessageBox::No);
    forceToForeground(&confirm);
    if (confirm.exec() != QMessageBox::Yes) return;
  }

  performCspSync(destPath, animationXdtsPath);
}

void InstanceManager::performCspSync(QString destPath,
                                     const QString& animationXdtsPath) {
  if (!copyFileOverwrite(animationXdtsPath, destPath)) {
    QMessageBox box(QMessageBox::Critical, tr("CSP Sync"),
                    tr("Failed to write %1.").arg(destPath), QMessageBox::Ok);
    forceToForeground(&box);
    box.exec();
    return;
  }

  QLocalSocket* previousLinkedSocket = m_linkedWorkerSocket;
  m_linkedDestPath                   = PathUtils::canonicalizePath(destPath);
  QLocalSocket* targetSocket = m_workers.value(m_linkedDestPath, nullptr);

  // Tell the previously linked window (if it's a different one, and still
  // open) that it's no longer linked.
  // 以前紐づいていたウィンドウ（destPathと異なり、まだ開いている場合）へ、
  // 紐づけが解除されたことを通知する。
  if (previousLinkedSocket && previousLinkedSocket != targetSocket) {
    sendFrame(previousLinkedSocket, Command::LinkStatusChanged, QString());
  }

  if (targetSocket) {
    m_linkedWorkerSocket = targetSocket;
    sendFrame(targetSocket, Command::Reload, QString());
    sendFrame(targetSocket, Command::LinkStatusChanged, QStringLiteral("1"));
  } else {
    // The new worker isn't registered yet; the Command::Register handler
    // in onServerSocketReadyRead() will notify it (and set
    // m_linkedWorkerSocket) once it connects.
    // 新しいワーカーはまだ未登録のため、onServerSocketReadyRead()内の
    // Command::Registerハンドラが、接続時に改めて通知する
    // （m_linkedWorkerSocketもそこで設定される）。
    m_linkedWorkerSocket = nullptr;
    spawnWorker(destPath);
  }
}
