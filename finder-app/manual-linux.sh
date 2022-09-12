#!/bin/bash
# Script outline to install and build kernel.
# Author: Sricharan Kidambi
# Build the linux system
# Include compiling the kernel, setting up the file system with app content.
# Date: 11th September 2022
# References: Mastering Embedded Linux Chapter 5, coursera Module 2 videos

set -e
set -u

# For Debugging, discussed with swapnil for the belo point
sudo env "PATH=$PATH"

# set default directory to OUTDIR, condition a.i in assignment instructions
OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi
# create a directory if it doesnt exist, else no operation
# condition b in assignment instructions
mkdir -p ${OUTDIR}
# traverse to that directory
cd "$OUTDIR"
# 
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    
    # TODO: Add your kernel build steps here
    
    #mrproper performs "deep-clean"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    #defconfig configures virtual arm dev board which will emulate QEMU
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    #Build a kernel image for booting with QEMU
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    #Build any kernel modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    #Build the device tree
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

# copy the image from the existing location to OUTDIR directory
echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
# Reference - Lecture material, building a root file system
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

# Base directories created
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make -j4 CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
export SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

cp $SYSROOT/lib/ld-linux-aarch64.so.1 lib
cp $SYSROOT/lib64/libm.so.6 lib64
cp $SYSROOT/lib64/libresolv.so.2 lib64
cp $SYSROOT/lib64/libc.so.6 lib64

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
# references: discussed with swapnil ghonge and obtained ideas on how to copy contents to rootfs directory
# Additional references: https://askubuntu.com/questions/835657/copy-file-to-current-repository
# Point 1.f in assignment instructions
cp ./*.sh ${OUTDIR}/rootfs/home
# Point 1.e.ii in assignment instructions
cp ./writer ${OUTDIR}/rootfs/home
# Point 1.g in assignment instructions
cp -r ./conf/ ${OUTDIR}/rootfs/home
# References: Mastering Embedded Linux Programming chapter 5 page 199
# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# References: Mastering Embedded Linux Programming chapter 5 page 219
# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find .| cpio -H newc -ov --owner root:root > ../initramfs.cpio
# Navigate to OUTDIR FILE
cd ..
# Zip the OUTDIR FILE
# references: https://linuxsize.com/post/gzip-command-in-linux/
gzip -f initramfs.cpio
