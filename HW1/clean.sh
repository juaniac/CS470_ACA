#!/bin/bash

cd given_tests || { echo "given_tests folder not found!"; exit 1; }

for dir in */; do
  if [ -d "$dir" ]; then
    rm -f "$dir/user_output.json"
  fi
done

cd ..
rm -f sim

echo "Cleanup complete."