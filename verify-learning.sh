#!/bin/sh
# Run on QNX after deploying each chapter's build directory.
set -eu
cd /tmp/qnx-ipc-lab-learning
mkdir -p results
chmod u+x 09-fidl-flow/build/server 09-fidl-flow/build/client \
    10-posix-mqueue/build/producer 10-posix-mqueue/build/consumer \
    11-pipe/build/pipe_demo 12-fifo/build/reader 12-fifo/build/writer
job_pid=
fifo_owned=no
cleanup() {
    if [ -n "$job_pid" ]; then kill "$job_pid" 2>/dev/null || :; wait "$job_pid" 2>/dev/null || :; fi
    if [ "$fifo_owned" = yes ] && [ -p /tmp/qnx-ipc-lab-add-12.fifo ]; then
        rm /tmp/qnx-ipc-lab-add-12.fifo
    fi
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
wait_log() {
    attempts=0
    until grep -q "$2" "$1"; do
        attempts=$((attempts + 1))
        if [ "$attempts" -ge 10 ]; then echo "Timed out waiting for $2"; exit 1; fi
        kill -0 "$job_pid"
        sleep 1
    done
}

./09-fidl-flow/build/server once >results/09-add-server.txt 2>&1 &
job_pid=$!
wait_log results/09-add-server.txt 'name='
./09-fidl-flow/build/client >results/09-add-client.txt 2>&1
wait "$job_pid"
job_pid=
grep -q 'result=12' results/09-add-client.txt
./09-fidl-flow/build/server once >results/09-unknown-server.txt 2>&1 &
job_pid=$!
wait_log results/09-unknown-server.txt 'name='
./09-fidl-flow/build/client unknown >results/09-unknown-client.txt 2>&1
wait "$job_pid"
job_pid=
if grep -q '\[service\]' results/09-unknown-server.txt; then exit 1; fi
echo '09: normal=12, unknown rejected before service PASS'

queue=./10-posix-mqueue/build
"$queue/producer" cleanup >results/10-queue.txt 2>&1
"$queue/producer" >>results/10-queue.txt 2>&1
"$queue/consumer" >>results/10-queue.txt 2>&1
"$queue/producer" full >>results/10-queue.txt 2>&1
"$queue/consumer" one-keep >>results/10-queue.txt 2>&1
"$queue/producer" retry >>results/10-queue.txt 2>&1
"$queue/consumer" two >>results/10-queue.txt 2>&1
grep -q 'third send EAGAIN=1' results/10-queue.txt
echo '10: retained request=12, full=EAGAIN, receive and retry PASS'

./11-pipe/build/pipe_demo >results/11-normal.txt 2>&1
./11-pipe/build/pipe_demo small >results/11-small.txt 2>&1
grep -q 'result=12' results/11-small.txt
grep -q 'read bytes=2' results/11-small.txt
for mode in invalid long partial; do
    if ./11-pipe/build/pipe_demo "$mode" >"results/11-$mode.txt" 2>&1; then
        echo "Expected rejection: $mode"; exit 1
    fi
done
echo '11: normal, 2-byte reads, invalid/long/partial rejection PASS'

if [ -e /tmp/qnx-ipc-lab-add-12.fifo ] || [ -L /tmp/qnx-ipc-lab-add-12.fifo ]; then
    echo 'FIFO path already exists; inspect it before running verification'; exit 1
fi
./12-fifo/build/reader >results/12-reader.txt 2>&1 &
job_pid=$!
wait_log results/12-reader.txt 'before open'
./12-fifo/build/writer >results/12-writer.txt 2>&1
wait "$job_pid"
job_pid=
grep -q 'result=12' results/12-reader.txt
test ! -e /tmp/qnx-ipc-lab-add-12.fifo

./12-fifo/build/reader --prepare >results/12-prepare.txt 2>&1
fifo_owned=yes
./12-fifo/build/writer >results/12-writer-first.txt 2>&1 &
job_pid=$!
wait_log results/12-writer-first.txt 'before open'
sleep 1
kill -0 "$job_pid"
if grep -q 'after open' results/12-writer-first.txt; then exit 1; fi
echo 'writer remained blocked before reader opened' >results/12-wait-observed.txt
./12-fifo/build/reader >results/12-reader-second.txt 2>&1
wait "$job_pid"
job_pid=
grep -q 'result=12' results/12-reader-second.txt
test -p /tmp/qnx-ipc-lab-add-12.fifo
echo '12: normal=12, writer open waits, existing FIFO retained PASS'
echo 'All learning checks PASS'
