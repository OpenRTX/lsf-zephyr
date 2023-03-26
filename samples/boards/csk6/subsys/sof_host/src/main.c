/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>

#include <logging/log.h>

#include <fs/fs.h>


#include <ff.h>


#if(CONFIG_USB_DEVICE_STACK > 0)
#include <usb/usb_device.h>
#endif


#define FS_MOUNT_POINT "/NAND:"
struct fs_mount_t *p_g_fs_mount = NULL;
static struct fs_mount_t fs_mount_p;
static FATFS s_fat_fs;



static void s_fs_show(struct fs_mount_t *mp)
{
	struct fs_statvfs sbuf;
	int rc;

	rc = fs_statvfs(mp->mnt_point, &sbuf);
	if (rc < 0) {
		printk("FAIL: statvfs: %d\n", rc);
		goto out;
	}

	printk("%s: bsize = %lu ; frsize = %lu ;"
	       " blocks = %lu ; bfree = %lu\n",
	       mp->mnt_point, sbuf.f_bsize, sbuf.f_frsize, sbuf.f_blocks, sbuf.f_bfree);

	struct fs_dir_t dir;

	fs_dir_t_init(&dir);

	rc = fs_opendir(&dir, mp->mnt_point);
	printk("%s opendir: %d\n", mp->mnt_point, rc);

	while (rc >= 0) {
		struct fs_dirent ent = { 0 };

		rc = fs_readdir(&dir, &ent);
		if (rc < 0) {
			break;
		}
		if (ent.name[0] == 0) {
			printk("End of files\n");
			break;
		}
		printk("  %c %zu %s\n", (ent.type == FS_DIR_ENTRY_FILE) ? 'F' : 'D', ent.size,
		       ent.name);
	}

	(void)fs_closedir(&dir);
	printk("%s closedir\n", mp->mnt_point);
out:
	return;
}

static int s_fs_mount(void)
{
	struct fs_mount_t *mp;
	int rc;

	if (p_g_fs_mount != NULL) {
		printk("The mount point<%s> busy!\n", p_g_fs_mount->mnt_point);
		return -1;
	}
	p_g_fs_mount = &fs_mount_p;
	mp = p_g_fs_mount;
	memset(mp, 0, sizeof(struct fs_mount_t));
	mp->type = FS_FATFS;
	/*mp->flags |= FS_MOUNT_FLAG_NO_FORMAT;*/
	mp->fs_data = &s_fat_fs;
	mp->mnt_point = FS_MOUNT_POINT;
	rc = fs_mount(mp);
	if (rc < 0) {
		printk("FAIL: mount id %" PRIuPTR " at %s: %d\n", (uintptr_t)mp->storage_dev,
		       mp->mnt_point, rc);
		return rc;
	}
	printk("%s mount: %d\n", mp->mnt_point, rc);

	s_fs_show(mp);

	return 0;
}



void main(void)
{

	s_fs_mount();


	while (1) {
		k_msleep(1000);
	}
}
