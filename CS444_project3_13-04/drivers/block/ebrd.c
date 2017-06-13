/*
 * Encrypted ram backed block device driver.
 *
 * Michael Elliott, Liv Vitale, Kiarash Teymoury
 *
 * Parts derived from drivers/block/brd.c, and drivers/block/cryptoloop.c,
 * copyright of their respective owners.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/highmem.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <asm/uaccess.h>

#define SECTOR_SHIFT		9
#define PAGE_SECTORS_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS		(1 << PAGE_SECTORS_SHIFT)

static char *key = CONFIG_BLK_DEV_ERAM_KEY;
module_param(key, charp, S_IRUGO);

#define KEY_SIZE 32
static char crypto_key[KEY_SIZE];
static int key_size = 0;


struct crypto_cipher *cipher;

ssize_t key_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_DEBUG "crypt: Copying key\n");
	return scnprintf(buf, PAGE_SIZE, "%s\n", crypto_key);
}

ssize_t key_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if (count != 16 && count != 24 && count != 32)
	{
		printk(KERN_WARNING "Crpyt: invalid key size %d\n", count);
		return -EINVAL;
	}

	printk(KERN_DEBUG "crpyt: storing key\n");
	snprintf(crypto_key, sizeof(crypto_key), "%.*s", (int)min(count, sizeof(crypto_key) - 1), buf);
	key_size = count;
	return count;
}

DEVICE_ATTR(key, 0600, key_show, key_store);

/*
 * Each block ramdisk device has a radix_tree ebrd_pages of pages that stores
 * the pages containing the block device's contents. An ebrd page's ->index is
 * its offset in PAGE_SIZE units. This is similar to, but in no way connected
 * with, the kernel's pagecache or buffer cache (which sit above our block
 * device).
 */
struct ebrd_device {
	int		ebrd_number;

	struct request_queue	*ebrd_queue;
	struct gendisk		*ebrd_disk;
	struct list_head	ebrd_list;

	/*
	 * Backing store of pages and lock to protect it. This is the contents
	 * of the block device.
	 */
	spinlock_t		ebrd_lock;
	struct radix_tree_root	ebrd_pages;
};

/*
 * Look up and return a ebrd's page for a given sector.
 */
static DEFINE_MUTEX(ebrd_mutex);
static struct page *ebrd_lookup_page(struct ebrd_device *ebrd, sector_t sector)
{
	pgoff_t idx;
	struct page *page;

	/*
	 * The page lifetime is protected by the fact that we have opened the
	 * device node -- ebrd pages will never be deleted under us, so we
	 * don't need any further locking or refcounting.
	 *
	 * This is strictly true for the radix-tree nodes as well (ie. we
	 * don't actually need the rcu_read_lock()), however that is not a
	 * documented feature of the radix-tree API so it is better to be
	 * safe here (we don't have total exclusion from radix tree updates
	 * here, only deletes).
	 */
	rcu_read_lock();
	idx = sector >> PAGE_SECTORS_SHIFT; /* sector to page index */
	page = radix_tree_lookup(&ebrd->ebrd_pages, idx);
	rcu_read_unlock();

	BUG_ON(page && page->index != idx);

	return page;
}

/*
 * Look up and return a ebrd's page for a given sector.
 * If one does not exist, allocate an empty page, and insert that. Then
 * return it.
 */
static struct page *ebrd_insert_page(struct ebrd_device *ebrd, sector_t sector)
{
	pgoff_t idx;
	struct page *page;
	gfp_t gfp_flags;

	page = ebrd_lookup_page(ebrd, sector);
	if (page)
		return page;

	/*
	 * Must use NOIO because we don't want to recurse back into the
	 * block or filesystem layers from page reclaim.
	 *
	 * Cannot support XIP and highmem, because our ->direct_access
	 * routine for XIP must return memory that is always addressable.
	 * If XIP was reworked to use pfns and kmap throughout, this
	 * restriction might be able to be lifted.
	 */
	gfp_flags = GFP_NOIO | __GFP_ZERO;
#ifndef CONFIG_BLK_DEV_XIP
	gfp_flags |= __GFP_HIGHMEM;
#endif
	page = alloc_page(gfp_flags);
	if (!page)
		return NULL;

	if (radix_tree_preload(GFP_NOIO)) {
		__free_page(page);
		return NULL;
	}

	spin_lock(&ebrd->ebrd_lock);
	idx = sector >> PAGE_SECTORS_SHIFT;
	page->index = idx;
	if (radix_tree_insert(&ebrd->ebrd_pages, idx, page)) {
		__free_page(page);
		page = radix_tree_lookup(&ebrd->ebrd_pages, idx);
		BUG_ON(!page);
		BUG_ON(page->index != idx);
	}
	spin_unlock(&ebrd->ebrd_lock);

	radix_tree_preload_end();

	return page;
}

static void ebrd_free_page(struct ebrd_device *ebrd, sector_t sector)
{
	struct page *page;
	pgoff_t idx;

	spin_lock(&ebrd->ebrd_lock);
	idx = sector >> PAGE_SECTORS_SHIFT;
	page = radix_tree_delete(&ebrd->ebrd_pages, idx);
	spin_unlock(&ebrd->ebrd_lock);
	if (page)
		__free_page(page);
}

static void ebrd_zero_page(struct ebrd_device *ebrd, sector_t sector)
{
	struct page *page;

	page = ebrd_lookup_page(ebrd, sector);
	if (page)
		clear_highpage(page);
}

/*
 * Free all backing store pages and radix tree. This must only be called when
 * there are no other users of the device.
 */
#define FREE_BATCH 16
static void ebrd_free_pages(struct ebrd_device *ebrd)
{
	unsigned long pos = 0;
	struct page *pages[FREE_BATCH];
	int nr_pages;

	do {
		int i;

		nr_pages = radix_tree_gang_lookup(&ebrd->ebrd_pages,
				(void **)pages, pos, FREE_BATCH);

		for (i = 0; i < nr_pages; i++) {
			void *ret;

			BUG_ON(pages[i]->index < pos);
			pos = pages[i]->index;
			ret = radix_tree_delete(&ebrd->ebrd_pages, pos);
			BUG_ON(!ret || ret != pages[i]);
			__free_page(pages[i]);
		}

		pos++;

		/*
		 * This assumes radix_tree_gang_lookup always returns as
		 * many pages as possible. If the radix-tree code changes,
		 * so will this have to.
		 */
	} while (nr_pages == FREE_BATCH);
}

/*
 * copy_to_ebrd_setup must be called before copy_to_ebrd. It may sleep.
 */
static int copy_to_ebrd_setup(struct ebrd_device *ebrd, sector_t sector, size_t n)
{
	unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	if (!ebrd_insert_page(ebrd, sector))
		return -ENOMEM;
	if (copy < n) {
		sector += copy >> SECTOR_SHIFT;
		if (!ebrd_insert_page(ebrd, sector))
			return -ENOMEM;
	}
	return 0;
}

static void discard_from_ebrd(struct ebrd_device *ebrd,
			sector_t sector, size_t n)
{
	while (n >= PAGE_SIZE) {
		/*
		 * Don't want to actually discard pages here because
		 * re-allocating the pages can result in writeback
		 * deadlocks under heavy load.
		 */
		if (0)
			ebrd_free_page(ebrd, sector);
		else
			ebrd_zero_page(ebrd, sector);
		sector += PAGE_SIZE >> SECTOR_SHIFT;
		n -= PAGE_SIZE;
	}
}

void key_setup(void);
void key_setup()
{
		crypto_cipher_clear_flags(cipher, ~0);
		crypto_cipher_setkey(cipher, key, strlen(key));
		key_size = strlen(key);
}

/*
 * Copy n bytes from src to the ebrd starting at sector. Does not sleep.
 */
static void copy_to_ebrd(struct ebrd_device *ebrd, const void *src,
			sector_t sector, size_t n)
{
	struct page *page;
	void *dst;
	unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
	size_t copy;

	int i;

	key_setup();

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	page = ebrd_lookup_page(ebrd, sector);
	BUG_ON(!page);

	dst = kmap_atomic(page);

	if (key_size != 0) {
		for (i = 0; i < n; i += crypto_cipher_blocksize(cipher)) {
			crypto_cipher_encrypt_one(cipher, dst + i, src + i);
		}
	} else
		memcpy(dst + offset, src, copy);
	
	kunmap_atomic(dst);

	if (copy < n) {
		src += copy;
		sector += copy >> SECTOR_SHIFT;
		copy = n - copy;
		page = ebrd_lookup_page(ebrd, sector);
		BUG_ON(!page);

		dst = kmap_atomic(page);

		if (key_size != 0) {
			for (i = 0; i < n; i += crypto_cipher_blocksize(cipher)) {
				crypto_cipher_encrypt_one(cipher, dst + i, src + i);
			}
		} else
			memcpy(dst, src, copy);
		
		kunmap_atomic(dst);
	}

	printk(KERN_DEBUG "KEY: %s\n", key);
	printk(KERN_DEBUG "\nPLAINTEXT: ");
	for (i = 0; i < n; i++) {
		printk("%u", *(unsigned int *)(src+i));
	}
	printk(KERN_DEBUG "ENCRYPTED: ");
	for (i = 0; i < n; i++) {
		printk("%u", *(unsigned int *)(dst+i));
	}
	printk("\n");
}

/*
 * Copy n bytes to dst from the ebrd starting at sector. Does not sleep.
 */
static void copy_from_ebrd(void *dst, struct ebrd_device *ebrd,
			sector_t sector, size_t n)
{
	struct page *page;
	void *src;
	unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
	size_t copy;

	int i;
	
	key_setup();

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	page = ebrd_lookup_page(ebrd, sector);
	if (page) {
		src = kmap_atomic(page);

		if (key_size != 0) {
			for (i = 0; i < n; i += crypto_cipher_blocksize(cipher)) {
				crypto_cipher_decrypt_one(cipher,dst + i,src + i);
			}
		} else
			memcpy(dst, src + offset, copy);
		
		kunmap_atomic(src);
	} else
		memset(dst, 0, copy);

	if (copy < n) {
		dst += copy;
		sector += copy >> SECTOR_SHIFT;
		copy = n - copy;
		page = ebrd_lookup_page(ebrd, sector);
		if (page) {
			src = kmap_atomic(page);

			if (key_size != 0) {
				for (i = 0; i < n; i += crypto_cipher_blocksize(cipher)) {
					crypto_cipher_decrypt_one(cipher,dst + i,src + i);
				}
			} else
				memcpy(dst, src, copy);
			
			kunmap_atomic(src);
		} else
			memset(dst, 0, copy);
	}

	printk(KERN_DEBUG "KEY: %s\n", key);
	printk(KERN_DEBUG "ENCRYPTED: ");
	for (i = 0; i < n; i++) {
		printk("%u", *(unsigned int *)(dst+i));
	}
	printk(KERN_DEBUG "\nPLAINTEXT: ");
	for (i = 0; i < n; i++) {
		/* printk("%u", *(unsigned int *)(src+i)); */
	}
	printk("\n");
}

/*
 * Process a single bvec of a bio.
 */
static int ebrd_do_bvec(struct ebrd_device *ebrd, struct page *page,
			unsigned int len, unsigned int off, int rw,
			sector_t sector)
{
	void *mem;
	int err = 0;

	if (rw != READ) {
		err = copy_to_ebrd_setup(ebrd, sector, len);
		if (err)
			goto out;
	}

	mem = kmap_atomic(page);
	if (rw == READ) {
		copy_from_ebrd(mem + off, ebrd, sector, len);
		flush_dcache_page(page);
	} else {
		flush_dcache_page(page);
		copy_to_ebrd(ebrd, mem + off, sector, len);
	}
	kunmap_atomic(mem);

out:
	return err;
}

static void ebrd_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	struct ebrd_device *ebrd = bdev->bd_disk->private_data;
	int rw;
	struct bio_vec bvec;
	sector_t sector;
	struct bvec_iter iter;
	int err = -EIO;

	sector = bio->bi_iter.bi_sector;
	if (bio_end_sector(bio) > get_capacity(bdev->bd_disk))
		goto out;

	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
		err = 0;
		discard_from_ebrd(ebrd, sector, bio->bi_iter.bi_size);
		goto out;
	}

	rw = bio_rw(bio);
	if (rw == READA)
		rw = READ;

	bio_for_each_segment(bvec, bio, iter) {
		unsigned int len = bvec.bv_len;
		err = ebrd_do_bvec(ebrd, bvec.bv_page, len,
					bvec.bv_offset, rw, sector);
		if (err)
			break;
		sector += len >> SECTOR_SHIFT;
	}

out:
	bio_endio(bio, err);
}

#ifdef CONFIG_BLK_DEV_XIP
static int ebrd_direct_access(struct block_device *bdev, sector_t sector,
			void **kaddr, unsigned long *pfn)
{
	struct ebrd_device *ebrd = bdev->bd_disk->private_data;
	struct page *page;

	if (!ebrd)
		return -ENODEV;
	if (sector & (PAGE_SECTORS-1))
		return -EINVAL;
	if (sector + PAGE_SECTORS > get_capacity(bdev->bd_disk))
		return -ERANGE;
	page = ebrd_insert_page(ebrd, sector);
	if (!page)
		return -ENOMEM;
	*kaddr = page_address(page);
	*pfn = page_to_pfn(page);

	return 0;
}
#endif

static int ebrd_ioctl(struct block_device *bdev, fmode_t mode,
			unsigned int cmd, unsigned long arg)
{
	int error;
	struct ebrd_device *ebrd = bdev->bd_disk->private_data;

	if (cmd != BLKFLSBUF)
		return -ENOTTY;

	/*
	 * ram device BLKFLSBUF has special semantics, we want to actually
	 * release and destroy the ramdisk data.
	 */
	mutex_lock(&ebrd_mutex);
	mutex_lock(&bdev->bd_mutex);
	error = -EBUSY;
	if (bdev->bd_openers <= 1) {
		/*
		 * Kill the cache first, so it isn't written back to the
		 * device.
		 *
		 * Another thread might instantiate more buffercache here,
		 * but there is not much we can do to close that race.
		 */
		kill_bdev(bdev);
		ebrd_free_pages(ebrd);
		error = 0;
	}
	mutex_unlock(&bdev->bd_mutex);
	mutex_unlock(&ebrd_mutex);

	return error;
}

static const struct block_device_operations ebrd_fops = {
	.owner =		THIS_MODULE,
	.ioctl =		ebrd_ioctl,
#ifdef CONFIG_BLK_DEV_XIP
	.direct_access =	ebrd_direct_access,
#endif
};

/*
 * And now the modules code and kernel interface.
 */
int erd_size = CONFIG_BLK_DEV_RAM_SIZE;
static int max_part;
static int part_shift;
module_param(erd_size, int, S_IRUGO);
MODULE_PARM_DESC(erd_size, "Size of each Encrypted RAM disk in kbytes.");
module_param(max_part, int, S_IRUGO);
MODULE_PARM_DESC(max_part, "Maximum number of partitions per Encrypted RAM disk");
MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(RAMDISK_MAJOR);
MODULE_AUTHOR("Michael Elliott");

#ifndef MODULE
/* Legacy boot options - nonmodular */
static int __init ramdisk_size(char *str)
{
	erd_size = simple_strtol(str, NULL, 0);
	return 1;
}
__setup("ramdisk_size=", ramdisk_size);
#endif

/*
 * The device scheme is derived from loop.c. Keep them in synch where possible
 * (should share code eventually).
 */
static LIST_HEAD(ebrd_devices);
static DEFINE_MUTEX(ebrd_devices_mutex);

static struct ebrd_device *ebrd_alloc(int i)
{
	struct ebrd_device *ebrd;
	struct gendisk *disk;

	ebrd = kzalloc(sizeof(*ebrd), GFP_KERNEL);
	if (!ebrd)
		goto out;
	ebrd->ebrd_number		= i;
	spin_lock_init(&ebrd->ebrd_lock);
	INIT_RADIX_TREE(&ebrd->ebrd_pages, GFP_ATOMIC);

	ebrd->ebrd_queue = blk_alloc_queue(GFP_KERNEL);
	if (!ebrd->ebrd_queue)
		goto out_free_dev;
	blk_queue_make_request(ebrd->ebrd_queue, ebrd_make_request);
	blk_queue_max_hw_sectors(ebrd->ebrd_queue, 1024);
	blk_queue_bounce_limit(ebrd->ebrd_queue, BLK_BOUNCE_ANY);

	ebrd->ebrd_queue->limits.discard_granularity = PAGE_SIZE;
	ebrd->ebrd_queue->limits.max_discard_sectors = UINT_MAX;
	ebrd->ebrd_queue->limits.discard_zeroes_data = 1;
	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, ebrd->ebrd_queue);

	disk = ebrd->ebrd_disk = alloc_disk(1 << part_shift);
	if (!disk)
		goto out_free_queue;
	disk->major		= RAMDISK_MAJOR;
	disk->first_minor	= i << part_shift;
	disk->fops		= &ebrd_fops;
	disk->private_data	= ebrd;
	disk->queue		= ebrd->ebrd_queue;
	disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(disk->disk_name, "eram%d", i);
	set_capacity(disk, erd_size * 2);

	return ebrd;

out_free_queue:
	blk_cleanup_queue(ebrd->ebrd_queue);
out_free_dev:
	kfree(ebrd);
out:
	return NULL;
}

static void ebrd_free(struct ebrd_device *ebrd)
{
	put_disk(ebrd->ebrd_disk);
	blk_cleanup_queue(ebrd->ebrd_queue);
	ebrd_free_pages(ebrd);
	kfree(ebrd);
}

static struct ebrd_device *ebrd_init_one(int i)
{
	struct ebrd_device *ebrd;

	list_for_each_entry(ebrd, &ebrd_devices, ebrd_list) {
		if (ebrd->ebrd_number == i)
			goto out;
	}

	ebrd = ebrd_alloc(i);
	if (ebrd) {
		add_disk(ebrd->ebrd_disk);
		list_add_tail(&ebrd->ebrd_list, &ebrd_devices);
	}
out:
	return ebrd;
}

static void ebrd_del_one(struct ebrd_device *ebrd)
{
	list_del(&ebrd->ebrd_list);
	del_gendisk(ebrd->ebrd_disk);
	ebrd_free(ebrd);
}

static struct kobject *ebrd_probe(dev_t dev, int *part, void *data)
{
	struct ebrd_device *ebrd;
	struct kobject *kobj;

	mutex_lock(&ebrd_devices_mutex);
	ebrd = ebrd_init_one(MINOR(dev) >> part_shift);
	kobj = ebrd ? get_disk(ebrd->ebrd_disk) : NULL;
	mutex_unlock(&ebrd_devices_mutex);

	*part = 0;
	return kobj;
}

static int __init ebrd_init(void)
{
	unsigned long range;
	struct ebrd_device *ebrd, *next;

	cipher = crypto_alloc_cipher("aes", 0, 0);
	key_size = strlen(crypto_key);

	part_shift = 0;
	if (max_part > 0) {
		part_shift = fls(max_part);

		/*
		 * Adjust max_part according to part_shift as it is exported
		 * to user space so that user can decide correct minor number
		 * if [s]he want to create more devices.
		 *
		 * Note that -1 is required because partition 0 is reserved
		 * for the whole disk.
		 */
		max_part = (1UL << part_shift) - 1;
	}

	if ((1UL << part_shift) > DISK_MAX_PARTS)
		return -EINVAL;

	range = 1UL << MINORBITS;

	if (register_blkdev(RAMDISK_MAJOR, "ramdisk"))
		return -EIO;

	ebrd = ebrd_alloc(0);
	if (!ebrd)
		goto out_free;
	list_add_tail(&ebrd->ebrd_list, &ebrd_devices);

	/* point of no return */

	list_for_each_entry(ebrd, &ebrd_devices, ebrd_list)
		add_disk(ebrd->ebrd_disk);

	blk_register_region(MKDEV(RAMDISK_MAJOR, 0), range,
				  THIS_MODULE, ebrd_probe, NULL, NULL);

	printk(KERN_INFO "ebrd: module loaded\n");
	return 0;

out_free:
	list_for_each_entry_safe(ebrd, next, &ebrd_devices, ebrd_list) {
		list_del(&ebrd->ebrd_list);
		ebrd_free(ebrd);
	}
	unregister_blkdev(RAMDISK_MAJOR, "ramdisk");

	return -ENOMEM;
}

static void __exit ebrd_exit(void)
{
	unsigned long range;
	struct ebrd_device *ebrd, *next;

	crypto_free_cipher(cipher);
	
	range = 1UL << MINORBITS;

	list_for_each_entry_safe(ebrd, next, &ebrd_devices, ebrd_list)
		ebrd_del_one(ebrd);

	blk_unregister_region(MKDEV(RAMDISK_MAJOR, 0), range);
	unregister_blkdev(RAMDISK_MAJOR, "ramdisk");
}

module_init(ebrd_init);
module_exit(ebrd_exit);
