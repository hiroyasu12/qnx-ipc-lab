# ステップB：複数clientとserverの受信スレッド数

確認する問いは「同期通信で2つのclientを扱うとき、serverの受信スレッド数は何を変えるか」。
同じバイナリを server 1 / server 2 で起動し、返信前に10秒待つ足し算要求を比較する。

## 公式の根拠

- [Channels and connections](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.sys_arch/topic/ipc_Channels.html)：通信先はチャネル。複数のスレッドで同じチャネルを受信できる。
- [MsgReceive](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/m/msgreceive.html) / [MsgSend](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/m/msgsend.html)：受信前の送信側はSEND待ち、受信後は返信までREPLY待ちになる。
- [name_attach](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/n/name_attach.html)：登録した名前のchidをMsgReceiveに使う。切断・解除要求のパルスも届く。

## 構成

- protocol.hpp：7 + 5要求と64bitの結果。サービス名は qnx-ipc-lab-concurrency。
- server.cpp：name_attachは1回。mainも受信し、2モードだけpthreadを1つ追加する。受信ループは共通で、同じchidを使う。
- client.cpp：各プロセスが1回接続し、Enter後に1回MsgSendする。
- log.hpp：CLOCK_MONOTONICの秒、PID、TIDを表示する。monoは同じVM内で比較できる時刻で、暦時刻ではない。
- build.cmd：この章だけをビルドする。

受信バッファ・rcvid・要求・返信はスレッドごとのローカル変数。共有する設定は読み取り専用。
ログの1行だけstdoutをロックする。10秒の待ちをロックで囲まない。
10秒のnanosleepは遅い処理の代用で、CPUを使い続ける計算負荷の実験ではない。
タイマー通知、共有データの更新、優先度継承の詳細、TimerTimeoutは今回の比較に含めない。

name_openの前後をログに出し、両clientがREADYになるまで送信しない。
これにより、後から起動したclientが接続処理で待つ現象を足し算要求のSEND待ちと混同しない。

解除要求パルスには別スレッドからMsgErrorしない。元要求を受信したスレッドが10秒後に通常返信する方針とし、二重完了を避ける。
この実験は即時キャンセルを実装していない。待機中のclientへのシグナルで即座に復帰するとは限らない。

## ビルド・転送

SDP設定済みのWindows cmdで、リポジトリ直下から実行する。

```bat
call "%USERPROFILE%\qnx800\qnxsdp-env.bat"
call 06-concurrency\build.cmd
```

成功した場合だけ、QNX_IPを実際のIPに置き換えて転送する。

```bat
ssh -m hmac-sha2-256-etm@openssh.com root@QNX_IP "mkdir -p /tmp/06-concurrency"
scp -r -o MACs=hmac-sha2-256-etm@openssh.com 06-concurrency/build root@QNX_IP:/tmp/06-concurrency/
```

## 実測手順

QNX側にserver用、client A用、client B用の3端末を用意する。
状態も見る場合は4つ目を使う。同じサービス名のserverは同時に起動しない。

1. 端末Sで1受信スレッドのserverを起動する。

   ```sh
   /tmp/06-concurrency/build/server 1
   ```

2. 端末AとBで、それぞれclientを起動する。両方にREADYが表示されるまでEnterを押さない。

   ```sh
   /tmp/06-concurrency/build/client A
   ```

   ```sh
   /tmp/06-concurrency/build/client B
   ```

3. AでEnter、すぐにBでEnterを押す。10秒の処理中に両要求を送る。
   serverのBEGIN / ENDの時刻・TID・client_pidと、各clientのelapsedを記録する。
   どちらが先に受信されるかはログで判断する。

4. 必要なら処理中に4つ目の端末で観察する。

   ```sh
   pidin -p server
   pidin -p client
   pidin -l -d 5 -p client
   ```

   名前が同じ別章のプロセスがある場合は、ログのPIDを指定する。
   clientはREADY後のEnter待ちと、MsgSend中の待ちを区別する。

5. 両clientが戻ってから端末SでCtrl+Cでserverを終了する。
   server 2で再起動し、手順2〜4を繰り返す。

   ```sh
   /tmp/06-concurrency/build/server 2
   ```

起動ログのexperiment、build、receiversを確認する。2モードでは同じPIDに異なるTIDのreceiver readyが2行出る。
mainも受信スレッドなので、1モードの受信スレッド数は1、2モードは2。
Ctrl+Cではプロセスが終了し、OSがチャネル・スレッド等を回収する。

## 予想（実測ではない）

| 観点 | server 1 | server 2 |
| --- | --- | --- |
| serverのログ | BEGIN → 約10秒 → END → 次のBEGIN → 約10秒 → END | 別TIDで2つのBEGIN、その約10秒後に各END |
| clientの時間 | 先の要求は約10秒、後の要求は約20秒から送信時刻差を引いた時間 | 各要求がおおむね10秒 |
| 処理中のclient | 先はREPLY、後は受信されるまでSEND | 両方が受信された後はREPLY |
| serverの状態 | 処理中は1受信スレッドがNANOSLEEP | 処理中は2受信スレッドがNANOSLEEP |

1モードでは2つ目がまだ受信されず、2モードでは2つ目も受信されて返信を待てる、という差を見る。
ENDはMsgReply成功後のログなので、相手のclientがENDを先に表示することもある。
時刻、表示順、担当TID、手動操作の時間差、スケジューリングにより値は変わる。厳密な性能測定には使わない。
同期通信が待たせるのは要求したclientスレッドであり、serverの接続相手の数を1つに制限しない。
複数の処理待ちが重なることを観察する実験で、複数CPU上で同時に命令が動くことの証明ではない。

## 進捗

- 実装：作成済み。
- クロスビルド：QNX SDP 8.0のq++（gcc_ntox86_64 / C++17 / -Wall -Wextra）でserver/clientともに成功。警告・エラー出力なし。
- QNX実機での1 / 2スレッド比較：未実測。
- ステップBの学習完了：比較結果と理解の確認後に判断する。

記録する項目：モード、各clientのPID・送信時刻・elapsed・result、各要求のserver TID・BEGIN / END時刻、観察できたSTATE。