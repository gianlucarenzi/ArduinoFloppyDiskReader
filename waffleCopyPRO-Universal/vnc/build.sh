#!/bin/bash
#
# $Id: build.sh,v 1.1 2016/09/06 12:19:14 gianluca Exp $
#

PRJROOT=$1

cd $PRJROOT

make clean 2>/dev/null
make

exit $?
