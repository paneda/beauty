#!/bin/bash
# Description: Demonstrate HTTP 100-continue with curl

set -e

# First upload a small file that will not trigger 100-continue
echo "Uploading a small file without Expect: 100-continue..."
FILE="test_512KB.bin"
dd if=/dev/urandom of="$FILE" bs=1K count=512

MD5SUM=$(md5sum "$FILE" | awk '{print $1}')
echo "MD5 checksum of $FILE: $MD5SUM"

curl -v -F "file=@$FILE" http://localhost:8080
rm "$FILE"


# Now upload a larger file that will trigger 100-continue
echo "Uploading a large file with Expect: 100-continue..."
FILE="test_2mb.bin"
dd if=/dev/urandom of="$FILE" bs=1M count=2

# Calculate and display the MD5 checksum
MD5SUM=$(md5sum "$FILE" | awk '{print $1}')
echo "MD5 checksum of $FILE: $MD5SUM"

# Set your upload URL here
URL="http://localhost:8080"

curl -v \
  --expect100-timeout 10 \
  -H "Expect: 100-continue" \
  -H "Authorization: Bearer=valid_token" \
  -F "file=@$FILE" \
  "$URL"

# Clean up
rm "$FILE"

