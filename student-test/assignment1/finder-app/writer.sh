#!/bin/bash

if [[ $# -eq 0 ]]; then
  echo "ERROR: No arguments specified!"
  exit 1
fi

writefile="$1"
writestr="$2"

# Checking for the file
if [[ ! -f ${writefile} ]]; then
  echo "ERROR: File could not be found!"
  exit 1
fi

# Creating the file and putting the text
echo "Creating the text file ..."
if touch ${writefile}; then
  echo "Writing to the text file ..."
  echo ${writestr} >${writefile}
else
  echo "ERROR: File could not be created!"
  exit 1
fi
