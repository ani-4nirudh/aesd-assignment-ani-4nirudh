#!/bin/bash

if [[ $# -lt 2 ]]; then
  echo "ERROR: Invalid number of arguments! Please give 2."
  exit 1
fi

writefile="$1"
writestr="$2"

# Checking for the file
if [[ ! -f ${writefile} ]]; then
  echo "ERROR: File could not be found!"
fi

# Extracting the directory name
dir_name=$(dirname ${writefile})
if [[ ! -d ${dir_name} ]]; then
  echo "Creating directory ..."
  mkdir -p ${dir_name}
fi

# Creating the file and putting the text
echo "Putting text in file ..."
if [[ $(echo ${writestr} >${writefile}) -eq 1 ]]; then
  echo "ERROR: Could not put text inside the file!"
  exit 1
fi
