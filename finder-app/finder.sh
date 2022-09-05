#!/bin/sh

# Name		: Sricharan Kidambi
# Course	: ECEN 5713 Advanced Embedded software Development
# Date		: 4th September 2022
# file		: finder.sh

# recommended by deekshith patil on office hours

if [ $# -ne 2 ];
then
   echo "Arguments requested not found, exiting...."
   exit 1
fi

# https://youtube.com/watch?v=mXh7rObKU4E - Exit status and logging references

directoryname=$1
stringname=$2

if [ ! -d "${directoryname}" ];
then
   echo "This is not a directory"
   exit 1
fi

numfiles=$(find $directoryname -type f | wc -l)
numberofmatches=$(grep -r $stringname $directoryname | wc -l)

# r searches recursively on every file, | used for feeding the output of one file to another file

echo "The number of files are $numfiles and the number of matching lines are $numberofmatches"

exit 0
