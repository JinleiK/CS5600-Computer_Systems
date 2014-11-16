#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "blkdev.h"

/* example main() function that takes several disk image filenames as
 * arguments on the command line.
 * Note that you'll need three images to test recovery after a failure.
 */
int main(int argc, char **argv)
{
    /* create the underlying blkdevs from the images
     */
    struct blkdev *d1 = image_create(argv[1]);
    struct blkdev *d2 = image_create(argv[2]);
    /* ... */

    /* create the mirror
     */
    struct blkdev *disks[] = {d1, d2};
    struct blkdev *mirror = mirror_create(disks);

    /* two asserts - that the mirror create worked, and that the size
     * is correct. 
    */
    // creates a volume properly
    assert(mirror != NULL);
    // returns the correct length
    assert(mirror->ops->num_blocks(mirror) == d1->ops->num_blocks(d1));

    /* put your test code here. Errors should exit via either 'assert'
     * or 'exit(1)' - that way the shell script knows that the test failed.
     */

    /* suggested test codes (from the assignment PDF)
         You should test that your code:
          - creates a volume properly
          - returns the correct length
          - can handle reads and writes of different sizes, and
            returns the same data as was written 
          - reads data from the proper location in the images, and
            doesn't overwrite incorrect locations on write. 
          - continues to read and write correctly after one of the
            disks fails. (call image_fail() on the image blkdev to
            force it to fail) 
          - continues to read and write (correctly returning data
            written before the failure) after the disk is replaced. 
          - reads and writes (returning data written before the first
            failure) after the other disk fails
    */
    
    // can handle reads and writes of different sizes, and
    // returns the same data as was written
    char buf[BLOCK_SIZE * 16];
    FILE *f = fopen("/dev/urandom", "r");
    assert(fread(buf, sizeof(buf), 1, f) == 1);
    fclose(f);
    // reads data from the proper location in the images, and
    // doesn't overwrite incorrect locations on write. 
    int i, result;
    //illegal write
    int num_blks = mirror->ops->num_blocks(mirror);
    result = mirror->ops->write(mirror, -1, 16, buf);
    assert(result == E_BADADDR);
    result = mirror->ops->write(mirror, num_blks - 1, 16, buf);
    assert(result == E_BADADDR);
    result = mirror->ops->write(mirror, num_blks + 1, 16, buf);
    assert(result == E_BADADDR);

    // normal write
    for(i = 0; i < 128; i += 16)
    {
      result = mirror->ops->write(mirror, i, 16, buf);
      assert(result == SUCCESS);
    }
    // normal read
    char dest[BLOCK_SIZE * 16];
    for(i = 0; i < 128; i += 16)
    {
      result = mirror->ops->read(mirror, i, 16, dest);
      assert(result == SUCCESS);
      assert(memcmp(buf, dest, BLOCK_SIZE*16) == 0);
    }

    // continues to read and write correctly after one of the
    // disks fails.
    image_fail(d1);
    for(i = 0; i < 128; i += 16)
    {
      result = mirror->ops->read(mirror, i, 16, dest);
      assert(result == SUCCESS);
      assert(memcmp(buf, dest, BLOCK_SIZE*16) == 0);
    }

    for(i = 0; i < 128; i += 16)
    {
      result = mirror->ops->write(mirror, i, 16, buf);
      assert(result == SUCCESS);
    }

    // continues to read and write (correctly returning data
    // written before the failure) after the disk is replaced. 
    struct blkdev *d3 = image_create(argv[3]);
    assert(mirror_replace(mirror, 0, d3) == SUCCESS);
    for(i = 0; i < 128; i += 16)
    {
      result = mirror->ops->read(mirror, i, 16, dest);
      assert(result == SUCCESS);
      assert(memcmp(buf, dest, BLOCK_SIZE*16) == 0);
    }

    for(i = 0; i < 128; i += 16)
    {
      result = mirror->ops->write(mirror, i, 16, buf);
      assert(result == SUCCESS);
    }


    // reads and writes (returning data written before the first
    // failure) after the other disk fails
    image_fail(d2);
    for(i = 0; i < 128; i += 16)
    {
      result = mirror->ops->read(mirror, i, 16, dest);
      assert(result == SUCCESS);
      assert(memcmp(buf, dest, BLOCK_SIZE*16) == 0);
    }

    for(i = 0; i < 128; i += 16)
    {
      result = mirror->ops->write(mirror, i, 16, buf);
      assert(result == SUCCESS);
    };

    // reads and writes after both disks fail
    image_fail(d3);
    assert(mirror->ops->read(mirror, 0, 16, dest) == E_UNAVAIL);
    assert(mirror->ops->write(mirror, 0, 16, buf) == E_UNAVAIL);

    printf("Mirror Test: SUCCESS\n");
    return 0;
}
