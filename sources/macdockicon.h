#pragma once
#ifndef MACDOCKICON_H
#define MACDOCKICON_H

#ifdef __MACOS__
// Hides this process's Dock icon and Cmd+Tab entry
// (NSApplicationActivationPolicyAccessory). The invisible mothership
// process has no window of its own, but every process that starts a Cocoa
// event loop gets a Dock icon by default -- without this, launching XDTS
// Viewer shows two Dock icons (one for the mothership, one for the
// per-file worker window) instead of one. Safe to call at any time after
// QApplication is constructed; it does not prevent this process from
// later showing dialogs (e.g. the CSP sync picker) in the foreground.
// このプロセスのDockアイコンとCmd+Tabへの表示を隠す
// （NSApplicationActivationPolicyAccessory）。母艦プロセスは自身の
// ウィンドウを持たないが、Cocoaのイベントループを開始した各プロセスは
// 既定でDockアイコンを1つ持つため、これを行わないとXDTS Viewer起動時に
// Dockアイコンが2つ（母艦用とファイルごとのワーカーウィンドウ用）表示
// されてしまう。QApplication構築後であればいつ呼んでも良く、これを
// 呼んだ後もこのプロセスがCSP同期ピッカーなどのダイアログを前面に表示
// することは妨げられない。
void hideDockIcon();
#endif

#endif
