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
     //CASE 1: creates a volume properly and reports the correct size
     struct blkdev *disks[10];
    int i, ndisks;
    int nunits = atoi(argv[1]);
    int result,read_write_correct,start,end;
    for (i = 2, ndisks = 0; i < argc - 1; i++)
    {
        printf("%s\n",argv[i]);
        disks[ndisks++] = image_create(argv[i]);
    }
    ndisks = ndisks-1;
    struct blkdev *raid4 = raid4_create(ndisks+1, disks, nunits);
    assert(raid4 != NULL);
    int disk_sectors = disks[0]->ops->num_blocks(disks[0]);
    int size_blks = 512;
    int size = disk_sectors/ nunits * nunits * (ndisks);
    printf("%d\n",size);
    assert(size == raid4->ops->num_blocks(raid4));
    
    //CASE 2: small read and write
    //read and write the first block of the first disk
     char *buf = (char*)malloc(size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    memset(buf,'b',size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    result = raid4->ops->write(raid4, 0, raid4->ops->num_blocks(raid4), buf);
    assert(result == SUCCESS);
    char *buf2 = (char*)malloc(size_blks * nunits * sizeof(char));
    memset(buf2,'a',size_blks * nunits * sizeof(char));
    result = raid4->ops->write(raid4, 0, nunits, buf2);
    assert(result == SUCCESS);
    
    char *buf3 = (char*)malloc(size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    result = raid4->ops->read(raid4, 0, raid4->ops->num_blocks(raid4), buf3);
    assert(result == SUCCESS);
    
    read_write_correct = 1;
    start = 0;
    end = (start + nunits * size_blks) -1;
    
    for (i = 0; i < raid4->ops->num_blocks(raid4) * size_blks; i++)
    {
        if (start <= i && i <= end)
        {
            if (buf3[i] != 'a')
            {
                printf("a: %d,%c\n",i,buf3[i]);
                read_write_correct = 0;
                break;
            }
        }
        else
        {
            if (buf3[i] != 'b')
            {
                printf("b: %d,%c\n",i,buf3[i]);
                read_write_correct = 0;
                break;
            }
        }
    }
    free(buf);
    free(buf2);
    free(buf3);
    buf = NULL;
    buf2 = NULL;
    buf3 = NULL;
    assert(read_write_correct == 1);

    //CASE 3: large and unaligned read and write
    //starting, ending in the middle of a stripe
     char *buf4 = (char*)malloc(size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    memset(buf4,'b',size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    raid4->ops->write(raid4, 0, raid4->ops->num_blocks(raid4), buf4);
    
    char *buf5 = (char*)malloc(size_blks * nunits * ndisks * 2 * sizeof(char));
    memset(buf5,'a',size_blks * nunits * ndisks * 2 * sizeof(char));
    result = raid4->ops->write(raid4, nunits * ndisks / 2, nunits * ndisks * 2, buf5);
    assert(result == SUCCESS);
    
    char *buf6 = (char*)malloc(size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    result = raid4->ops->read(raid4, 0, raid4->ops->num_blocks(raid4), buf6);
    assert(result == SUCCESS);
    
    read_write_correct = 1;
    start = nunits * ndisks/ 2 ;
    end = start + nunits * ndisks * 2;
    for (i = 0; i < raid4->ops->num_blocks(raid4) * size_blks; i++)
    {
        if (start*size_blks <= i && i <= end*size_blks-1)
        {
            if (buf6[i] != 'a')
            {
                printf("a %c\n",buf6[i]);
                read_write_correct = 0;
                break;
            }
        }
        else
        {
            if (buf6[i] != 'b')
            {
                printf("b %c\n",buf6[i]);
                printf("%d\n",i);
                read_write_correct = 0;
                break;
            }
        }
    }
    free(buf4);
    free(buf5);
    free(buf6);
    buf4 = NULL;
    buf5 = NULL;
    buf6 = NULL;
    assert(read_write_correct == 1);

    //CASE 4: continues to read and write correctly after one of the disks fails
    char *buf7 = (char*)malloc(size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    memset(buf7,'b',size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    raid4->ops->write(raid4, 0, raid4->ops->num_blocks(raid4), buf7);
    
    image_fail(disks[0]);
    char *buf8 = (char*)malloc(size_blks * nunits * sizeof(char));
    memset(buf8,'a',size_blks * nunits * sizeof(char));
    result = raid4->ops->write(raid4, 0, nunits, buf8);
    assert(result == SUCCESS);
    
    char *buf9 = (char*)malloc(size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    result = raid4->ops->read(raid4, 0, raid4->ops->num_blocks(raid4), buf9);
    assert(result == SUCCESS);
    
    read_write_correct = 1;
    start = 0;
    end = (start + nunits * size_blks) -1;
    
    for (i = 0; i < raid4->ops->num_blocks(raid4) * size_blks; i++)
    {
        if (start <= i && i <= end)
        {
            if (buf9[i] != 'a')
            {
                read_write_correct = 0;
                break;
            }
        }
        else
        {
            if (buf9[i] != 'b')
            {
                read_write_correct = 0;
                break;
            }
        }
    }
    free(buf7);
    free(buf8);
    free(buf9);
    buf7 = NULL;
    buf8 = NULL;
    buf9 = NULL;
    assert(read_write_correct == 1);

    //CASE 5: continues to read and write (correctly returning data
    //written before the failure) after the disk is replaced
    disks[ndisks+1] = image_create(argv[argc - 1]);
    raid4_replace(raid4, 0, disks[ndisks]);
    
    char *buf12 = (char*)malloc(size_blks * raid4->ops->num_blocks(raid4) * sizeof(char));
    result = raid4->ops->read(raid4, 0, raid4->ops->num_blocks(raid4), buf12);
    assert(result == SUCCESS);
    
    read_write_correct = 1;
    start = 0;
    end = (start + nunits * size_blks) -1;
    
    for (i = 0; i < raid4->ops->num_blocks(raid4) * size_blks; i++)
    {
        if (start <= i && i <= end)
        {
            if (buf12[i] != 'a')
            {
                read_write_correct = 0;
                break;
            }
        }
        else
        {
            if (buf12[i] != 'b')
            {
                read_write_correct = 0;
                break;
            }
        }
    }
    free(buf12);
    buf12 = NULL;
    assert(read_write_correct == 1);

    extern int image_devs_open;
    assert(image_devs_open == ndisks+1);
    
    printf("RAID4 Test: SUCCESS\n");
    return 0;
}
