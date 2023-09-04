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
	echo "Usage: $0 <filename> <filecontents>"
	exit 1
fi

# Check writefile is specified
if [ -z "$writefile" ]; then
  echo "Error: 'filename' argument is not specified."
  exit 1
fi

# Check writestr is specified
if [ -z "$writestr" ]; then
  echo "Error: 'filecontents' argument is not specified."
  exit 1
fi

# Create output directory
mkdir -p "$(dirname "$writefile")"

# Write the content to the file
# This code block was fully generated using ChatGPT at https://chat.openai.com/ with prompts including "shell script to write string to a file"
echo "$writestr" > "$writefile"

# Check for error
if [ $? -ne 0 ]; then
  echo "Error: Failed to write to '$writefile'."
  exit 1
fi

echo "File '$writefile' has been created successfully."
