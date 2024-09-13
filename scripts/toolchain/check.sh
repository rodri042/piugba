#!/bin/bash

try() {
  "$@" > /dev/null 2>&1
  status=$?
  if [ $status -eq 127 ]; then
      echo "❌  $1" >&2
      exit $status
  fi
}

if [ -n "$NVM_DIR" ]; then
  source $NVM_DIR/nvm.sh
fi

try gbfs
echo "✔️  gbfs"

try pngfix
echo "✔️  pngfix"

try rm --version
echo "✔️  rm"

try dd --version
echo "✔️  dd"

try md5sum --version
echo "✔️  md5sum"

try cut --version
echo "✔️  cut"

try ffmpeg -version
echo "✔️  ffmpeg"

try ffplay -version
echo "✔️  ffplay"

try magick -version
echo "✔️  magick"

try grit
echo "✔️  grit"

try make -version
echo "✔️  make"

try node -v
echo "✔️  node"

echo ""
echo "✔️  all tools are installed!"
