#!/bin/sh

# any scripting support you need for running the test. (e.g. creating
# image files, running the actual test executable)

# disk with 3 disks
for i in 1 2 3; do
    disks3="$disks3 /tmp/$USER-disk$i.img"
done

# use 'dd' to create 3 disk images, each with 256 512-byte blocks
for d in $disks3; do
    # note that 2<&- suppresses dd info messages
    dd if=/dev/zero bs=512 count=256 of=$d 2<&- 
done

# disks with 5 disks
for i in 1 2 3 4 5; do
    disks5="$disks5 /tmp/$USER-newdisk$i.img"
done

# use 'dd' to create 5 disk images, each with 256 512-byte blocks
for d in $disks5; do
    # note that 2<&- suppresses dd info messages
    dd if=/dev/zero bs=512 count=502 of=$d 2<&- 
done


# run the test and clean up

./stripe-test 2 $disks3
#./stripe-test 4 $disks3
#./stripe-test 7 $disks3
#./stripe-test 2 $disks5
#./stripe-test 4 $disks5
#./stripe-test 7 $disks5
#./stripe-test 32 $disks5

rm -f $disks3
rm -f $disks5
