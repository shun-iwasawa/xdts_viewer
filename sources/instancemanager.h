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
class QEvent;

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

  // Clears the CSP link if it currently points at this worker window.
  // No-op unless this process's role is Worker. Non-destructive (only
  // affects future CSP syncs), so callers need not confirm before calling.
  // このワーカーウィンドウがCSP連携の紐づけ先である場合、その紐づけを
  // 解除する。役割がWorker以外の場合は何もしない。非破壊的な操作
  // （以後のCSP同期にのみ影響）なので、呼び出し前の確認は不要。
  void requestUnlink();

  // macOS only: Finder/`open` file-open requests for a file other than the
  // one this process was launched with arrive as a QFileOpenEvent on the
  // running QApplication instead of a fresh process with a new argv (macOS's
  // Launch Services treats the bundle as already running and does not start
  // a second process). Installed as an application-wide event filter by
  // main.cpp so it sees these regardless of which existing process (the
  // mothership or a worker) macOS happens to deliver them to.
  // macOS専用: 起動時に渡されたのとは別のファイルに対するFinder/`open`の
  // オープン要求は、新しいargvを持つ新規プロセスとしてではなく、既存の
  // QApplicationインスタンスへのQFileOpenEventとして届く（macOSの
  // Launch Servicesはバンドルを既に実行中とみなし、2つ目のプロセスを
  // 起動しないため）。macOSがどのプロセス（母艦かワーカーか）にこれを
  // 配送するかに関わらず受け取れるよう、main.cppからアプリケーション
  // 全体のイベントフィルタとしてインストールされる。
  bool eventFilter(QObject* watched, QEvent* event) override;

signals:
  // Worker role only: emitted when the mothership asks this window to
  // reload its XDTS data and come to the foreground, because another
  // launch requested the file already open here.
  // Worker側でのみ発火する。別の起動がこのウィンドウで既に開いている
  // ファイルを要求したため、母艦からXDTSデータの再読み込みと前面化を
  // 指示された際にシグナルされる。
  void reloadAndActivateRequested();

  // Worker role only: emitted when the mothership reports that this
  // window's CSP link status changed (established, moved to another
  // window, or explicitly/implicitly unlinked).
  // Worker側でのみ発火する。このウィンドウのCSP連携の紐づけ状態が変化
  // した（成立・他ウィンドウへの移動・解除のいずれか）ことを母艦から
  // 通知された際にシグナルされる。
  void cspLinkStatusChanged(bool linked);

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
    ClearRegistration = 3,
    Unlink            = 4,
    LinkStatusChanged = 5
  };

  bool tryAcquireLock();
  void startMothership(const QString& initialPath);
  void connectAsWorker();
  void handleOpenRequest(const QString& path);
  bool sendFrame(QLocalSocket* socket, Command cmd, const QString& path);
  static bool tryReadFrame(QByteArray& buffer, Command& cmd, QString& path);
  void spawnWorker(const QString& path);

  // CLIP STUDIO PAINT integration: CSP always writes its export to the
  // same fixed path and launches XDTS Viewer with that path as argv[1].
  // This is treated as a request to sync a real XDTS file (m_linkedDestPath)
  // rather than a request to open that fixed path directly.
  // CLIP STUDIO PAINT連携: CSPは常に同一の固定パスへ書き出し、そのパスを
  // 引数にXDTS Viewerを起動する。これを固定パス自体を開く要求としてではなく、
  // 実体のあるXDTSファイル（m_linkedDestPath）を同期する要求として扱う。
  bool isCspExchangePath(const QString& path) const;
  void handleCspSyncRequest(const QString& animationXdtsPath);
  // destPath is taken by value (not const&): callers sometimes pass
  // m_linkedDestPath itself, and this function reassigns m_linkedDestPath
  // partway through, so a reference parameter would alias it.
  // destPathは値渡し（const参照ではない）: 呼び出し元がm_linkedDestPath
  // 自身を渡すことがあり、本関数の途中でm_linkedDestPathを再代入する
  // ため、参照渡しだとエイリアシングが発生してしまう。
  void performCspSync(QString destPath, const QString& animationXdtsPath);
  bool getXdtsMetadata(const QString& path, int& layerCount,
                       int& duration) const;
  QStringList currentlyOpenDestPaths() const;

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

  // CLIP STUDIO PAINT integration: canonical path of the XDTS file
  // currently linked to CSP's export, and the worker socket that owns it
  // (nullptr if unlinked). m_linkedWorkerSocket is the authoritative
  // "is there an active link" signal (a plain pointer check, kept in sync
  // eagerly on link/unlink/disconnect) -- m_linkedDestPath is only for
  // display and metadata comparison.
  // CLIP STUDIO PAINT連携: CSPの書き出しに現在紐づいているXDTSファイルの
  // 正規化パスと、それを所有するワーカーのソケット（未紐づけならnullptr）。
  // 「現在紐づけ中か」の判定はm_linkedWorkerSocketの単純なポインタ判定を
  // 正とし（紐づけ／解除／切断のたびに即時更新する）、m_linkedDestPathは
  // 表示とメタデータ比較のためだけに使う。
  QString m_linkedDestPath;
  QLocalSocket* m_linkedWorkerSocket = nullptr;

  // Whether this mothership has ever spawned a worker process. Used only
  // by startMothership() to detect the "initial request ended with nothing
  // to manage" case (see the comment there).
  // この母艦がワーカープロセスを一度でも起動したかどうか。startMothership()
  // で「最初の要求が管理対象なしで終わった」場合の検出のみに使う
  // （詳細はそちらのコメントを参照）。
  bool m_spawnedAnyWorker = false;
};

#endif
