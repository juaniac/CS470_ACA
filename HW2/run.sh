#!/bin/bash

if [ "$#" -ne 3 ]; then
    echo "Usage: ./run.sh </path/to/input.json> </path/to/simple.json> </path/to/pip.json>"
    exit 1
fi

python3 src/main.py $1 $2 $3