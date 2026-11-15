# ステップC：共有メモリ・通知・排他

確認する問いは次の2つ。

1. 別プロセスのclientが同じデータを更新するとき、共有メモリとMutexはそれぞれ何を担うか。
2. 更新完了を知らせるパルスと、共有メモリから読む値にはどのような関係があるか。

## 公式の根拠

- [shm_open](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/s/shm_open.html) / [mmap](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.sys_arch/topic/ipc_mmap.html)：名前付き共有メモリを開き、各プロセスのアドレス空間へマップする。
- [pthread_mutexattr_setpshared](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/p/pthread_mutexattr_setpshared.html)：共有メモリ内のMutexをPTHREAD_PROCESS_SHARED属性で初期化し、プロセス間で使う。
- [MsgSendPulse](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/m/msgsendpulse.html)：返信を待たない短い通知を送る。送信成功は受信側の処理完了を意味しない。

## 最小実験の構成

06の受信スレッド数比較から離れ、serverも各clientも1スレッドにする。
2つのclientプロセスが同じ共有データを更新し、serverが完了通知を受けて読む。

| 仕組み | この実装での役割 |
| --- | --- |
| shm_open + mmap(MAP_SHARED) | updatesとtotal、Mutex本体を同じ領域に置く |
| プロセス間Mutex | 更新回数と合計値の読み書きをひとまとまりで保護する |
| DONEパルス | clientが共有領域の使用を終えたことをserverへ知らせる |
| JOINのMsgSend / MsgReply | 初期化後の参加受付と反復回数の受け渡し |

serverは共有領域を新規作成し、サイズ設定・マップ・Mutex初期化・カウンタ初期化を終えてからサービス名を登録する。
clientはJOINに成功してから同じ名前の領域をマップする。各プロセスの仮想アドレスが同じである必要はない。
共有構造体にはプロセス固有のポインタを入れず、Mutexのコピーやclientによる再初期化もしない。

clientはEnter後、各反復でMutexをロックし、updatesを1、totalを12（7 + 5）増やしてアンロックする。
ループ全体を一度のロックで囲まないため、2プロセスの更新が途中で交代できる。
serverも同じMutexをロックして2つの値を取り出し、アンロック後にログを出す。
MAP_SHAREDだけでは複数項目の更新やread-modify-writeを排他できない。volatileもMutexの代わりにはならない。

各clientはアンロック・munmap後に1回だけDONEを送る。パルスの値は送信clientのPIDであり、合計値は運ばない。
serverは指定数の完了通知を受けると期待値を検証し、サービス登録解除・Mutex破棄・munmap・shm_unlinkを行って終了する。
JOINの参加上限を設け、実行中の追加参加はEBUSYで拒否する。
同じユーザー・同じバイナリの実験用プロセスだけで使う前提で、偽造・重複通知への認証は実装していない。

## ソースの読み方とコーディング規約

- protocol.hpp：通信形式、共有データの構造、回数上限。
- shared.hpp / shared.cpp：SharedMemoryの宣言と実装。create / attach / close、Mutex初期化、引数の数値検証。
- client.cpp：mainからrun_clientへ進み、join_server → attach → wait_for_start → update_shared_data → close → notify_completionの順に読む。
- server.cpp：mainで初期化後、run_server → receive_notificationsへ進む。通常要求はhandle_request、通知はhandle_pulseで処理し、最後にverify_final_snapshotで検証する。

適用するローカル規約と、MISRA C++適合確認で評価が必要なOS依存部分は[CODING_GUIDELINES.md](CODING_GUIDELINES.md)に記載した。
MISRA規約全文との照合・専用解析は未実施であり、正式なMISRA準拠とはしていない。

## ビルド・転送

SDP設定済みのWindows cmdで、リポジトリ直下から実行する。

```bat
call "%USERPROFILE%\qnx800\qnxsdp-env.bat"
call 07-shared-memory\build.cmd
```

ビルド成功後、QNX_IPを実際のIPへ置き換えて転送する。

```bat
ssh -m hmac-sha2-256-etm@openssh.com root@QNX_IP "mkdir -p /tmp/07-shared-memory"
scp -r -o MACs=hmac-sha2-256-etm@openssh.com 07-shared-memory/build root@QNX_IP:/tmp/07-shared-memory/
```

## 実測手順

QNX上でserver用、client A用、client B用の3端末を用意する。

1. server端末で参加数2、各clientの更新回数100000を指定する。

   ```sh
   /tmp/07-shared-memory/build/server 2 100000
   ```

2. client端末でそれぞれ起動し、両方のREADYを待つ。

   ```sh
   /tmp/07-shared-memory/build/client A
   ```

   ```sh
   /tmp/07-shared-memory/build/client B
   ```

3. AでEnter、続けてBでEnterを押す。clientのDONE queuedとserverのDONE / FINALを記録する。
   処理が短ければAが終了してからBが動くこともある。更新が重なる機会を増やすなら、serverの反復回数を10000000にして再実行する。

4. serverのFINALにPASSが出て正常終了することを確認する。
   正常終了後は同じコマンドで再実行できる。比較用にはserver 1 100000とclient 1個でも実行できる。

起動ログのexperiment、build、PID、clients、iterationsを確認し、古いバイナリとの混同を避ける。
clientはEnter待ちのREADY段階で参加済み。指定した全clientを最後まで実行する。

## 予想（実測ではない）

| 設定 | 最終updates | 最終total | 完了通知数 |
| --- | --- | --- | --- |
| server 1 100000 | 100000 | 1200000 | 1 |
| server 2 100000 | 200000 | 2400000 | 2 |

serverは最終値が両方とも一致すればPASS、違えばFAILを表示し、失敗時は終了コード1を返す。
途中のDONEログでもtotal = updates × 12が成り立つ予想。
ただし最初の通知を処理する前にもう一方のclientも更新を終えることがあるため、最初のsnapshotですでに最終値が見える場合もある。
パルスは「通知を送った時点のデータのコピー」ではなく、serverがロックした時点の最新値を読むきっかけになる。
受信パルスにはMsgReplyしない。JOINへの返信とは区別する。

この実験のPASSは設定した回数の更新結果の確認であり、実際に2つのclientが同時進行したことの証明や性能測定ではない。
ロックを外した実装はC++のデータ競合になるため、比較モードとして追加していない。

## 異常終了と再実行

Mutex保持中のプロセス終了からの復旧、client脱落時のタイムアウト、シグナルでの正常終了は今回の範囲に含めない。
clientが脱落するとserverは通知を待ち続け、Mutex保持中に終了すると他プロセスもロック待ちになり得る。
clientのDONE送信成功もserverの検証成功を保証しないので、serverのFINALを確認する。

Ctrl+Cなどで中断した場合は、この章のserverと全clientを終了してから、残った共有メモリ名をQNX上で削除する。

```sh
rm -f /dev/shmem/qnx-ipc-lab-shared
```

使用中の領域を消してserverを再起動しない。旧clientと新serverが別の領域を使う原因になる。
serverはO_EXCLで新規作成するため、残存領域がある場合はshm_openで失敗し、既存Mutexを上書きしない。
正常終了ではserverが領域を削除するので、この操作は不要。

## 進捗

- 実装：作成後、関数分割・所有権の明確化・入力検証・エラー処理の整理を実施。
- 整理後のクロスビルド：QNX SDP 8.0のq++（gcc_ntox86_64 / C++17）でserver/clientとも成功。上記規約の追加警告と-Werrorを有効にして警告・エラー出力なし。
- 整理前のQNX実測：ユーザー提示ログでserver 2 10000000のPASSを確認。updates=20000000、total=240000000。
- 整理後のQNX実測：未実施。同じコマンドで再確認する。
- ステップCの理解：共有メモリ・通知・Mutexの役割を説明し、複数clientからの加算結果を実測済み。

### 整理前の実測ログ（ユーザー提示）

```text
experiment=07-shared-memory build=Nov 15 2026 12:46:15 PID=1974305 clients=2 iterations=10000000 shm=/qnx-ipc-lab-shared
JOIN client_pid=1978402 participants=1/2
JOIN client_pid=1982499 participants=2/2
DONE client_pid=1982499 notifications=1/2 snapshot updates=18684894 total=224218728
DONE client_pid=1978402 notifications=2/2 snapshot updates=20000000 total=240000000
FINAL expected_updates=20000000 expected_total=240000000 PASS
```

記録する項目：clients、iterations、各PID、各DONE時点のupdates / total、FINAL、正常終了後の再実行結果。
