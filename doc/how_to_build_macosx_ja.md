# ビルド手順（MacOS）

## 必要なソフトウェア

- git
- brew
- Xcode
- cmake (3.2.2以降)
- Qt5 (5.15以降)

## ビルド手順

### Xcode をインストール

### Homebrew をインストール

1. ターミナルウィンドウを起動
2. 下記を実行します：
```
$ /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
```

### brew で必要なパッケージをインストール

```
$ brew install cmake qt@5
```

### リポジトリを clone
1. GitHub上でhttps://github.com/opentoonz/xdts_viewerをフォークします。
1. 下記を実行します：

```
$ git clone https://github.com/[Githubのユーザー名]/xdts_viewer.git
$ cd xdts_viewer
```

### 本体のビルド

1. buildディレクトリの作成
```
$ mkdir build
$ cd build
```

2. ビルド
コマンドラインの場合は下記を実行します。
```
$ cmake ../sources -DQTDIR='/usr/local/opt/qt@5'  #replace QT path with your installed QT version#
$ make
```
- Qt をHomebrewでなく http://download.qt.io/official_releases/qt/ からダウンロードして `/Users/ユーザ名/Qt` にインストールしている場合、`QT_PATH`の値は `~/Qt/5.15.2/clang_64/lib` または `~/Qt/5.15.2/clang_32/lib` のようになります。

Xcodeを用いる場合は下記を実行します。
```
$ sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
$ cmake -G Xcode ../sources -B. -DQTDIR='/usr/local/opt/qt@5' -DWITH_TRANSLATION=OFF
```
- オプション `-DWITH_TRANSLATION=OFF` はXcode12以降で必要です。
- Xcodeでプロジェクト `/Users/yourlogin/Documents/xdts_viewer/build/xdts_viewer.xcodeproj` を開き、ビルドします。

### ライブラリの準備
1. 下記を実行します。
```
$ cd build #or build/Release or build/Debug, where xdts_viewer.app has generated#
$ /usr/local/opt/qt@5/bin/macdeployqt xdts_viewer.app -verbose=1 -always-overwrite 
```
    - 必要なQtライブラリがxdts_viewerバイナリと同じフォルダにコピーされます。
 
### リソースフォルダのコピー

- `$xdts_viewer/xdts_viewer_resources`　をフォルダごと `xdts_viewer.app/Contents/MacOS` 内にコピーします。

### アプリケーションの実行

```
$ open ~/Documents/xdts_viewer/build/xdts_viewer.app
```

- Xcode でビルドしている場合、アプリケーションは　`/Users/yourlogin/Documents/xdts_viewer/build/Debug/xdts_viewer.app` にあります。