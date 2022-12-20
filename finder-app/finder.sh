#!/bin/bash
if [ -z "$1" ] || [ -z "$2" ]
then
	echo "Please enter in following format:"
	echo "arg 1: location to search"
	echo "arg 2: string to search."
	exit 1
fi

if [ -d $1 ] || [ -f $1 ]
then
	:
else
	echo "file/folder not found."
	exit 1
fi

files_num=$(grep -rl "$2" "$1" | wc -l)
match_num=$(grep -r "$2" "$1" |wc -l)
echo "The number of files are ${files_num} and the number of matching lines are ${match_num}"
