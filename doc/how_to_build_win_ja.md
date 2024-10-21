# ビルド手順（Windows）

Visual Studio 2022 と Qt 5.15 でビルドできることを確認しています。

## 必要なソフトウェアの導入

### Visual Studio Community 2022
- https://visualstudio.microsoft.com/ja/vs/
- C++ によるデスクトップ開発の環境をインストールします。

### CMake
- https://cmake.org/download/
- Visual Studio 用のプロジェクトファイルの生成に使用します

### Qt
- https://www.qt.io/download-open-source/
- クロスプラットフォームの GUI フレームワークです
- 上記の URL から以下のファイルをダウンロードして Qt 5.15 (64 ビット版) を適当なフォルダにインストールします
  - [Qt Online Installer for Windows](http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe)

#### WinTabサポート付きカスタマイズ版 Qt5.15.2
- Qtは5.12以降Windows Ink APIをネイティブで使用しています。5.9まで使用されていたWinTab APIとはタブレットの挙動が異なり、それによる不具合が報告されています。
- そこで、公式には6.0から導入されるWinTab APIへの切り替え機能をcherry-pickしたカスタマイズ版の5.15.2を頒布しています。
- MSVC2019-x64向けのビルド済みパッケージは [こちら](https://github.com/shun-iwasawa/qt5/releases/tag/v5.15.2_wintab) から入手できます。

## ソースコードの取得
- 本リポジトリを `git clone` します
- 以下の説明中の `$xdts_viewer` は、本リポジトリの root を表します
- Visual Studio は BOM の無い UTF-8 のソースコードを正しく認識できず、改行コードが LF で、1行コメントの末尾が日本語の場合に、改行が無視されて次の行もコメントとして扱われる問題があるため、Git に下記の設定をして改行コードを CRLF に変換すると良いでしょう
  - `git config core.safecrlf true`

## ビルド

### CMake で Visual Studio のプロジェクトを生成する
1. CMake を立ち上げる
2. Where is the source code に `$xdts_viewer/sources` を指定する
3. Where to build the binaries に `$xdts_viewer/build` を指定する
  - 他の場所でも構いません
  - チェックアウトしたフォルダ内に作成する場合は、buildから開始するフォルダ名にするとgitから無視されます
  - ビルド先を変更した場合は、以下の説明を適宜読み替えてください
4. Configure をクリックして、 Visual Studio 17 2022 Win64 を選択します
5. Qt のインストール先がデフォルトではない場合、 `Specify QTDIR properly` というエラーが表示されるので、 `QTDIR` に Qt5 をインストールしたパスを指定します
6. Generate をクリック
    - CMakeLists.txt に変更があった場合は、ビルド時に自動的に処理が走るので、以降は CMake を直接使用する必要はありません

## ビルド
1. `$xdts_viewer/build/xdts_viewer.sln` を開いて Release 構成を選択してビルドします
2. `$xdts_viewer/build/Release` にファイルが生成されます

## 実行
### 実行可能ファイルなどの配置
1. `$xdts_viewer/build/Release` の中身を適当なフォルダにコピーします
3. `xdts_viewer.exe`のパスを引数にして Qt に付属の `windeployqt.exe` をそれぞれ実行します
    - 必要な Qt のライブラリなどが `xdts_viewer.exe` と同じフォルダに集められます

### リソースフォルダのコピー
- `xdts_viewer.exe`と同じフォルダに`$xdts_viewer/xdts_viewer_resources`をフォルダごとコピーします。このフォルダにはXDTS Viewerの動作に必要な各種設定、翻訳ファイル、タイムシートのテンプレートなどが入っています。

### 実行
xdts_viewer.exe を実行して動作すれば成功です。おめでとうございます。

## 翻訳ファイルの生成
Qt の翻訳ファイルは、ソースコードから `.ts` ファイルを生成して、 `.ts` ファイルに対して翻訳作業を行い、 `.ts` ファイルから `.qm` ファイルを生成します。Visual Studioソリューション中の`translation_`から始まるプロジェクトに対して「 `translation_???` のみをビルド」を実行すると、 `.ts` ファイルと `.qm` ファイルの生成が行われます。これらのプロジェクトはソリューションのビルドではビルドされないようになっています。
