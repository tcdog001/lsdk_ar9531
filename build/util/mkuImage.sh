#!/bin/sh -x

#
# Copyright (c) 2013 Qualcomm Atheros, Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# 
# $1 == u-boot/tools path
# $2 == kernel tree path
# $3 == optional additions to filename

MKIMAGE=$1/mkimage
VMLINUX=$2/vmlinux
VMLINUXBIN=$2/arch/mips/boot/vmlinux.bin
# NOTE You can direct the outputs elsewhere by pre-defining TFTPPATH
if [ -z "$TFTPPATH" ]
then
    TFTPPATH=/tftpboot/`whoami`
fi
echo $0 Using TFTPPATH=$TFTPPATH  ###### DEBUG

ENTRY=`readelf -a ${VMLINUX}|grep "Entry"|head -1|cut -d":" -f 2`
LDADDR=`readelf -a ${VMLINUX}|grep "\[ 1\]"|head -1|cut -d" " -f 26`

# gzip -f ${VMLINUXBIN}

if [ $# -gt 3 ]
then
	suffix=$3
else
	suffix=
fi

if [ "xx$4" = "xxlzma" -o "xx$3" = "xxlzma" ]
then
	echo "**** Generating vmlinux${suffix}.lzma.uImage ********"
	${MKIMAGE} \
		-A mips -O linux -T kernel -C lzma \
		-a 0x${LDADDR} -e ${ENTRY} -n "Linux Kernel Image" \
		-d ${VMLINUXBIN}.lzma ${IMAGEPATH}/vmlinux${suffix}.lzma.uImage
	cp ${IMAGEPATH}/vmlinux${suffix}.lzma.uImage ${TFTPPATH}
else
	echo "**** Generating vmlinux${suffix}.gz.uImage ********"
	${MKIMAGE} \
		-A mips -O linux -T kernel -C gzip \
		-a 0x${LDADDR} -e ${ENTRY} -n "Linux Kernel Image" \
		-d ${VMLINUXBIN}.gz ${IMAGEPATH}/vmlinux${suffix}.gz.uImage
	cp ${IMAGEPATH}/vmlinux${suffix}.gz.uImage ${TFTPPATH}
fi
