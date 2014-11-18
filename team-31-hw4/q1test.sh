#!/bin/bash
#
# file:        q1test.sh
# description: test script for command line read-only access
#

cmd=./homework
origdisk=disk1.img

for f in $cmd $origdisk; do
  if [ ! -f $f ] ; then
      echo "Unable to access: $f"
      exit
  fi
done

disk=/tmp/$USER-dis1.img
cp $origdisk $disk

echo Test 1 - cmd-line read-only access

$cmd --cmdline $disk > /tmp/output-$$ <<EOF
ls
ls-l file.txt
show file.txt
cd home
ls-l small-1
show small-1
cd ..
ls work
cd work/dir-2
pwd
cd ..
cd dir-1
ls-l small-4
show small-4
statfs
EOF
echo >> /tmp/output-$$
[ "$verbose" ] && echo wrote /tmp/output-$$

diff - /tmp/output-$$ <<EOF
read/write block size: 1000
cmd> ls
another-file
dir_other
file.txt
home
work
cmd> ls-l file.txt
/file.txt -rw-r--r-- 1800 2
cmd> show file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
cmd> cd home
cmd> ls-l small-1
/home/small-1 -rw-r--r-- 216 1
cmd> show small-1
small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1
small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1
small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1
cmd> cd ..
cmd> ls work
dir-1
dir-2
cmd> cd work/dir-2
cmd> pwd
/work/dir-2
cmd> cd ..
cmd> cd dir-1
cmd> ls-l small-4
/work/dir-1/small-4 -rw-r--r-- 160 1
cmd> show small-4
small-4 small-4 small-4 small-4 small-4 small-4 small-4 small-4 small-4
small-4 small-4 small-4 small-4 small-4 small-4 small-4 small-4 small-4
small-4 small-4
cmd> statfs
max name length: 43
block size: 1024
cmd> 
EOF

if [ $? != 0 ] ; then
    echo 'Some tests failed - check the outputs for reference'
else
    echo 'All tests passed'
fi
[ "$verbose" ] || rm -f /tmp/output-$$
