#!/bin/sh

# recommended by deekshith patil on office hours

if [ $# -ne 2 ];
then
   echo "Arguments requested not found, exiting...."
   exit 1
fi

# https://youtube.com/watch?v=mXh7rObKU4E - Exit status and logging references

if [ ! -d $1 ];
then
   echo "This is not a directory"
   exit 1
fi

x=$(find $1 -type f | wc -l)
y=$(grep -r $2 $1 | wc -l)

# r searches recursively on every file, | used for feeding the output of one file to another file

echo "The number of files are $x and the number of matching lines are $y"

exit 0
