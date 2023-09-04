#------------------------------------------------
# Author: Mubeena Udyavar Kazi
# Course: ECEN 5713 - AESD
# Reference: ChatGPT and stack overflow
# -----------------------------------------------

#!/bin/sh

writefile="$1"
writestr="$2"

# Check number of arguments
if [ $# -ne 2 ]; then
	echo "Usage: $0 <Provide a write file> <Provide a text to write in the file>"
	exit 1
fi

# Check writefile is specified
if [ -z "$writefile" ]; then
  echo "Error: 'writefile' argument is not specified."
  exit 1
fi

# Check writestr is specified
if [ -z "$writestr" ]; then
  echo "Error: 'writestr' argument is not specified."
  exit 1
fi

# Create directory
mkdir -p "$(dirname "$writefile")"

# Write the content to the file
echo "$writestr" > "$writefile"

# Check for error
if [ $? -ne 0 ]; then
  echo "Error: Failed to write to '$writefile'."
  exit 1
fi

echo "Content has been successfully written to '$writefile'."
