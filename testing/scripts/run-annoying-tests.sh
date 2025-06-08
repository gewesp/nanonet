#! /bin/bash

. scripts/testenv.sh

set -e

$BIN_DIR/audio-test $INPUT_DIR/triad.melody
mv $INPUT_DIR/triad.snd $GOLDEN_DIR

if [ Darwin = `uname` ] ; then
  TESTDIR=/tmp/cpp-lib-test-output
  mkdir -p $TESTDIR
  afconvert -d LEI16 -f caff --verbose $GOLDEN_DIR/triad.snd -o $TESTDIR/triad.caf
  open $TESTDIR
fi
