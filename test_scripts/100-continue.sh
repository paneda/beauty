#!/bin/bash
# Description: Demonstrate HTTP 100-continue with curl

set -e

# Create a 2MB file
FILE="test_2mb.bin"
dd if=/dev/zero of="$FILE" bs=1M count=2

# Set your upload URL here
URL="http://localhost:8080"

echo "Uploading $FILE to $URL with Expect: 100-continue (timeout 10s)..."

curl -v \
  --expect100-timeout 10 \
  -H "Expect: 100-continue" \
  -H "Authorization: Bearer=valid_token" \
  -F "file=@$FILE" \
  "$URL"

# Clean up
rm "$FILE"

