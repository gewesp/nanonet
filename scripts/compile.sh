#! /bin/bash

# Exit if any subprogram exits with an error
set -e

if [[ "" == $GITLAB_CI ]] ; then
  if [[ Darwin == $(uname -s) ]] ; then
    # brew install png++ eigen
    export PNGPP_INCLUDE_DIR=/opt/homebrew/include/png++
    export EIGEN3_INCLUDE_DIR=/opt/homebrew/opt/eigen/include/eigen3
    cmake_compiler="-D CMAKE_CXX_COMPILER=clang++"
  else
    if grep 18\.04 /etc/issue ; then
      cmake_compiler="-D CMAKE_CXX_COMPILER=clang++-7"
    elif grep 24\.04 /etc/issue ; then
      cmake_compiler="-D CMAKE_CXX_COMPILER=clang++"
    else
      echo "Not a supported Ubuntu version (24.04 or 18.04)"
      exit 1
    fi
  fi
else
  # Gitlab CI: Use whatever is available
  # As of 6/2020, use clang++ as well on CI pipeline,
  # reason:  https://github.com/nlohmann/json/issues/1920
  cmake_compiler="-D CMAKE_CXX_COMPILER=clang++-7"
fi

if [ "" = "$1" ] ; then
  build_type=RelWithDebInfo
else
  build_type="$1"
fi

rm -rf build CMakeCache.txt CMakeFiles/
mkdir build
cd build
echo "Configuring for $build_type ..."
cmake $cmake_compiler -D CMAKE_BUILD_TYPE=$build_type ..
make -j6 VERBOSE=1
