#!/bin/bash
TIMEOUTSEC=1.5
while [ true ]
do
  ISDISK=$(./waffle-fuse --probe /dev/ttyUSB0)
  if [ "${ISDISK}" == "disk" ]
  then
      break
  fi
  sleep ${TIMEOUTSEC}
done

mkdir -p /tmp/waffledisk

# Check for Amiga DS/DD first
./waffle-fuse /dev/ttyUSB0  /tmp/waffledisk -o format=affs

ISVALID=0
COUNT=0
# Wait for validating disk
while [ true ]
do
	ISMOUNT=$(mount | grep /tmp/waffledisk)
	if [ "${ISMOUNT}" != "" ]
        then
            ISVALID=1
            break
        fi
        sleep 1
        COUNT=$((${COUNT}+1))
        if [ "${COUNT}" -gt 10 ]
        then
            break
        fi
done

if [ ${ISVALID} -eq 1 ]
then
    echo "Amiga 880K disk"
else
    echo "Another format"
fi

exit 0

