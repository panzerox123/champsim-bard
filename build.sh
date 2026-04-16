#!/bin/bash

if [ "$#" -eq 0 ]; then
    echo "Usage: $0 <config_name> [config_name_2 ...]"
    echo "Example: $0 bard baseline bard_new_watermark"
    exit 1
fi

for MODEL in "$@"; do
    CONFIG_NAME="$MODEL"

    # Automatically append .json if not provided
    if [[ "$CONFIG_NAME" != *.json ]]; then
        CONFIG_NAME="${CONFIG_NAME}.json"
    fi

    JSON_PATH="json/${CONFIG_NAME}"

    if [ ! -f "$JSON_PATH" ]; then
        echo "Error: Configuration file '$JSON_PATH' does not exist. Skipping."
        continue
    fi

    echo "========================================"
    echo " configuring and building with: $JSON_PATH"
    echo "========================================"

    ./config.sh "$JSON_PATH" && make -j4
    
    if [ $? -ne 0 ]; then
        echo "Error: Build failed for '$JSON_PATH'. Stopping."
        exit 1
    fi
done

echo "========================================"
echo " All builds completed successfully."
echo "========================================"
