# HiDeStore

The paper has been accepted by Middleware 2020.

> Pengfei Li, Yu Hua, Qin Cao, Mingxuan Zhang, “Improving the Restore Performance via Physical-Locality Middleware for Backup Systems”, Proceedings of the annual ACM/IFIP Middleware conference (Middleware), 2020.

## Environment

Linux 64bit

## Dependencies

HiDeStore is implemented based on [Destor](https://github.com/fomy/destor).

1. libssl-dev is required to calculate sha-1 digest;

2. GLib 2.32 or later version

   > libglib.so and glib.h may not be found when you first install it.

   > The header files (that originally locate in /usr/local/include/glib-2.0 and /usr/local/lib/glib-2.0/include) are required to be moved to a searchable path, such as "/usr/local/include".

   > Also a link named libglib.so should be created in "/usr/local/lib".

3. Makefile is automatically generated by GNU autoconf and automake.

## Compile

> [optional] Run the following codes to check whether all the dependencies are installed.
>
> ```shell
> cd check
> make
> ./check
> ```

Compile HiDeStore:

```shell
make
```

## Artifact Evaluation

#### 0.Preparing

At the root directory of HiDeStore and run the following commands, which will build the necessary directories:

```shell
chmod +x init.sh
./init.sh
```

#### 1.Download the datasets

We use 4 public datasets in our evaluations.

- linux kernel: https://mirrors.edge.kernel.org/pub/linux/kernel/
- gcc: https://ftp.gnu.org/gnu/gcc
- fslhomes: http://tracer.filesystems.org/traces/fslhomes/
- macos: http://tracer.filesystems.org/traces/macos/

You can download some versions for evaluations. We take the dataset ‘linux kernel’ as an example to show how to organize the data, and other datasets are similar.

> There are two directories under ./data/kernel after running init.sh, including tar and kernel. Please download the compressed data into the directory: data/kernel/tar. Run uncompress.sh at the directory of data, which uncompresses the data to directory data/kernel/kernel.

```shell
cd data     # at the directory of data
chmod +x uncompress.sh
./uncompress.sh kernel bz

# change "kernel" to "[gcc, fsl, macos]" for other workloads
./uncompress.sh gcc bz
# change "bz" to "[gz, xz]" for other types of the compressed files
./uncompress.sh kernel xz
```

For example, the final direcory tree looks like this:

```
|--data
|  |--kernel
|  |  |--kernel
|  |  |  |--linux-1.3.4
|  |  |  |--linux-1.3.5
|  |  |--tar
|  |  |  |--linux-1.3.4.tar.bz2
|  |  |  |--linux-1.3.5.tar.bz2
|  |--uncompress.sh     
```

#### 2.Running

At the root directory of HiDeStore and run the script files:

```shell
# be at the root directoy of HiDeStore
chmod +x rebuild drebuild
chmod +x ./script/*
./script/run_benchmark.sh kernel [or gcc, fsl, macos]
./script/dbenchmark.sh kernel [or gcc, fsl, macos]  
./script/drestore.sh 9
./script/restore.sh 9
# the number 9 represents that we restore 10 backup versions
```

More details are shown in data/evaluation.docx.
