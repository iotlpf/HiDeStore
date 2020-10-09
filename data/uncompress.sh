#!/bin/sh

dataset="kernel"
type="bz"

if [ $# -gt 0 ];then
    dataset=$1
fi

if [ $# -gt 1 ];then
    type=$2
fi

echo "tar dataset <- $dataset"


case $dataset in
	"kernel")
		Folder="./kernel/tar"
		newpath="./kernel/kernel"
		;;
	"gcc")
		Folder="./gcc/tar"
		newpath="./gcc/gcc"
		;;
	"fsl")
		Folder="./fslhomes/tar"
		newpath="./fslhomes/fsl"
		;;
	"macos")
		Folder="./macos/tar"
		newpath="./macos/macos"
		;;
	*)
		echo "Wrong dataset!"
		exit 1
		;;
esac

echo $Folder


files=$(ls $Folder)
for file in $files
do
	path="${Folder}/${file}"

	case $type in
		"bz")
			newfile=${file%.tar.bz2}
			tar -jxvf ${path} -C ${newpath} > /dev/null
			;;
		"gz")
			newfile=${file%.tar.gz}
			tar -zxvf ${path} -C ${newpath} > /dev/null
			;;
		"xz")
			newfile=${file%.tar.xz}
			tar -xvJf ${path} -C ${newpath} > /dev/null
			;;
		*)
			echo "Wrong dataset!"
			exit 1
			;;
	esac


	if [[ $dataset == "kernel" ]]; then
		mv ${newpath}/linux ${newpath}/${newfile}
	fi
	echo "tar ${newfile}"
done
