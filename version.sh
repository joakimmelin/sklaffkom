#!/bin/sh
VERSION=1.33-beta9a
COMPILE=`cat .compile`
COMPILE=`expr ${COMPILE:-0} + 1`
echo $COMPILE > .compile
echo 'char *sklaff_version = "'$VERSION'(#'$COMPILE')";' > version.c
echo "Building SklaffKOM "$VERSION"("$COMPILE")"
