#!/bin/bash

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <make-target> [make-arguments...]"
    exit 1
fi

wsl -e ./dockermake.sh "$@"
