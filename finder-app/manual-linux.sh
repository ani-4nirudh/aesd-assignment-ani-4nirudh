#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=$(aarch64-none-linux-gnu-gcc -print-sysroot)

if [ $# -lt 1 ]; then
  echo "Using default directory ${OUTDIR} for output"
else
  OUTDIR=$1
  echo "Using passed directory ${OUTDIR} for output"
fi

# Check directory creation
if [[ ! -d ${OUTDIR} ]]; then
  echo -e "\n -- Directory could not be found!"
  echo "-- Creating directory ..."
  mkdir -p ${OUTDIR}
fi

# echo "Installing additional dependencies..."
# sudo apt update
# sudo apt install -y flex bison build-essential libssl-dev libelf-dev
# echo "SUCCESS: Installed additional dependencies"

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

  # TODO: Add your kernel build steps here

  # Step 1: Clean
  make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} mrproper

  # Step 2: Defconfig build
  make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} defconfig

  # Step 3: Build vmlinux target
  make -j4 ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} all

  # Step 4: Build modules and device tree
  # make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-modules
  make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo -e "\n-- Adding the Image in outdir"
cp -rv ${OUTDIR}/linux-stable/arch/arm64/boot/Image ${OUTDIR}

echo -e "\n-- Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]; then
  echo -e "\n-- Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
  sudo rm -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "-- Creating the root filesystem ..."
mkdir -p "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc lib lib64 proc sbin sys tmp usr var home
mkdir -p usr/bin usr/sbin usr/lib
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]; then
  git clone git://busybox.net/busybox.git
  cd busybox
  git checkout ${BUSYBOX_VERSION}

  # TODO:  Configure busybox
  make distclean
  make defconfig
else
  cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd ${OUTDIR}/rootfs

echo -e "\n ----- Library dependencies -----"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"
echo "----------------------"

# TODO: Add library dependencies to rootfs
cp ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/
cp ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/
cp ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
cd "${FINDER_APP_DIR}"

echo -e "\n-- Cleaning the writer app..."
make clean

echo -e "\n-- Compiling 'writer' ..."
make CROSS_COMPILE=${CROSS_COMPILE}

echo -e "\n-- Copying 'writer' executable ..."
cp -rv "${FINDER_APP_DIR}/writer" "${OUTDIR}/rootfs/home/"

cd "${OUTDIR}"

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo -e "\n-- Copying files from the finder-app directory ..."
cp -rv "${FINDER_APP_DIR}/finder.sh" "${OUTDIR}/rootfs/home/"
cp -rv "${FINDER_APP_DIR}/finder-test.sh" "${OUTDIR}/rootfs/home/"
cp -rv "${FINDER_APP_DIR}/autorun-qemu.sh" "${OUTDIR}/rootfs/home/"
mkdir -p "${OUTDIR}/rootfs/home/conf"
cp -rv "${FINDER_APP_DIR}/conf/username.txt" "${OUTDIR}/rootfs/home/conf/"
cp -rv "${FINDER_APP_DIR}/conf/assignment.txt" "${OUTDIR}/rootfs/home/conf/"

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd "${OUTDIR}/rootfs"
# Compress the root file system and copy it to the main directory
find . | cpio -H newc -ov --owner root:root >${OUTDIR}/initramfs.cpio
cd "${OUTDIR}"
echo -e "\n-- Compressing the initramfs.cpio file ..."
gzip -f initramfs.cpio
echo -e "\n -- Listing the file in human readable format ..."
ls -lah initramfs.cpio.gz
