#! /bin/bash

diff=$(git diff | wc -l)

if [ 0 = $diff ] ; then
  echo "All tests passed OK"
else
  echo "Test results disagree with expected results."
  if [[ $GITLAB_CI = "" ]] ; then
    echo "Please run git diff for details."
  else
    git diff
  fi
  exit 1
fi
