TMP=$USER.test.$$
trap "rm -f $TMP" 0
RESULT=PASSED

for i in 1 2 3 4 5; do
    disks="$disks test$i.img"
done

for d in $disks; do
    dd if=/dev/zero bs=512 count=9 of=$d 2<&- 
done

./raid4-test 2 test1.img test2.img test3.img test4.img test5.img > $TMP || RESULT=FAILED
if grep -q 'SUCCESS' $TMP ; then
    echo 'count=9 SUCCESS'
else
    echo ' count=9 Fail'
fi

for d in $disks; do
    dd if=/dev/zero bs=512 count=24 of=$d 2<&- 
done
./raid4-test 5 test1.img test2.img test3.img test4.img test5.img > $TMP || RESULT=FAILED
if grep -q 'SUCCESS' $TMP ; then
    echo 'count=24 SUCCESS'
else
    echo ' count=24 Fail'
fi

for d in $disks; do
    dd if=/dev/zero bs=512 count=24 of=$d 2<&- 
done
./raid4-test 7 test1.img test2.img test3.img test4.img test5.img > $TMP || RESULT=FAILED
if grep -q 'SUCCESS' $TMP ; then
    echo 'count=24 SUCCESS'
else
    echo ' count=24 Fail'
fi
rm -f $disks
rm -f $TMP