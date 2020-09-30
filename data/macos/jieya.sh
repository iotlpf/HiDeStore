#!/bin/sh

Folder="/home/lpf/workspace/macos/tarbz"
files=$(ls $Folder)
for file in $files
do
	path="/home/lpf/workspace/macos/tarbz/${file}"
	newfile=${file%.tar.bz2}
	newpath="/home/lpf/workspace/macos/macos"
	tar -jxvf ${path} -C ${newpath} > /dev/null
	echo "tar ${newfile}"
done
