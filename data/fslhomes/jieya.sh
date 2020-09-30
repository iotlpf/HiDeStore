#!/bin/sh

Folder="/home/lpf/workspace/fslhomes/tarbz"
files=$(ls $Folder)
for file in $files
do
	path="/home/lpf/workspace/fslhomes/tarbz/${file}"
	newfile=${file%.tar.bz2}
	newpath="/home/lpf/workspace/fslhomes/fsl"
	tar -jxvf ${path} -C ${newpath} > /dev/null
	echo "tar ${newfile}"
done
