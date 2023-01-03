#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
LOC=$(which "aarch64-none-linux-gnu-gcc")
LOC=$(dirname ${LOC})
LOC="${LOC}/aarch64-none-linux-gnu-"
if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
#    make ARCH=${ARCH} CROSS_COMPILE={CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    # TODO: Add your kernel build steps here
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir rootfs
cd rootfs
mkdir bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log
ROOTFS=$(pwd)
tree -d

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
sudo make ARCH=${ARCH} CROSS_COMPILE="${LOC}" CONFIG_PREFIX=${ROOTFS} defconfig
sudo make ARCH=${ARCH} CROSS_COMPILE="${LOC}" CONFIG_PREFIX=${ROOTFS} install
cd ${ROOTFS}
echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc --print-sysroot)
cd ${ROOTFS}
cp -ar ${SYSROOT}/lib/* lib/
cp -ar ${SYSROOT}/lib64/* lib64/
# TODO: Make device nodes
sudo mknod -m 622 dev/console c 5 1
sudo mknod -m 666 dev/null c 1 3
# TODO: Clean and build the writer utility

cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${FINDER_APP_DIR}/finder.sh ${ROOTFS}/home/
cp ${FINDER_APP_DIR}/writer ${ROOTFS}/home/
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${ROOTFS}/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${ROOTFS}/home/
cp -a ${FINDER_APP_DIR}/conf ${ROOTFS}/home/
mkdir -p ${ROOTFS}/conf
cp -r ${FINDER_APP_DIR}/../conf/* ${ROOTFS}/conf/

# TODO: Chown the root directory
cd ${ROOTFS}
sudo chown -R root:root *
# TODO Create initramfs.cpio.gz

cd ${OUTDIR}
cp linux-stable/arch/arm64/boot/Image .
cd ${ROOTFS}
sudo find . | cpio -H newc  -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
#finished creating basic image!
