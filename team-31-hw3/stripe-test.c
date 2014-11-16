#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "blkdev.h"

int main(int argc, char **argv)
{
    /* This code reads arguments <stripesize> d1 d2 d3 ...
     * Note that you don't need any extra images, as there's no way to
     * repair a striped volume after failure.
     */
    struct blkdev *disks[10];
    int i, ndisks, stripesize = atoi(argv[1]);

    for (i = 2, ndisks = 0; i < argc; i++)
        disks[ndisks++] = image_create(argv[i]);

    /* and creates a striped volume with the specified stripe size
     */
    struct blkdev *striped = striped_create(ndisks, disks, stripesize);
    assert(striped != NULL);

    /* your tests here */
    
   /* from pdf:
   - reports the correct size
   - reads data from the right disks and locations. (prepare disks
     with known data at various locations and make sure you can read it back) 
   - overwrites the correct locations. i.e. write to your prepared disks
     and check the results (using something other than your stripe
     code) to check that the right sections got modified. 
   - fail a disk and verify that the volume fails.
   - large (> 1 stripe set), small, unaligned reads and writes
     (i.e. starting, ending in the middle of a stripe), as well as small
     writes wrapping around the end of a stripe. 
   - Passes all your other tests with different stripe sizes (e.g. 2,
     4, 7, and 32 sectors) and different numbers of disks.  
 	*/
     
	// - reports the correct size
	int stripe_volume_size = ((disks[0]->ops->num_blocks(disks[0]) / stripesize) * stripesize) * ndisks;
	assert(striped->ops->num_blocks(striped) == stripe_volume_size);

printf("correct size\n");
	// - reads data from the right disks and locations
	char src[BLOCK_SIZE*stripesize*ndisks];
	for(i = 0; i < ndisks; i++)
	{
		memset(src+(BLOCK_SIZE*stripesize*i), 'a'+i, BLOCK_SIZE*stripesize);
	}
	
	int result = striped->ops->write(striped, 0, stripesize*ndisks, src);
	assert(result == SUCCESS);

	char dst[BLOCK_SIZE*stripesize*ndisks];
	result = striped->ops->read(striped, 0, stripesize*ndisks, dst);
	assert(result == SUCCESS);
	assert(memcmp(src, dst, stripesize*ndisks*BLOCK_SIZE) == 0);
printf("read from right disks and locations\n");
	// - overwrites the correct locations
	char src1[BLOCK_SIZE*(stripesize + 1)];
	memset(src1, 'z', BLOCK_SIZE*(stripesize + 1));

	result = striped->ops->write(striped, (stripesize -1), (stripesize +1), src1);
	assert(result == SUCCESS);
	
	char dst1[BLOCK_SIZE*(stripesize+1)];
	result = disks[0]->ops->read(disks[0], stripesize-1, 1, dst1);
	assert(result == SUCCESS);
	result = disks[1]->ops->read(disks[1], 0, stripesize, dst1+BLOCK_SIZE);
	assert(result == SUCCESS);
	assert(memcmp(src1, dst1, BLOCK_SIZE*(stripesize+1)) == 0);
printf("overwrites correct locations\n");
	// - small writes wrapping around the end of a stripe
	result = striped->ops->write(striped, stripesize-1, stripesize*ndisks, src);
	assert(result == SUCCESS);

	result = striped->ops->read(striped, stripesize-1, stripesize*ndisks, dst);
	assert(result == SUCCESS);
	assert(memcmp(src, dst, stripesize*ndisks*BLOCK_SIZE) == 0);


	result = striped->ops->write(striped, stripesize+1, stripesize*ndisks, src);
	assert(result == SUCCESS);

	result = striped->ops->read(striped, stripesize+1, stripesize*ndisks, dst);
	assert(result == SUCCESS);
	assert(memcmp(src, dst, stripesize*ndisks*BLOCK_SIZE) == 0);
printf("small writes\n");	
	// - check large read and write
	char src2[BLOCK_SIZE*stripesize*ndisks*15];
	for(i = 0; i < ndisks*15; i++)
	{
		memset(src2+(BLOCK_SIZE*stripesize*i), 'a'+i, BLOCK_SIZE*stripesize);
	}

	result = striped->ops->write(striped, 0, stripesize*ndisks*15, src2);
	assert(result == SUCCESS);

	char dst2[BLOCK_SIZE*stripesize*ndisks*15];
	result = striped->ops->read(striped, 0, stripesize*ndisks*15, dst2);
	assert(result == SUCCESS);
	assert(memcmp(src2, dst2, stripesize*ndisks*BLOCK_SIZE*15) == 0);
printf("large write and read\n");
	// - fail a disk and verify that the volume fails
	image_fail(disks[ndisks-1]);
	result = striped->ops->read(striped, 0, stripesize*ndisks, dst);
	assert(result == E_UNAVAIL);
	result = striped->ops->read(striped, 0, stripesize*(ndisks-1), dst);
	assert(result == SUCCESS);
printf("fail read\n");
	result = striped->ops->write(striped, 0, stripesize*ndisks, src);
	printf("write 1\n");
	assert(result == E_UNAVAIL);
	result = striped->ops->write(striped, 0, stripesize*(ndisks-1), src);
	assert(result == SUCCESS);
printf("fail\n");
    printf("Striping Test: SUCCESS\n");
    return 0;
}
