#---------------------------------------------
# Author: Mubeena Udyavar Kazi
# Course: ECEN 5713 - AESD
# Reference: ChatGPT and stack overflow
#---------------------------------------------

#!/bin/sh

filesdir="$1"
searchstr="$2"

# Check number of arguments
if [ $# -ne 2 ]; then
  echo "Usage: $0 <filepath> <search_string>"
  exit 1
fi

# Check if specified directory exist
if [ ! -d "$filesdir" ]; then 
   echo "Error: Specified filepath '$filesdir' is not a directory or does not exist"
   exit 1
fi       

# Function to search for the text string in files and count matching lines
string_search(){
  local dir="$1"
  local search="$2"
  local number_of_files=0
  local number_of_lines=0

  all_files=$(find "${dir}" -type f)
  for file in ${all_files}; do
  # This code block was partially generated using ChatGPT at https://chat.openai.com/ with prompts including "shell script to count lines with matching string in file".
    if [ -f "$file" ]; then
      number_of_files=$((number_of_files + 1))
      matching_lines=$(grep -c "$search" "$file")
      number_of_lines=$((number_of_lines + matching_lines))
    fi
  done

  echo "The number of files are $number_of_files and the number of matching lines are $number_of_lines"
}

# Call the function to search 
string_search "$filesdir" "$searchstr"
