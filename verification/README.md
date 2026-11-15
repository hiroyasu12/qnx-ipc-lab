# 09〜12のビルド・配置・実測

2026-11-16、WindowsのQNX SDP 8.0からx86_64へクロスビルドし、指定されたVMで実行した。
QNX 8.0.0 x86_64、mqueueとpipe managerの稼働を確認済み。
Core generatorの生成・観察は[09の生成記録](../09-fidl-flow/GENERATED.md)へ分けた。

| 学習順 | 検証した内容 | 結果 |
| --- | --- | --- |
| 1：09手書き | 7+5=12、未知メソッドをENOSYSで拒否、拒否時はサービスへ到達しない | PASS |
| 2：09生成物 | 同じFIDLをCore 3.2.15で生成、5ヘッダーのadd・型・返信手段を観察 | 生成・観察済み |
| 3：10 | producer終了後にconsumerで12、容量2の3件目でEAGAIN、受信後の再送成功 | PASS |
| 4：11 | 通常系、2バイトread、EOF、子の回収、不正行・長い行・未終端行の拒否 | PASS |
| 5：12 | 通常系、writer先行openの待機、既存FIFOの保持、自作FIFOの削除 | PASS |

CommonAPI bindingとruntimeを使う通信は、計画にある別枠の発展課題として未実施。
全章の実装を用意したが、学習は09の手書き→生成物→10→11→12の順に進め、各章の問いを説明してから次へ移る。

## ビルド

SDP環境設定済みのWindows cmdでリポジトリ直下から実行する。
各章のbuild.cmdも単独実行可能。生成ツールやJavaがなくても手書き教材はビルドできる。

```bat
call C:\Users\hi9nn\qnx800\qnxsdp-env.bat
build-learning.cmd
```

C++17、`-Vgcc_ntox86_64`、`-D_QNX_SOURCE`、デバッグ情報、最適化なし。
`-Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Werror`ですべて成功。
QNX 8.0のrcvid_tとMsgSend戻り値を狭いintに変換せず扱う。
mq_*はlibcで解決し、追加ライブラリー不要でリンクできた。

## 配置と一括検証

以下は今回の接続先。別のVMではroot@192.168.56.101を置き換える。
同名の旧実験プロセスがない状態で行う。各章の配置先を分け、既存の/tmp/buildは使わない。

Windows PowerShell、リポジトリ直下：

```powershell
ssh -o MACs=hmac-sha2-256-etm@openssh.com root@192.168.56.101 "mkdir -p /tmp/qnx-ipc-lab-learning/09-fidl-flow /tmp/qnx-ipc-lab-learning/10-posix-mqueue /tmp/qnx-ipc-lab-learning/11-pipe /tmp/qnx-ipc-lab-learning/12-fifo"
$chapters = @('09-fidl-flow', '10-posix-mqueue', '11-pipe', '12-fifo')
foreach ($chapter in $chapters) {
    scp -r -o MACs=hmac-sha2-256-etm@openssh.com "$chapter/build" "root@192.168.56.101:/tmp/qnx-ipc-lab-learning/$chapter/"
    if ($LASTEXITCODE -ne 0) { throw "Transfer failed: $chapter" }
}
scp -o MACs=hmac-sha2-256-etm@openssh.com verify-learning.sh root@192.168.56.101:/tmp/qnx-ipc-lab-learning/
ssh -o MACs=hmac-sha2-256-etm@openssh.com root@192.168.56.101 "sh /tmp/qnx-ipc-lab-learning/verify-learning.sh"
```

スクリプトはWindowsから転送したバイナリーへ実行権限を付ける。
09はserver onceを使い、サーバーが名前を登録したログを確認してclientを起動する。
キューは教材専用名をcleanupしてから開始するため、同じ教材の別実験を同時実行しない。
FIFOは既存パスがあれば止まり、今回prepareしたFIFOのみ終了時に削除する。
Ctrl+Cなどで中断した場合も、スクリプトが保持する子プロセスを終了・回収する。

## 実測記録

[2026-11-16/results/](2026-11-16/results/09-add-client.txt)に、VMから取得した各実験の標準出力・標準エラーを保存した。
11-invalid、11-long、11-partialのFAILは拒否実験の期待結果であり、一括検証は非ゼロ終了を確認してPASSにする。
FIFOの待機はbefore openを確認後に1秒待ち、プロセス存続とafter open未出力を確認して記録した。
ログは実測の一例であり、親子・別プロセスの出力順序は実行ごとに変わりうる。

```text
09: normal=12, unknown rejected before service PASS
10: retained request=12, full=EAGAIN, receive and retry PASS
11: normal, 2-byte reads, invalid/long/partial rejection PASS
12: normal=12, writer open waits, existing FIFO retained PASS
All learning checks PASS
```

一括検証後に常駐server・reader・writer・教材キュー・FIFOは残らない。
VMの配置済みバイナリーとresultsは、再学習用に/tmp/qnx-ipc-lab-learningへ残した。
