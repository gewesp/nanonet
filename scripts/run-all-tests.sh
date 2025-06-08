#! /bin/sh

. ./scripts/testenv.sh

./scripts/run-fast-tests.sh

$BIN_DIR/dispatch-test                      > $GOLDEN_DIR/dispatch.txt

./scripts/check-git-diffs.sh
