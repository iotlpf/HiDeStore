#!/bin/bash


dataset="kernel"

if [ $# -gt 0 ];then
    dataset=$1
fi

echo "tar dataset <- $dataset"


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
> log

if [[ "$dataset" = "kernel" ]] || [[ "$dataset" = "gcc" ]]; then
	for file in $(ls $path); do
		./build/destor $path/$file -a>> log
		echo " " >> log
		echo "process $file"
	done
fi

if [[ "$dataset" = "fsl" ]] || [[ "$dataset" = "macos" ]]; then
	for dir in $(ls $path); do
		for file in $(ls $path/$dir); do
			if [[ $file == *$hashfile* ]]; then
				./build/destor $path/$dir/$file -a -p"simulation-level all" -p"trace-format fsl">> log
				echo " " >> log
				echo "process $file"
			fi
		done
	done
fi

./build/destor -s >> backup.log