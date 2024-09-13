#!/bin/bash

FILE_INPUT="$1"
FILE_TMP="piugba.tmp.gba"
FILE_OUTPUT="piugba.out.gba"
DATA="$2"

if [ -z "$FILE_INPUT" ] || [ -z "$DATA" ] ; then
    echo ""
    echo "Usage: ./package.sh path/to/piugba.gba path/to/files.gbfs"
    echo ""
    exit 1
fi

if [ ! -f "$DATA" ]; then
    echo ""
    echo "The file $DATA does not exist. Run make import first!"
    echo ""
    exit 1
fi

KB=$((1024))
MAX_ROM_SIZE_KB=$((32 * $KB - 1))
INITIAL_REQUIRED_SIZE_KB=1024

ROM_SIZE=$(wc -c < "$FILE_INPUT")
if [ $? -ne 0 ]; then
  exit 1
fi
GBFS_SIZE=$(wc -c < $DATA)
if [ $? -ne 0 ]; then
  exit 1
fi
ROM_SIZE_KB=$(($ROM_SIZE / $KB))
GBFS_SIZE_KB=$(($GBFS_SIZE / $KB))
MAX_REQUIRED_SIZE_KB=$(($MAX_ROM_SIZE_KB - $GBFS_SIZE_KB))
if (( $MAX_REQUIRED_SIZE_KB < $ROM_SIZE_KB )); then
    echo ""
    echo "[!] ERROR:"
    echo "GBFS file too big."
    echo ""
    echo "GBFS_SIZE_KB=$GBFS_SIZE_KB"
    echo "ROM_SIZE_KB=$ROM_SIZE_KB"
    echo "(MAX_ROM_SIZE_KB=$MAX_ROM_SIZE_KB)"
    echo ""
    exit 1
fi
REQUIRED_SIZE_KB=$(($INITIAL_REQUIRED_SIZE_KB > $MAX_REQUIRED_SIZE_KB ? $MAX_REQUIRED_SIZE_KB : $INITIAL_REQUIRED_SIZE_KB))
PAD_NEEDED=$((($REQUIRED_SIZE_KB * $KB) - $ROM_SIZE))

cp "$FILE_INPUT" "$FILE_TMP"
if [ $? -ne 0 ]; then
  exit 1
fi
dd if=/dev/zero bs=$PAD_NEEDED count=1 >> "$FILE_TMP"
if [ $? -ne 0 ]; then
  exit 1
fi
cat "$FILE_TMP" "$DATA" > "$FILE_OUTPUT"
if [ $? -ne 0 ]; then
  exit 1
fi
rm "$FILE_TMP"
