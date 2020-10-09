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


DIR=/home/lpf/workspace/restore/
rm -rf ${DIR}/*

> rlog
for ((i=$rbv;i>0;i--))
do
	mkdir -p ${DIR}/active
	./build/destor -c${i} ${DIR}/active >> rlog
	echo " " >> rlog
done
