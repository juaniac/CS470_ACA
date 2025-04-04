#!/bin/bash

if [ "$#" -ne 2 ]; then
  echo "Usage: ./run.sh </path/to/input.json> </path/to/output.json>"
  exit 1
fi

./cpu "$1" "$2"