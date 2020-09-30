#!/bin/sh

Folder="/home/lpf/workspace/kernel/tarbz"
files=$(ls $Folder)
for file in $files
do
	path="/home/lpf/workspace/kernel/tarbz/${file}"
	newfile=${file%.tar.bz2}
	newpath="/home/lpf/workspace/kernel/kernel"
	tar -jxvf ${path} -C ${newpath} > /dev/null
	mv ${newpath}/linux ${newpath}/${newfile}
	echo "tar ${newfile}"
done
