# ステップA：タイマーのパルスと通常要求

目的は「返信が必要な要求」と「返信不要の通知」を、同じMsgReceiveループで比較すること。
serverは1スレッドで、2秒周期のタイマー通知とclientの7 + 5要求を処理する。

## 公式の根拠

- [MsgReceive](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/m/msgreceive.html)：通常メッセージとパルスを受信する。パルスの戻り値は0で、パルス自体には返信しない。
- [sigevent](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/s/sigevent.html)：SIGEV_PULSE_INT_INITで宛先coid・優先度・code・整数valueを設定する。
- [timer_create](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/t/timer_create.html) / [timer_settime](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/t/timer_settime.html)：タイマーを作成し、初回満了と繰り返し周期を設定する。
- [ConnectAttach](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/c/connectattach.html)：自分のチャネルに通知用接続を作る。_NTO_SIDE_CHANNELを使う。

## 確認する問いと予想（実験前の予想）

1. clientがなくても通知を受け取れるか。
   - 予想：server単独でおおむね2秒ごとにtimer pulseが表示される。rcvid=0で、返信しなくても次の通知を受け取れる。
2. 同じ受信ループで足し算要求も処理できるか。
   - 予想：client起動時にrcvid > 0の要求が届き、MsgReply後にclientがresult=12で戻る。その前後にもタイマー通知が続く。

## 構成と既存実験からの変更

- server.cpp：name_attach → 自チャネルへのConnectAttach → SIGEV_PULSE_INT_INIT → timer_create → timer_settime → 共通受信ループ。
- client.cpp：足し算要求を1回だけ送信する。今回はタイムアウト実験を含めない。
- protocol.hpp：足し算の要求・返信型。サービス名はqnx-ipc-lab-pulseにして既存章と区別する。
- build.cmd：このディレクトリのserver/clientをbuild/にビルドする。転送はしない。

04-timeoutの20秒sleepは使わない。解除要求パルスの処理は後始末として残し、今回観察する通知は独自codeのタイマーパルスに絞る。
タイマー用のアプリスレッドは追加しない。CLOCK_MONOTONICを使い、初回・周期ともに2秒。
value=42は固定の識別値で、時刻や回数ではない。receivedはserverが処理したタイマーパルスの累計で、満了回数の厳密な測定には使わない。

## ビルド・転送

SDP環境を設定したWindowsのcmdで、リポジトリ直下から実行する。

```bat
call "%USERPROFILE%\qnx800\qnxsdp-env.bat"
call 05-pulse\build.cmd
```

ビルドが成功した場合だけ転送する。次のQNX_IPを実際のIPへ置き換える。
既存の/tmp/buildと区別するため、転送先を/tmp/05-pulseにする。

```bat
ssh -m hmac-sha2-256-etm@openssh.com root@QNX_IP "mkdir -p /tmp/05-pulse"
scp -r -o MACs=hmac-sha2-256-etm@openssh.com 05-pulse/build root@QNX_IP:/tmp/05-pulse/
```

計画書に記載されたルートのbuild-deploy.cmdは、実装時のワークスペースには存在しなかったため、この章専用のbuild.cmdを用意した。

## 実測手順

1. QNXの端末Aでserverを起動し、clientなしで通知を3回ほど確認する。

   ```sh
   /tmp/05-pulse/build/server
   ```

2. serverを動かしたまま、QNXの端末Bでclientを実行する。

   ```sh
   /tmp/05-pulse/build/client
   ```

3. clientのstatus=0 / result=12と、serverのadd request / repliedを確認する。その後もtimer pulseが続くかを見る。
4. 必要ならclientをもう一度実行し、同じserverで応答できるか確認する。
5. 端末AでCtrl+Cを押して終了する。通常のCtrl+Cはプロセス終了となり、OSがリソースを回収する。コード末尾の明示的解放は受信エラーでループを抜けた経路で実行される。

予想ログの一部（実測ログではない。rcvidの数値や表示順は固定しない）：

```text
[server] timer pulse: rcvid=0 code=... value=42 received=1 (no reply)
[server] add request: rcvid=... 7 + 5
[server] replied: result=12
[server] timer pulse: rcvid=0 code=... value=42 received=2 (no reply)
[client] MsgSend returned: status=0 result=12
```

タイマーのパルスも通常要求も、serverがMsgReceiveで取り出してからこのスレッドで処理する。
ログの間隔は厳密な2秒を保証しない。今回は遅延・負荷・優先度の比較までは行わない。

## 進捗と到達点

- 実装：作成済み。
- クロスビルド：QNX SDP 8.0のq++（gcc_ntox86_64 / C++17 / -Wall -Wextra）でserver/clientともに成功。警告・エラー出力なし。
- QNX上での実行：ユーザーから、周期パルス・clientへの応答・切断通知の出力を確認したとの報告あり。生ログはこの会話には未提示のため、具体的な数値・表示順・周期精度は検証済みとしない。
- ステップA：ユーザーの理解確認と終了の申し出により完了。

理解確認：同じMsgReceiveでパルスとMsgSendによる通常要求を受信し、処理を分岐する流れを理解したとの報告あり。
学習内容：rcvid=0は返信不要のパルス、rcvid > 0は返信可能な通常要求。何も届かない間はMsgReceive内で待ち、-1はエラーを表す。パルスはcodeと小さなvalueを持ち、ConnectAttachは自己接続にも別プロセスへの接続にも使える。タイマー・sigeventの引数の細部は、必要時に参照する扱いとした。
