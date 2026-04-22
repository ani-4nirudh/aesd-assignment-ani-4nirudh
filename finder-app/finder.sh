#!/bin/bash

# Checking if the arguments are passed
if [[ $# -lt 2 ]]; then
  echo "ERROR: Pass two arguments!"
  exit 1
fi

filesdir="$1"
searchstr="$2"

# Check if the file path is a valid directory
if [[ ! -d ${filesdir} ]]; then
  echo "ERROR: Directory does not exist!"
  exit 1
fi

# Counting number of files
FILECOUNT=$(find ${filesdir} -type f | wc -l)
SEARCHCOUNT=$(grep -irn "${searchstr}" ${filesdir} | wc -l)

echo "The number of files are ${FILECOUNT} and the number of matching lines are ${SEARCHCOUNT}"
