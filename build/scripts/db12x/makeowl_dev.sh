#!/bin/bash

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

# source the variables needed for build
TOP=$1/build
cd $TOP

make BOARD_TYPE=ar7240_emu BUILD_TYPE=jffs2 common_fusion
make BOARD_TYPE=ar7240_emu BUILD_TYPE=jffs2 fusion_build
make BOARD_TYPE=ar7240_emu BUILD_TYPE=ram ram_build

# Nothing special in u-boot for fusion
make BOARD_TYPE=ar7240_emu uboot

echo "---------------------"
find . -name \* -user root -exec sudo chown build {} \; -print
find . -name \.config  -exec chmod 777 {} \; -print
