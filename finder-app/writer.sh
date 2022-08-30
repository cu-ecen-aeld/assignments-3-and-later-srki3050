#!/bin/sh

if [ $# -ne 2 ];
then
   echo "Arguments requested not found, exiting...."
   exit 1
fi

# Create a file if file did not exist, or else overwrite the file
echo $2 > $1
exit 0


