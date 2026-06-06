#!/bin/sh
VERSION='1.33rc1'
COMPILE=`cat .compile`
COMPILE=`expr ${COMPILE:-0} + 1`
echo $COMPILE > .compile
echo 'char *sklaff_version = "'$VERSION'(#'$COMPILE')";' > version.c
echo 'char *sklaff_build_date = __DATE__;' >> version.c
echo 'char *sklaff_build_time = __TIME__;' >> version.c

echo "Building SklaffKOM "$VERSION"("$COMPILE")"
