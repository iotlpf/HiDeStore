#!/bin/bash


dataset="kernel"

rbv=10

if [ $# -gt 0 ];then
    rbv=$1
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


DIR=./myworking/restore/
mkdir -p ${DIR}
rm -rf ${DIR}/*

> drlog
for ((i=0;i<$rbv;i++))
do
	mkdir -p ${DIR}/destor
	./build/destor -r${i} ${DIR}/destor >> drlog
	echo " " >> drlog
done
