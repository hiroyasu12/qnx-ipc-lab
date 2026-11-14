# 第1章：QNXの同期メッセージパッシング

## 目的と今回の位置づけ

同じQNX OS上でサーバーとクライアントを別プロセスとして起動し、「7＋5」の要求に「12」を返信する最小のIPCを確認した。

今回使ったのは、QNXネイティブの**同期メッセージパッシング（Send／Receive／Reply）**。共有メモリやSOME/IPは使っていない。SSHはプログラムを起動・操作する入口であり、この要求・返信の通信経路ではない。

| QNXで使える代表的なIPC | 概念 | 今回 |
| --- | --- | --- |
| メッセージパッシング | 要求を送り、返信を待つ | **主な学習対象** |
| パルス | 少量の情報を持つ、返信不要の通知 | 接続終了時のOS通知の処理だけ含む |
| 共有メモリ | 同じメモリ領域を複数プロセスから使う | 未使用。排他・同期は別途設計する |
| POSIXメッセージキュー | メッセージをキューに積み、受信側が取り出す | 未使用 |
| パイプ／FIFO | バイト列を流して受け渡す | 未使用 |
| シグナル | プロセスやスレッドにイベントを知らせる | IPCの実験対象としては未使用 |
| ソケット | 通信の端点間でデータを送受信する | アプリ間通信には未使用 |

## 環境とファイル

- 開発ホスト：Windows、QNX SDP 8.0、VS Code＋QNX Toolkit
- ターゲット：VirtualBox上のQNX OS 8.0（x86_64）
- コンパイラ：SDP付属の`q++`。ターゲットは`gcc_ntox86_64`

| ファイル | 役割 |
| --- | --- |
| `protocol.hpp` | サービス名、要求種別、要求・返信のデータ構造を定義 |
| `server.cpp` | 名前と受信チャネルを登録し、受信・計算・返信を繰り返す |
| `client.cpp` | 名前で接続し、要求を送って返信結果を表示する |
| `build/server`、`build/client` | QNX用実行ファイル。Windowsでは実行しない |

## 動作の流れ

1. サーバーが`name_attach(nullptr, SERVICE_NAME, 0)`を呼ぶ。
   - 受信チャネルを作り、名前を`/dev/name/local/qnx-ipc-lab-add`に登録する。
   - サーバーは`attach->chid`を使って`MsgReceive()`で受信を待つ。
2. クライアントが`name_open(SERVICE_NAME, 0)`を呼ぶ。
   - 登録済みの名前を使ってサーバーへの接続を取得する。
   - 返ってきたコネクションID（`coid`）を、以降の送信に使う。
3. クライアントが`MsgSend()`で`MSG_ADD`、`7`、`5`を送る。
   - 要求データはサーバーの受信バッファへコピーされる。
   - 呼び出したスレッドは返信を待ってブロックする。
4. サーバーの`MsgReceive()`が戻る。
   - メッセージ種別とサイズを確認し、計算する。
   - 通常メッセージでは、返信に使う受信ID（`rcvid`）が返る。
5. サーバーが`MsgReply(rcvid, 0, &reply, sizeof(reply))`を呼ぶ。
   - 結果`12`がクライアントの返信バッファへ渡る。
   - クライアントは実行可能になり、スケジュールされると`MsgSend()`から戻る。
6. クライアントは結果を表示して接続を閉じ、終了する。
   - サーバーは次の要求を待ち続ける。

### 名前・種別・IDの違い

| 値 | 意味 | 使用箇所 |
| --- | --- | --- |
| `SERVICE_NAME` | どのサービスに接続するか | `name_attach()`、`name_open()` |
| `MSG_ADD` | 接続したサービスへ何の処理を頼むか | 要求データ内の`type` |
| `chid` | サーバーの受信チャネルID | `MsgReceive()` |
| `coid` | クライアントからサーバーへの接続ID | `MsgSend()` |
| `rcvid` | 受信した要求に返信するための識別子 | `MsgReply()`、`MsgError()` |

サービス名は接続を取得するときに使う。毎回の要求で名前検索するわけではない。サーバーが未起動などで名前が存在しなければ、`name_open()`は失敗する。

## 同期通信で「待つ」とは

| 状況 | 呼び出し元スレッドの状態 |
| --- | --- |
| 要求を送ったが、相手がまだ受信していない | SEND待ち |
| 相手が受信したが、まだ返信していない | REPLY待ち |
| 相手が返信した | 実行可能になり、CPUが割り当てられると再開 |

**受信されたことと、返信されたことは別。** 通常の`MsgSend()`は、相手が受信しただけでは戻らない。

- 待つのは呼び出したスレッド。同じプロセスの別スレッドは動ける。
- 待機中にCPUを使ってポーリングし続けるわけではない。
- 今回はタイムアウト未設定。サーバーが生きたまま返信を忘れると、待ち続ける可能性がある。
- サーバー終了などのエラーでも`MsgSend()`は戻り得るため、戻り値の確認が必要。

## データの受け渡しで確認したこと

```cpp
const int status = MsgSend(
    coid,
    &request, sizeof(request),
    &reply, sizeof(reply));
```

- `request`は送るデータ、`reply`は返信を受け取る領域。
- サーバーへクライアントのメモリを直接公開する仕組みではない。
- 今回は`MsgReply()`の第2引数を`0`にしたため、成功時の`status`は`0`。
- **計算結果の`12`は戻り値ではなく、`reply.result`に入る。**
- `protocol.hpp`は双方でバイト列の解釈を合わせる約束。QNXが構造体の意味や「足し算」を解釈するわけではない。
- この例は同じABIでビルドした単純な構造体を送る。ポインタや`std::string`の内部表現をそのまま送る方式には拡張しない。

サーバーは通常メッセージ以外も受ける。`rcvid == 0`はパルス、`rcvid == -1`は受信エラー。接続終了通知のパルスには`MsgReply()`せず、必要な接続の後始末をする。

## ビルドと実行

### Windows側：コマンドプロンプトでビルド

`01-message`フォルダで実行する。VS Codeの端末がPowerShellなら、先に`cmd`でコマンドプロンプトへ入る。

```bat
call "%USERPROFILE%\qnx800\qnxsdp-env.bat"
if not exist build mkdir build
q++ -Vgcc_ntox86_64 -std=c++17 -D_QNX_SOURCE -g -O0 -Wall -Wextra -o build/server server.cpp
q++ -Vgcc_ntox86_64 -std=c++17 -D_QNX_SOURCE -g -O0 -Wall -Wextra -o build/client client.cpp
```

`-D_QNX_SOURCE`は、標準C++以外のPOSIX／QNXのAPI宣言も利用するために指定する。今回、これがないと`getpid()`が未宣言になった。

また、受信情報の`info.msglen`と`sizeof(...)`の符号が異なる比較では警告が出たため、固定サイズ側をメンバーの型へ変換した。

```cpp
info.msglen < static_cast<decltype(info.msglen)>(sizeof(AddRequest))
```

### Windows側：転送とSSH接続

以下の`QNX_IP`は、実際のターゲットIPに置き換える。今回使用した学習用VMのログインユーザーは`root`。

```sh
scp -r -o MACs=hmac-sha2-256-etm@openssh.com ./build root@QNX_IP:/tmp/
ssh -m hmac-sha2-256-etm@openssh.com root@QNX_IP
```

この環境では通常のSSH接続で`Corrupted MAC on input`が出たため、MAC方式を明示して接続できた。これは今回のSSHクライアントとの接続設定であり、QNXのIPCには関係しない。SFTP関連で転送に失敗する環境では、`scp`に`-O`を追加して従来方式を使う。

### QNX側：端末を2つ使って実行

SSHで同じQNXへ2回接続する。

端末A：

```sh
cd /tmp/build
chmod +x server client
./server
```

端末B：

```sh
cd /tmp/build
./client
```

クライアントで`status=0 result=12`が表示されることを確認した。クライアントを再実行すると、同じサーバーが再び処理する。終了時はサーバー側で`Ctrl+C`を押す。

## 今回の到達点と次の実験

別プロセス間で、名前による接続、要求データの送信、受信、返信まで実行できた。**QNXの同期メッセージ通信では、データの受け渡しと呼び出し元の待機・再開が組み合わされている**ことを理解した。

次はサーバーの返信を遅らせ、`pidin`でクライアントのREPLY待ちを観察する。サーバー終了時の挙動とタイムアウトは、その後に扱う。

## 参考資料

- [QNX OS 8.0：Synchronous message passing](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.sys_arch/topic/ipc_Sync_messaging.html)
- [QNX OS 8.0：name_attach()](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/n/name_attach.html)
- [QNX OS 8.0：name_open()](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/n/name_open.html)
