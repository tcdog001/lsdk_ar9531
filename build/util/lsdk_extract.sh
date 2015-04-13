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

function usage()
{
cat <<EOF
usage: $0 options script_file

OPTIONS:
  -d  (mandatory) Directory package to parse
  -p  (mandatory) Package name (will be used to create output dirs)
  file(mandatory) File containing the make commands

This script parses the LSDK build structure and figure out which Makefile
variables is used in a particular directory
EOF
}

DIR=
PKG=
while getopts "hd:p:b:" OPTION
do
  case $OPTION in
    h)
      usage
      exit 1
      ;;
    d)
      DIRS="$DIRS $OPTARG"
      ;;
    p)
      PKG=$OPTARG
      ;;
    ?)
      usage
      exit
      ;;
  esac
done

shift $(echo $OPTIND-1|bc)
SCRIPT_FILE=$1

if [  -z "$DIRS" -o -z "$PKG" -o -z "${SCRIPT_FILE}" ]
then
  usage
  exit 1
fi

for dir in $DIRS;do
  if [ ! -d "$dir" ]; then
    echo "$dir: folder does not exist"
    usage
    exit 1
  fi
done

if [ ! -f "${SCRIPT_FILE}" ]; then
  echo "$SCRIPT_FILE: file does not exist"
  exit 1
fi

rm -f ${PKG}_vars.mk
echo "show${PKG}vars:" > ${PKG}_vars.mk
echo -e "\trm -f \${IMAGEPATH}/configs/${PKG}.config" >> ${PKG}_vars.mk
echo -e "\tmkdir -p \${IMAGEPATH}/configs/" >> ${PKG}_vars.mk
for i in `grep -r 'export.*=' scripts/ | grep -v ':\s*#' | grep -v MAKEARCH | grep -v SAMBA_MIPS_74K_CFLAGS| sed 's,.*export \(.*\)=.*,\1,'|sort|uniq|grep -v MAKEARCH|grep -v ':' | grep -v '\?' | grep -v '\+' `;do if grep -rqw $i $DIRS; then echo $i;fi;done |tee -a ${PKG}_vars.mk.tmp
sed -i "s/\(.*\)/@echo \1=\"\${\1}\" >> \${IMAGEPATH}\/configs\/${PKG}.config/" ${PKG}_vars.mk.tmp
echo "sed -i 's/^.*=\$\$//' \${IMAGEPATH}/configs/${PKG}.config" >> ${PKG}_vars.mk.tmp
echo "sed -i '/^\$\$/d' \${IMAGEPATH}/configs/${PKG}.config" >> ${PKG}_vars.mk.tmp
sed -i 's/\(.*\)/\t\1/' ${PKG}_vars.mk.tmp
cat ${PKG}_vars.mk.tmp >> ${PKG}_vars.mk && rm ${PKG}_vars.mk.tmp

chmod u+w Makefile
grep -q '^-include \*.mk$' Makefile || \
  echo '-include *.mk' >> Makefile

while read line; do
  [[ "${line}" =~ .*make.* ]] || continue
  eval ${line} show${PKG}vars
done < ${SCRIPT_FILE}
