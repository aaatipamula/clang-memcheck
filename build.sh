#!/usr/bin/bash

set -e

BUILD_DIR="./build"
OS=$(uname -s)

CLANG_VERSION=21

# Select the clang include library path
if [[ "$OS" == "Darwin" ]]; then
  # NOTE: May have to change this directory if clang is not installed with homebrew
  CLANG_INCLUDE="/opt/homebrew/opt/llvm/clang"
elif [[ "$OS" == "Linux" ]]; then
  CLANG_INCLUDE="/usr/lib/clang"
else
  echo "Unsupported OS" &>2
  exit 1
fi

# Remove build directory if exists
if [[ ! -d "$BUILD_DIR" ]]; then
  # Create build directory
  mkdir -p build
  cd build

  # Configure with cmake
  if [[ "$OS" == "Darwin" ]]; then
    # NOTE: Assumes that llvm and clang are installed with homebrew.
    #       Change -DLLVM_DIR and -DClang_DIR to the correct path if needed
    cmake -DLLVM_DIR=/opt/homebrew/opt/llvm/lib/cmake/llvm \
          -DClang_DIR=/opt/homebrew/opt/llvm/lib/cmake/clang ..
  elif [[ "$OS" == "Linux" ]]; then
    cmake ..
  fi
else
  # Just cd into current build folder
  cd build
fi 

# Build
make

if [[ "$1" = "test" ]]; then
  # Run on test files
  for file in ../tests/*.c; do
    echo "=== Testing on $file ==="
    ./memory-analyzer "$file" -- "-I$CLANG_INCLUDE/$CLANG_VERSION/include"
    echo
  done
fi

