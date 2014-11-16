/*
 * file:        homework.c
 * description: skeleton code for CS 5600 Homework 3
 *
 * Peter Desnoyers, Northeastern Computer Science, 2011
 * $Id: homework.c 410 2011-11-07 18:42:45Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "blkdev.h"

/********** MIRRORING ***************/

/* example state for mirror device. See mirror_create for how to
 * initialize a struct blkdev with this.
 */
struct mirror_dev {
    struct blkdev *disks[2];    /* flag bad disk by setting to NULL */
    int nblks;
};
    
static int mirror_num_blocks(struct blkdev *dev)
{
    /* your code here */
    struct mirror_dev *mdev = dev->private;
    assert(mdev != NULL && mdev->disks != NULL);
    return mdev->nblks;
}

/* read from one of the sides of the mirror. (if one side has failed,
 * it had better be the other one...) If both sides have failed,
 * return an error.
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should close the
 * device and flag it (e.g. as a null pointer) so you won't try to use
 * it again. 
 */
static int mirror_read(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{
    /* your code here */
    int result = E_UNAVAIL;
    if(dev == NULL || dev->private == NULL)
        return result;

    struct mirror_dev *mdev = dev->private;
    if(mdev->disks == NULL)
        return result;

    struct blkdev *disk1 = mdev->disks[0];
    struct blkdev *disk2 = mdev->disks[1];
    if(disk1 == NULL && disk2 == NULL)
        return result;
    // if disk1 is available, read from disk1 and return read result
    if(disk1 != NULL)
    {
        result = disk1->ops->read(disk1, first_blk, num_blks, buf);
        if(result == SUCCESS)
            return result;
        else if(result == E_UNAVAIL)
        {
            disk1->ops->close(disk1);
            mdev->disks[0] = NULL;
        }
    }
    // else if disk2 is available, read from disk2 and return read result
    if(disk2 != NULL)
    {
        result = disk2->ops->read(disk2, first_blk, num_blks, buf);
        if(result == SUCCESS)
            return result;
        else if(result == E_UNAVAIL)
        {
            disk2->ops->close(disk2);
            mdev->disks[1] = NULL;
        }
    }
    return result;
}

/* write to both sides of the mirror, or the remaining side if one has
 * failed. If both sides have failed, return an error.
 * Note that a write operation may indicate that the underlying device
 * has failed, in which case you should close the device and flag it
 * (e.g. as a null pointer) so you won't try to use it again.
 */
static int mirror_write(struct blkdev * dev, int first_blk,
                        int num_blks, void *buf)
{
    /* your code here */
    int result = E_UNAVAIL;
    if(dev == NULL || dev->private == NULL)
        return result;

    struct mirror_dev *mdev = dev->private;
    if(mdev->disks == NULL)
        return result;

    struct blkdev *disk1 = mdev->disks[0];
    struct blkdev *disk2 = mdev->disks[1];
    if(disk1 == NULL && disk2 == NULL)
        return result;
    // if disk1 is available, write to disk1
    if(disk1 != NULL)
    {
        result = disk1->ops->write(disk1, first_blk, num_blks, buf);
        if(result == E_UNAVAIL)
        {
            disk1->ops->close(disk1);
            mdev->disks[0] = NULL;
        } else if(result == E_BADADDR)
            return result;
    }
    // if disk2 is available, write to disk2
    if(disk2 != NULL)
    {
        int tempRs = disk2->ops->write(disk2, first_blk, num_blks, buf);
        if(tempRs == E_UNAVAIL)
        {
            disk2->ops->close(disk2);
            mdev->disks[1] = NULL;
        }
        // if disk1 fails, use disk2's write result
        if(result != SUCCESS)
            result = tempRs;
    }
    return result;
}

/* clean up, including: close any open (i.e. non-failed) devices, and
 * free any data structures you allocated in mirror_create.
 */
static void mirror_close(struct blkdev *dev)
{
    /* your code here */
    struct mirror_dev *mdev = dev->private;
    struct blkdev *disk1 = mdev->disks[0];
    struct blkdev *disk2 = mdev->disks[1];
    if(disk1 != NULL)
    {
        disk1->ops->close(disk1);
        free(mdev->disks[0]);
    }
    if(disk2 != NULL)
    {
        disk2->ops->close(disk2);
        free(mdev->disks[1]);
    }
    free(mdev);
    free(dev);
}

struct blkdev_ops mirror_ops = {
    .num_blocks = mirror_num_blocks,
    .read = mirror_read,
    .write = mirror_write,
    .close = mirror_close
};

/* create a mirrored volume from two disks. Do not write to the disks
 * in this function - you should assume that they contain identical
 * contents. 
 */
struct blkdev *mirror_create(struct blkdev *disks[2])
{
    struct blkdev *dev = malloc(sizeof(*dev));
    struct mirror_dev *mdev = malloc(sizeof(*mdev));

    /* your code here */
    if(disks[0]->ops->num_blocks(disks[0]) != disks[1]->ops->num_blocks(disks[1]))
    {
        printf("ERROR: diffrent sizes of volumes!\n");
        return NULL;
    }
    mdev->disks[0] = disks[0];
    mdev->disks[1] = disks[1];
    mdev->nblks = disks[0]->ops->num_blocks(disks[0]);

    dev->private = mdev;
    dev->ops = &mirror_ops;

    return dev;
}

/* replace failed device 'i' (0 or 1) in a mirror. Note that we assume
 * the upper layer knows which device failed. You will need to
 * replicate content from the other underlying device before returning
 * from this call.
 */
int mirror_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    struct mirror_dev *mdev = volume->private;
    struct blkdev *normal_disk = mdev->disks[1 - i];
    // if the new disk is not the same size as the old one
    if(normal_disk->ops->num_blocks(normal_disk) != newdisk->ops->num_blocks(newdisk))
        return E_SIZE;
    int len = normal_disk->ops->num_blocks(normal_disk);
    char buf[BLOCK_SIZE * len];
    int result;
    // read from the normal disk first, and get the result
    result = normal_disk->ops->read(normal_disk, 0, len, buf);
    if(result == SUCCESS)
        // write to the new disk, and get the result
        result = newdisk->ops->write(newdisk, 0, len, buf);
    // assign the new disk to the failed the location
    mdev->disks[i] = newdisk;
    return result;
}



/**********  STRIPING ***************/
struct stripe_dev {
	struct blkdev **disks;		// NULL if it is broken
	int unit;					// Stripe unit
	int disk_num;				// disk number
	int disk_size;				// disk size
};

int stripe_num_blocks(struct blkdev *dev)
{
	struct stripe_dev *sd = dev->private;
	assert(sd != NULL && sd->disks != NULL);
	assert(sd->unit != 0);
	return sd->disk_num * sd->disk_size;
}

/* read blocks from a striped volume. 
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should (a) close the
 * device and (b) return an error on this and all subsequent read or
 * write operations. 
 */
static int stripe_read(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{
	struct		stripe_dev *sd = dev->private;
	int			i = 0;
	int			unit = sd->unit;;
	struct		blkdev *current_disk = NULL;
	int			blk_disk = 0;
	int			disk_n = sd->disk_num;
	int			num = 0;
	int			result = 0;
	void*		current_buff_location = buf;
	int			current_disk_num = 0;
	
	//check before start
	if (sd == NULL || sd->disks == NULL)
		return E_UNAVAIL;
	if ( first_blk < 0 || 
		(num_blks + first_blk > stripe_num_blocks(dev)))
		return E_BADADDR;

	for (i = 0; i < num_blks; i++){

		current_disk_num = first_blk + i;
		blk_disk = (current_disk_num / unit) % disk_n;
		int current_layer = (current_disk_num / (unit * disk_n));
		num = unit * current_layer + current_disk_num % unit;
		current_disk = sd->disks[blk_disk];

		result = current_disk->ops->read(current_disk, num, 1, current_buff_location);
		if (result != SUCCESS)
		{
			if (result == E_UNAVAIL)//if got any error, return
			{
				dev->ops->close(dev);
				return result;
			}
			else if (result == E_BADADDR){
				return result;
			}
		}
		else
			current_buff_location = current_buff_location + BLOCK_SIZE;
	}

	return SUCCESS;
}

/* write blocks to a striped volume.
 * Again if an underlying device fails you should close it and return
 * an error for this and all subsequent read or write operations.
 */
static int stripe_write(struct blkdev * dev, int first_blk,
                        int num_blks, void *buf)
{
	struct stripe_dev *sd = dev->private;
	int		i = 0;
	int		unit = sd->unit;
	struct	blkdev *current_disk = NULL;
	int		blk_disk = 0;
	int		disk_n = sd->disk_num;
	int		num = 0;
	int		result = 0;
	void*	current_buff_location = buf;
	int		current_disk_num = 0;

	//struct stripe_dev *sd = dev->private;
	
	if (sd == NULL || sd->disks == NULL)
		return E_UNAVAIL;

	if ( first_blk < 0 || 
		(num_blks + first_blk > stripe_num_blocks(dev)))
		return E_BADADDR;

	for (i = 0; i< num_blks; i++){
		current_disk_num = first_blk + i;
		blk_disk = (current_disk_num / unit) % disk_n;

		int current_layer = (current_disk_num / (unit * disk_n));

		num = unit * current_layer + current_disk_num % unit;
		current_disk = sd->disks[blk_disk];

		// current disk failed, return
		if (current_disk == NULL)
		{
			dev->ops->close(dev);
			return E_UNAVAIL;
		}

		result = current_disk->ops->write(current_disk, num, 1, current_buff_location);

		if (result != SUCCESS)
		{
			if (result == E_UNAVAIL)//if got errors, return
			{
				current_disk->ops->close(current_disk);
				dev->ops->close(dev);
				return result;
			}
			else if (result == E_BADADDR){
				current_disk->ops->close(current_disk);
				return result;
			}
		}
		else
			current_buff_location = current_buff_location + BLOCK_SIZE;
	}

	return SUCCESS;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in stripe_create. 
 */
static void stripe_close(struct blkdev *dev)
{
	struct stripe_dev *sd = dev->private;
	// close
	int i;
	for (i = 0; i < sd->disk_num; i++)
	{
		sd->disks[i]->ops->close(sd->disks[i]);
	}
	free(sd);
	dev->private = NULL;
	free(dev);
}

struct blkdev_ops stripe_ops = {
	.num_blocks = stripe_num_blocks,
	.read = stripe_read,
	.write = stripe_write,
	.close = stripe_close
};

/* create a striped volume across N disks, with a stripe size of
 * 'unit'. (i.e. if 'unit' is 4, then blocks 0..3 will be on disks[0],
 * 4..7 on disks[1], etc.)
 * Check the size of the disks to compute the final volume size, and
 * fail (return NULL) if they aren't all the same.
 * Do not write to the disks in this function.
 */
struct blkdev *striped_create(int N, struct blkdev *disks[], int unit)
{
	// disks must be the same size
	int			i = 0;
	int			disk_size = -1;
	int			current_disk_size = 0;
	struct		blkdev *disk = NULL;
	struct		stripe_dev *s_dev = malloc(sizeof(*s_dev));
	s_dev->disks = disks;   //Question

	// Chek the size of the disks
	for (i = 0; i < N; i++)
	{
		if (disks[i] == NULL)
			return NULL;
		disk = NULL;
		disk = disks[i];
		current_disk_size = disk->ops->num_blocks(disk);
		if (disk_size < 0){
			disk_size = current_disk_size;
		}
		else if (disk_size != current_disk_size){
			// Not the same size
			fprintf(stderr, "Disks dont have same size.\n");
			free(s_dev);
			free(disk);
			return NULL;
		}
	}

	struct blkdev *dev = malloc(sizeof(*dev));

	s_dev->disk_num = N;
	s_dev->disk_size = disk_size - disk_size % unit;
	s_dev->unit = unit;
	dev->private = s_dev;
	dev->ops = &stripe_ops;

	return dev;
}


/**********   RAID 4  ***************/
struct raid4_dev {
    struct blkdev **disks;      // NULL if it is broken
    int unit;                   // unit
    int disk_num;               // disk number excluding parity disk
    int disk_size;              // disk size
    bool degraded;
    int broken_disk;
};

int raid4_num_blocks(struct blkdev *dev)
{
    struct raid4_dev *r4dev = dev->private;
    assert(r4dev != NULL && r4dev->disks != NULL);
    assert(r4dev->unit != 0);
    return r4dev->disk_num*r4dev->disk_size;
}
/* helper function - compute parity function across two blocks of
 * 'len' bytes and put it in a third block. Note that 'dst' can be the
 * same as either 'src1' or 'src2', so to compute parity across N
 * blocks you can do: 
 *
 *     void **block[i] - array of pointers to blocks
 *     dst = <zeros[len]>
 *     for (i = 0; i < N; i++)
 *        parity(block[i], dst, dst);
 *
 * Yes, it's slow.
 */
void parity(int len, void *src1, void *src2, void *dst)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i;
    for (i = 0; i < len; i++)
        d[i] = s1[i] ^ s2[i];
}
/*
char* reconstruct(struct raid4_dev *rdev, int broken_num, int inner_blk)
{
    int num_disks = rdev->disk_num; 
    int i = 0;
    char data[BLOCK_SIZE];
    char result[BLOCK_SIZE];
    char *result_ptr = result;
    int count = 0;
    for (i = 0; i < num_disks; i ++)
    {
        if (i != broken_num)
        {
            if (count == 0)
            {
                rdev->disks[i]->ops->read(rdev->disks[i], inner_blk, 1, result);
                count = 1;
            }
            else
            {
                rdev->disks[i]->ops->read(rdev->disks[i], inner_blk, 1, data);
                parity(BLOCK_SIZE, result, data, result); 
                //printf("temp:%s\n", data);
            }
        }
    }
    return result_ptr;
} 
*/

void reconstruct(struct raid4_dev *r4dev, int broken_num, int inner_lba, void *buf)
{
    char temp[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    bool first_blk = true;
    int i;
    for(i = 0; i <= r4dev->disk_num; i ++)
    {
        if(i != broken_num)
        {
            r4dev->disks[i]->ops->read(r4dev->disks[i], inner_lba, 1, temp);
            //printf("temp:%s %d\n", temp, i);
            if(first_blk)
            {
                
                memcpy(data_buf, temp, BLOCK_SIZE);
                //printf("data_buf:%s %d\n", data_buf, i);
                first_blk = false;
            }
            else{
                //printf("temp:%s %d\n", temp, i);
                parity(BLOCK_SIZE, data_buf, temp, data_buf);
            }
            
        }
    }
    //printf("data_buf:%s\n", data_buf);
    memcpy(buf, data_buf, BLOCK_SIZE);
    //printf("buf:%s\n", (char*) buf);
}

/* read blocks from a RAID 4 volume.
 * If the volume is in a degraded state you may need to reconstruct
 * data from the other stripes of the stripe set plus parity.
 * If a drive fails during a read and all other drives are
 * operational, close that drive and continue in degraded state.
 * If a drive fails and the volume is already in a degraded state,
 * close the drive and return an error.
 */
static int raid4_read(struct blkdev * dev, int first_blk,
                      int num_blks, void *buf) 
{
    int result = E_UNAVAIL;
    struct raid4_dev *r4dev = dev->private;
    if(r4dev == NULL || r4dev->disks == NULL)
        return result;
    //struct blkdev **disks = r4dev->disks;
    int unit = r4dev->unit;
    int n = r4dev->disk_num;
    //int broken_i = r4dev->broken_disk;
    int i;
    int disk_num;
    int inner_lba;
    void *addr_buf;
    for (i = first_blk; i < first_blk + num_blks; i++)
    {

        addr_buf = buf + (i - first_blk) * BLOCK_SIZE;
        disk_num = (i / unit) % n;
        inner_lba = (i / unit) / n * unit + i % unit;
        if(r4dev->disks[disk_num] != NULL)
            result = r4dev->disks[disk_num]->ops->read(r4dev->disks[disk_num], inner_lba, 1, addr_buf);
        else result = E_UNAVAIL;
        if(result == E_BADADDR)
            return result;
        if(result == E_UNAVAIL)
        {
            if(!r4dev->degraded)
            {
                r4dev->degraded = true;
                r4dev->broken_disk = disk_num;
                // reconstruct the data on the failed device
                //memcpy(addr_buf, reconstruct(r4dev, disk_num, inner_lba), 
                //  BLOCK_SIZE);
                
                reconstruct(r4dev, disk_num, inner_lba, addr_buf);
                //printf("1:%.*s\n", BLOCK_SIZE, (char*)addr_buf);
                // close the failed devide
                r4dev->disks[disk_num]->ops->close(r4dev->disks[disk_num]);
                r4dev->disks[disk_num] = NULL;
                
                result = SUCCESS;
            }
            else if(disk_num == r4dev->broken_disk)
            {
                //memcpy(addr_buf, reconstruct(r4dev, disk_num, inner_lba), 
                //  BLOCK_SIZE);

                
                reconstruct(r4dev, disk_num, inner_lba, addr_buf);
                //printf("2:%.*s\n", BLOCK_SIZE, (char*)addr_buf);
                result = SUCCESS;
            }
            else
            {
                r4dev->disks[disk_num]->ops->close(r4dev->disks[disk_num]);
                r4dev->disks[disk_num] = NULL;
                return result;
            }
               
        }
    }

    return result;
}

/* write blocks to a RAID 4 volume.
 * Note that you must handle short writes - i.e. less than a full
 * stripe set. You may either use the optimized algorithm (for N>3
 * read old data, parity, write new data, new parity) or you can read
 * the entire stripe set, modify it, and re-write it. Your code will
 * be graded on correctness, not speed.
 * If an underlying device fails you should close it and complete the
 * write in the degraded state. If a drive fails in the degraded
 * state, close it and return an error.
 * In the degraded state perform all writes to non-failed drives, and
 * forget about the failed one. (parity will handle it)
 */
static int raid4_write(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{
    int result = E_UNAVAIL;
    struct raid4_dev *r4dev = dev->private;
    if(r4dev == NULL || r4dev->disks == NULL)
        return result;
    int unit = r4dev->unit;
    //int broken_i = r4dev->broken_disk;
    int n = r4dev->disk_num;
    int disk_n;
    int inner_lba;
    void *addr_buf;
    char data_buf[BLOCK_SIZE];
    char parity_buf[BLOCK_SIZE];
    int i;
    for(i = first_blk; i < first_blk + num_blks; i ++)
    {
        addr_buf = buf + (i - first_blk) * BLOCK_SIZE;
        disk_n = i / unit % n;
        inner_lba = (i / unit) / n * unit + i % unit;
        // if the disk not failed, read old data first
        if(r4dev->disks[disk_n] != NULL)
            result = r4dev->disks[disk_n]->ops->read(r4dev->disks[disk_n], inner_lba, 1, data_buf);
        else result = E_UNAVAIL;
        if(result == E_BADADDR)
            return result;
        if(result == E_UNAVAIL)
        {
            if(!r4dev->degraded)
            {
                r4dev->degraded = true;
                r4dev->broken_disk = disk_n;
                // close the failed device
                r4dev->disks[disk_n]->ops->close(r4dev->disks[disk_n]);
                r4dev->disks[disk_n] = NULL;
                // reconstruct the data on the failed device
                reconstruct(r4dev, disk_n, inner_lba, data_buf);
                result = SUCCESS;
            }
            else if(r4dev->broken_disk == disk_n)
            {
                reconstruct(r4dev, disk_n, inner_lba, data_buf);
                result = SUCCESS;
            }
            else
            {
                r4dev->disks[disk_n]->ops->close(r4dev->disks[disk_n]);
                r4dev->disks[disk_n] = NULL;
                return E_UNAVAIL;
            }
        }

        // write new data to current disk
        if(r4dev->disks[disk_n] != NULL)
            r4dev->disks[disk_n]->ops->write(r4dev->disks[disk_n], inner_lba, 1, addr_buf);
        // read old parity data
        result = r4dev->disks[n]->ops->read(r4dev->disks[n], inner_lba, 1, parity_buf);
        if(result != SUCCESS)
            return result;
        // calculate new parity

        parity(BLOCK_SIZE, parity_buf, data_buf, parity_buf);
        parity(BLOCK_SIZE, parity_buf, addr_buf, parity_buf);
        // write new parity to parity disk
        //printf("parity:%s\n", parity_buf);
        result = r4dev->disks[n]->ops->write(r4dev->disks[n], inner_lba, 1, parity_buf);
        if(result != SUCCESS)
            return result;
        
    }
    return result;
    /*
    struct raid4_dev * rdev = dev->private;
    int N = rdev->disk_num;
    int unit = rdev->unit;
    int broken_num = rdev->broken_disk;
    int disk_num, inner_blk;
    void *buf_addr;
    int result = E_UNAVAIL;
    int i;
    char read_data[BLOCK_SIZE];
    char parity_data[BLOCK_SIZE];
    
    for (i = first_blk; i < first_blk + num_blks; i++)
    {
        //calculate corresponding disk_num and LBA#, ignoring parity disk
        disk_num = (i / unit) % N;
        inner_blk = (i / unit) / N * unit +i % unit;
        buf_addr = buf + (i - first_blk) * BLOCK_SIZE;
        
        //first, read the old data into read_data buf
        result = rdev->disks[disk_num]->ops->read(rdev->disks[disk_num], 
                 inner_blk, 1, read_data);
        
        //if read fail and in disgrade mode, retrun ERROR immediately;
        //in stardard mode, record the failed disk and recover the data
        if(result != SUCCESS)
        {
            if(broken_num == -1)
                broken_num = disk_num;
            if(broken_num != -1 && broken_num != disk_num)
                return E_UNAVAIL;
            reconstruct(rdev, broken_num, inner_blk, read_data);
            
            //memcpy(read_data, reconstruct(rdev, broken_num, inner_blk), 
            //      BLOCK_SIZE);
            printf("%s\n", read_data);
            result = SUCCESS;
        }
        
        //then, write the new data in and calculate the new parity
        rdev->disks[disk_num]->ops->write(rdev->disks[disk_num], 
                 inner_blk, 1, buf_addr);
        rdev->disks[N]->ops->read(rdev->disks[N],inner_blk,1,parity_data);
        parity(BLOCK_SIZE, parity_data, read_data, parity_data);
        parity(BLOCK_SIZE, parity_data, buf_addr, parity_data);
        printf("parity:%s\n", parity_data);
        rdev->disks[N]->ops->write(rdev->disks[N],inner_blk,1,parity_data); 
    }
    return result;
    */
}

/* clean up, including: close all devices and free any data structures
 * you allocated in raid4_create. 
 */
static void raid4_close(struct blkdev *dev)
{
    struct raid4_dev *r4dev = dev->private;
    int i;
    for(i = 0; i <= r4dev->disk_num; i ++)
    {
        r4dev->disks[i]->ops->close(r4dev->disks[i]);
    }
    free(r4dev);
    free(dev);
}

struct blkdev_ops raid4_ops = {
    .num_blocks = raid4_num_blocks,
    .read = raid4_read,
    .write = raid4_write,
    .close = raid4_close
};

/* Initialize a RAID 4 volume with stripe size 'unit', using
 * disks[N-1] as the parity drive. Do not write to the disks - assume
 * that they are properly initialized with correct parity. (warning -
 * some of the grading scripts may fail if you modify data on the
 * drives in this function)
 */
struct blkdev *raid4_create(int N, struct blkdev *disks[], int unit)
{
    struct blkdev *dev = malloc(sizeof(*dev));
    struct raid4_dev *r4dev = malloc(sizeof(*r4dev));
    if(dev == NULL || r4dev == NULL)
        return NULL;

    r4dev->disks = malloc(N * sizeof(*dev));
    //r4dev->disks = disks;
    assert(disks[0] != NULL);
    int i, j;
    int d_size = disks[0]->ops->num_blocks(disks[0]);
    char init_data[BLOCK_SIZE]; 
    for(i = 0; i < BLOCK_SIZE; i ++)
        init_data[i] = 0;
    // check the sizes of disks first
    for(i = 0; i < N; i ++)
    {
        assert(disks[i] != NULL);
        if(disks[i]->ops->num_blocks(disks[i]) != d_size)
        {
            printf("ERROR: Disks don't have same sizes!\n");
            return NULL;
        }
        for(j = 0; j < d_size / unit * unit; j ++)
            disks[i]->ops->write(disks[i], j, 1, init_data);
        r4dev->disks[i] = disks[i];
    }

    r4dev->unit = unit;
    r4dev->disk_num = N - 1; //disks[N-1] is the parity drive
    r4dev->disk_size = d_size / unit * unit;//d_size - d_size % unit;
    r4dev->degraded = false;
    r4dev->broken_disk = -1;

    dev->private = r4dev;
    dev->ops = &raid4_ops;

    return dev;
    
    /*
    //check that all disks have same size
    int disk_size = disks[0]->ops->num_blocks(disks[0]);
    int i;
    for (i = 0; i < N; i++)
    {
        if (disk_size != disks[i]->ops->num_blocks(disks[i]))
        {
            printf("Fail. Not same size.");
            return NULL;
        }
    }
    
    struct blkdev *dev = malloc(sizeof(*dev));
    struct raid4_dev *rdev = malloc(sizeof(*rdev)); 
    rdev->disks = malloc(N * sizeof(*dev));
    
    int start;
    char data[BLOCK_SIZE];
    for(i = 0; i < BLOCK_SIZE; i++)
    {
        data[i] = 0;
    }
    for(start = 0; start < (disk_size / unit) * unit; start++)
    {
        for(i = 0; i < N; i++)
        {
            disks[i]->ops->write(disks[i], start, 1, data);
        }
    }
        
    for (i = 0; i < N; i++)
    { 
        rdev->disks[i] = disks[i];
    }
    
    //rdev->nblks = (N ) * (disk_size / unit) * unit;
    rdev->disk_num = N - 1;
    rdev->disk_size = (disk_size / unit) * unit;
    rdev->unit = unit;
    rdev->broken_disk = -1;
    rdev->degraded = false;

    dev->private = rdev;
    dev->ops = &raid4_ops;  
    return dev;
    */
}

/* replace failed device 'i' in a RAID 4. Note that we assume
 * the upper layer knows which device failed. You will need to
 * reconstruct content from data and parity before returning
 * from this call.
 */
int raid4_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    int result = E_UNAVAIL;
    struct raid4_dev *r4dev = volume->private;
    struct blkdev *normal_disk = r4dev->disks[(i+1)%r4dev->disk_num];
    if(normal_disk->ops->num_blocks(normal_disk) != newdisk->ops->num_blocks(newdisk))
        return E_SIZE;
    char data_buf[BLOCK_SIZE];
    int j;
    // reconstruct the data on the failed device block by block, and copy it to the newdisk
    for(j = 0; j < r4dev->disk_size; j ++)
    {
        reconstruct(r4dev, i, j, data_buf);
        //memcpy(data_buf, reconstruct(r4dev, i, j), BLOCK_SIZE);
        result = newdisk->ops->write(newdisk, j, 1, data_buf);
    }
    r4dev->disks[i] = newdisk;
    r4dev->degraded = false;
    return result;
}

