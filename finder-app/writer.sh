#!/bin/sh
if [ -z "$1" ] || [ -z "$2" ]
then
	exit 1
fi

dir=$(dirname $1)
echo $dir
mkdir -p $dir
echo "$2" > "$1"
