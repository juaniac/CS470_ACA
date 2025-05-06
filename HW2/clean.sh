#!/bin/bash

cd given_tests || { echo "given_tests folder not found!"; exit 1; }

for dir in */; do
  if [ -d "$dir" ]; then
    rm -f "$dir/pip.json"
    rm -f "$dir/simple.json"
  fi
done

echo "Cleanup complete."