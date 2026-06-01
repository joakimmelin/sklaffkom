#!/bin/sh
VERSION='1.33rc1'
COMPILE=`cat .compile`
COMPILE=`expr ${COMPILE:-0} + 1`
echo $COMPILE > .compile
echo 'char *sklaff_version = "'$VERSION'(#'$COMPILE')";' > version.c
echo "Building SklaffKOM "$VERSION"("$COMPILE")"
