#!/bin/sh

# write in a string name inside a file
# create the path if the path doesnt exist previously

if [ $# -ne 2 ];
then
   echo "Arguments requested not found, exiting...."
   exit 1
fi

path_to_file=$1
path_str=$2

# create a directory if the path doesnt exist

if [ -d "$(dirname "$path_to_file")" ]
then
   echo "The current path exists"
else
   echo "Creating directory"
   mkdir $(dirname "$path_to_file")
fi

# Create a file if file did not exist, or else overwrite the file
echo $2 > $1
exit 0


