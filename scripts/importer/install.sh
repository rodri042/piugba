#!/bin/bash

if [ -n "$NVM_DIR" ]; then
  source $NVM_DIR/nvm.sh
fi

cd scripts/importer && npm install
