#!/bin/bash

# Checking if the arguments are passed
if [[ $# -eq 0 ]]; then
  echo "ERROR: No arguments passed!"
  exit 1
fi

filesdir="$1"
searchstr="$2"

# # Check if the file path is a valid directory
if [[ ! -d ${filesdir} ]]; then
  echo "ERROR: Directory does not exist!"
  exit 1
fi

echo "SUCCESS: Directory found!"

# Counting number of files
FILECOUNT=$(ls -ali | wc -l)
SEARCHCOUNT=$(grep -irn "${searchstr}" ${filesdir} | wc -l)

echo "The number of files on this path are ${FILECOUNT} and the number of matching lines are ${SEARCHCOUNT}."
