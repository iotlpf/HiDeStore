#!/bin/bash


dataset="kernel"

if [ $# -gt 0 ];then
    echo "dataset <- $1"
    dataset=$1
else
    echo "default dataset <- $dataset"
fi


kernel_path="./data/kernel/kernel"
gcc_path="./data/gcc/gcc"
fsl_path="./data/fslhomes/fsl/"
macos_path="./data/macos/macos"


case $dataset in
	"kernel")
		path=$kernel_path
		;;
	"gcc")
		path=$gcc_path
		;;
	"fsl")
		path=$fsl_path
		;;
	"macos")
		path=$macos_path
		;;
	*)
		echo "Wrong dataset!"
		exit 1
		;;
esac

hashfile=".4kb.hash.anon"

./rebuild
> dlog

if [[ "$dataset" = "kernel" ]] || [[ "$dataset" = "gcc" ]]; then
	for file in $(ls $path); do
		./build/destor $path/$file >> dlog
		echo " " >> dlog
		echo "process $file"
	done
fi

if [[ "$dataset" = "fsl" ]] || [[ "$dataset" = "macos" ]]; then
	for dir in $(ls $path); do
		for file in $(ls $path/$dir); do
			if [[ $file == *$hashfile* ]]; then
				./build/destor $path/$dir/$file -p"simulation-level all" -p"trace-format fsl">> dlog
				echo " " >> dlog
				echo "process $file"
			fi
		done
	done
fi

./build/destor -s >> backup.log