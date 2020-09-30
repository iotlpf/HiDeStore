#!/bin/sh

Folder="/home/lpf/workspace/gcc/tarbz"
files=$(ls $Folder)
for file in $files
do
	path="/home/lpf/workspace/gcc/tarbz/${file}"
	newfile=${file%.tar.bz2}
	newpath="/home/lpf/workspace/gcc/gcc"
	tar -jxvf ${path} -C ${newpath} > /dev/null
	echo "tar ${newfile}"
done
