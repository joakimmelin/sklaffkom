#! /bin/sh

PATH=~/bin:"$PATH" # fucking stupid systems _everywhere_
echo $PATH

tarfile="$1.tar.gz"
rm -rf lrzsz-build
mkdir "lrzsz-build" || exit 1
cd "lrzsz-build" || exit 1
tar xzf "../$tarfile" || exit 1
cd "$1" || exit 1
./configure -q
make -s V=0 buildcheck+
