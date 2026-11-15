# QNX IPC Lab

QNX OS 8.0でIPCを実験し、学習結果を記録するリポジトリ。

## 環境
- 開発ホスト: Windows
- 開発環境: QNX SDP 8.0 / VS Code / QNX Toolkit
- ターゲット: VirtualBox上のQNX OS 8.0（x86_64）

## 学習項目
- メッセージパッシング
- パルス
- 共有メモリと同期
- その他のIPC

## ステップC

[07-shared-memory](07-shared-memory/README.md)：別プロセスの共有メモリ更新をプロセス間Mutexで保護し、完了をパルスで通知する。
実装・QNX向けクロスビルド済み。整理前の実装はQNX上で2client・各1000万回の加算を実測済み。可読性整理後の実機再確認は未実施。

## ステップD

[08-resource-manager](08-resource-manager/README.md)：open / devctlで足し算サービスを呼び、アプリAPI・Proxy／Stubの役割・QNXメッセージ通信を対応付ける。IPC方式の比較も整理。
実装・QNX向けクロスビルド済み。QNX上での正常系・異常系の実行確認は未実施。

## 追加の学習計画

[FIDLとPOSIX IPCの学習計画・進捗](IPC_LEARNING_PLAN.md)に沿って、以下の順に学ぶ。

1. [09：FIDLと手書きProxy／Stub](09-fidl-flow/README.md)で、契約からQNXメッセージと足し算までを追う。
2. [同じFIDLのCommonAPI生成物](09-fidl-flow/GENERATED.md)で、実際の生成APIと手書きの役割を対応付ける。
3. [10：POSIXメッセージキュー](10-posix-mqueue/README.md)で、送信成功と計算完了を分ける。
4. [11：pipe](11-pipe/README.md)で、バイト列・改行による区切り・EOF・fdの継承を調べる。
5. [12：FIFO](12-fifo/README.md)で、独立プロセスの接続とopenの待機を調べる。

全章を実装し、QNX x86_64へのクロスビルドとQNX VMでの正常系・比較実験を確認済み。
Core生成も実施済み。CommonAPI bindingによる通信は計画どおり別枠の発展課題。
全章のビルドは`build-learning.cmd`、[配置・実測ログ・一括検証手順](verification/README.md)も用意した。
