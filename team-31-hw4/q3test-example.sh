#!/bin/bash
#
# file:        q3test-example.sh
# description: test script for FUSE version of on-disk FS
#
# CS 7600, Intensive Computer Systems, Northeastern CCIS
# Peter Desnoyers, April 2010
# $Id: q4test.sh 163 2010-04-20 19:51:46Z pjd $
#

MNT=/tmp/mnt-$$
failedtests=
TMP=/tmp/tmp-$$

DISK=/tmp/disk.$$.img
./mkfs-cs5600fs --create 1m $DISK
