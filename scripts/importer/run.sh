#!/bin/bash

if [ -n "$NVM_DIR" ]; then
  source $NVM_DIR/nvm.sh
fi

node scripts/importer/src/importer.js "$@"
