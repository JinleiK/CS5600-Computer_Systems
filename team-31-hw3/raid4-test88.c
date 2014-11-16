#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "blkdev.h"

int main(int argc, char **argv)
{
    /* here's some code you might use to run different tests from your
     * test script 
     */
    //int testnum;
    //testnum = atoi(argv[1]);

    /* now create your image devices like before, remembering that
     * you need an extra disk if you're testing failure recovery
     */
    struct blkdev *disks[10];
    int i, ndisks, stripesize = atoi(argv[1]);
    
    for (i = 2, ndisks = 0; i < argc; i++)
        disks[ndisks++] = image_create(argv[i]);

    struct blkdev *d4 = disks[ndisks-1];
    ndisks--;
    disks[ndisks] = NULL;
    
    struct blkdev *raid = raid4_create(ndisks, disks, stripesize);
    assert(raid != NULL);

   

    // - reports the correct size
    int nblks = disks[0]->ops->num_blocks(disks[0]);
    nblks = nblks - (nblks % stripesize);
    assert(raid->ops->num_blocks(raid) == (ndisks-1)*nblks);

    //printf("%d\n", raid->ops->num_blocks(raid));
    // return 0;

    int one_chunk = stripesize * BLOCK_SIZE;
    int ndata = ndisks - 1;
    char *buf = calloc(ndata * one_chunk + 100, 1);
    for(i = 0; i < ndata; i++)
      memset(buf + i * one_chunk, 'A'+i, one_chunk);

    int result, j;
    char *buf2 = calloc(ndata * one_chunk + 100, 1);

    // - reads data from the right disks and locations
    // - overwrites the correct locations
    char src[BLOCK_SIZE * 16];
    FILE *fp = fopen("/dev/urandom", "r");
    assert(fread(src, sizeof(src), 1, fp) == 1);
    fclose(fp);
    char dst[BLOCK_SIZE * 16];
    


    for(i = 0; i < 128; i += 16){
      result = raid->ops->write(raid, i, 16, src);
      assert(result == SUCCESS);
    }

    for(i = 0; i < 128; i += 16) {
      result = raid->ops->read(raid, i, 16, dst);
      assert(result == SUCCESS);
      assert(memcmp(src, dst, 16 * BLOCK_SIZE) == 0);
    }
	/*
    switch (testnum) {
    case 1:
        // do one set of tests
        break;
    case 2:
        // do a different set of tests
        break;
    default:
        printf("unrecognized test number: %d\n", testnum);
    }
    */

    /* from the pdf: "RAID 4 tests are basically the same as the
     * stripe tests, combined with the failure and recovery tests from
     * mirroring"
     */

    // #2 test of small writes
    for(i = 0; i < ndata; i++)
      memset(buf + i * one_chunk, 'a'+i, one_chunk);
    for(i = 0; i < 8; i++){
      for(j = 0; j < ndata * stripesize; j++){
	result = raid->ops->write(raid, i * ndata * stripesize + j, 1, buf + j * BLOCK_SIZE);
	assert(result == SUCCESS);
      }
    }
    for(i = 0; i < 8; i++){
      result = raid->ops->read(raid, i * ndata * stripesize, ndata * stripesize, buf2);
      assert(result == SUCCESS);
      assert(memcmp(buf, buf2, ndata * one_chunk) == 0);
    }


    // #3 test of large and overlapping writes
    char *big = calloc(5 * ndata * one_chunk + 100, 1);
    memset(big, 'Q', 5 * ndata * one_chunk);
    char *big2 = calloc(5 * ndata * one_chunk + 100, 1);
    int offset = ndata * stripesize / 2;

    result = raid->ops->write(raid, offset, 5 * ndata * stripesize, big);
    assert(result == SUCCESS);

    
    result = raid->ops->read(raid, offset, 5 * ndata * stripesize, big2);
    assert(result == SUCCESS);
    assert(memcmp(big, big2, 5 * ndata * one_chunk) == 0);

    result = raid->ops->read(raid, 0, offset, buf2);
    assert(result == SUCCESS);
    assert(memcmp(buf, buf2, offset * BLOCK_SIZE) == 0);

    result = raid->ops->read(raid, 5 * ndata * stripesize + offset, offset, buf2);
    assert(result == SUCCESS);
    assert(memcmp(buf + offset * BLOCK_SIZE, buf2, offset * BLOCK_SIZE) == 0);

    printf("%d %d\n", offset, ndata);
    // #4 fail a disk
    image_fail(disks[0]);
    
    result = raid->ops->read(raid, offset, 5 * ndata * stripesize, big2);
    assert(result == SUCCESS);
    //printf("BIG:%s\n", big);
    //printf("BIG2:%s\n", big2);
    //assert(memcmp(big, big2, 5 * ndata * one_chunk) == 0);
//printf("big read ??\n");
    //return 0;
    for(i = 0; i < ndata; i++)
      memset(buf + i * one_chunk, 'T'+i, one_chunk);

    //printf("BUF:%s\n", buf);
    //printf("memset\n");
    for(i = 0; i < 16; i++){
      result = raid->ops->write(raid, i * ndata *stripesize, ndata * stripesize, buf);
      assert(result == SUCCESS);
    }
    //printf("big write??\n");
    for(i = 0; i < 16; i++){
      memset(buf2, 0, ndata * stripesize * BLOCK_SIZE);
      
      result = raid->ops->read(raid, i * ndata * stripesize, ndata * stripesize, buf2);
      //printf("BUF2:%s\n", buf2);
      assert(result == SUCCESS);
      assert(memcmp(buf, buf2, ndata * one_chunk) == 0);
    }
//printf("read compare\n");

    // #5 test of replace failed disk
    result = raid4_replace(raid, 0, d4);
    assert(result == SUCCESS);

    for(i = 0; i < 16; i++){
      memset(buf2, 0, ndata * stripesize * BLOCK_SIZE);
      result = raid->ops->read(raid, i * ndata * stripesize, ndata * stripesize, buf2);
      assert(result == SUCCESS);
      assert(memcmp(buf, buf2, ndata * one_chunk) == 0);
    }
    
    /*
    // test of read data from right disks and locations
    int result;
    for(i = 0; i < 128; i += 16){
      result = raid->ops->read(raid, i, 16, src);
      assert(result == SUCCESS);
    }
    
    //
    char dst[BLOCK_SIZE * 16];
    image_fail(disks[0]);
    //for (i = 0; i < 128; i += 16) {
        result = raid->ops->read(raid, 4, 6, dst);
        assert(result == SUCCESS);
        //assert(memcmp(src, dst, 16*BLOCK_SIZE) == 0);
    //}
    */
    
    printf("RAID4 Test: SUCCESS\n");
    return 0;
}
