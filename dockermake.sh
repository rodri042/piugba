#!/bin/bash

set -e

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <make-target> [make-arguments...]"
    exit 1
fi

cleanup() {
    docker kill "$container_id"
    docker rm "$container_id"
    exit 130
}

trap cleanup SIGINT

container_id=$(docker run -it -d \
  -v "$(pwd)":/opt/piugba \
  -e PWD=/opt/piugba \
  afska/piugba-dev \
  make "$@")

docker logs -f "$container_id"
status_code=$(docker wait "$container_id")
docker rm "$container_id"

exit "$status_code"
