#! /bin/sh

set -e

. ./scripts/testenv.sh


$BIN_DIR/util-test  < $INPUT_DIR/util.txt   > $GOLDEN_DIR/util.txt
$BIN_DIR/error-test                         > $GOLDEN_DIR/error.txt

$BIN_DIR/tcp-test reverse test:stdio < $INPUT_DIR/tcp.txt | grep -v '500 Thread ID:' > $GOLDEN_DIR/tcp.txt

$BIN_DIR/syslogger-test                     > $GOLDEN_DIR/syslogger.txt

diff=$(git diff | wc -l)

./scripts/check-git-diffs.sh
