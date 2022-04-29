/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <fs/fs.h>
#if defined(CONFIG_SECOND_CORE_LOAD_BY_LITTLEFS)
#include <fs/littlefs.h>
#include <storage/flash_map.h>
#include <usb/usb_device.h>
#elif defined(CONFIG_SECOND_CORE_LOAD_BY_FATFS)
#include <ff.h>
#include <usb/usb_device.h>
#endif

#include <drivers/ipm.h>
#include <sys/printk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <init.h>

#include <openamp/open_amp.h>
#include <metal/device.h>
#include <remote_core_ctrl.h>
#include <hal/PSRAMManager.h>

#if defined(CONFIG_SECOND_CORE_LOAD_BY_LITTLEFS) || defined(CONFIG_SECOND_CORE_LOAD_BY_FATFS)

#if defined(CONFIG_SECOND_CORE_LOAD_BY_LITTLEFS)
/* Matches LFS_NAME_MAX */
#define MAX_PATH_LEN 255
#define PARTITION_NODE DT_NODELABEL(lfs1)
#if DT_NODE_EXISTS(PARTITION_NODE)
FS_FSTAB_DECLARE_ENTRY(PARTITION_NODE);
struct fs_mount_t *p_g_fs_mount = &FS_FSTAB_ENTRY(PARTITION_NODE);
#else /* PARTITION_NODE */
#error "Please declare the region of the file system first"
#endif /* PARTITION_NODE */
#elif defined(CONFIG_SECOND_CORE_LOAD_BY_FATFS)
#define FS_MOUNT_POINT "/NAND:"
struct fs_mount_t *p_g_fs_mount = NULL;
static struct fs_mount_t s_fs_mount;
static FATFS s_fat_fs;

#endif

static void fssys_show(struct fs_mount_t *mp)
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

static int fs_init(void)
{
	struct fs_mount_t *mp;

#if defined(CONFIG_SECOND_CORE_LOAD_BY_LITTLEFS)
	mp = p_g_fs_mount;
	/* Do not mount if auto-mount has been enabled */
#if !DT_NODE_EXISTS(PARTITION_NODE) ||                                                             \
	!(FSTAB_ENTRY_DT_MOUNT_FLAGS(PARTITION_NODE) & FS_MOUNT_FLAG_AUTOMOUNT)

	int rc = fs_mount(mp);
	if (rc < 0) {
		printk("FAIL: mount id %" PRIuPTR " at %s: %d\n", (uintptr_t)mp->storage_dev,
		       mp->mnt_point, rc);
		return rc;
	}
	printk("%s mount: %d\n", mp->mnt_point, rc);
#else
	printk("%s automounted\n", mp->mnt_point);
#endif

#elif defined(CONFIG_SECOND_CORE_LOAD_BY_FATFS)
	if (p_g_fs_mount != NULL) {
		printk("The mount point<%s> busy!\n", p_g_fs_mount->mnt_point);
		return -1;
	}
	p_g_fs_mount = &s_fs_mount;
	mp = p_g_fs_mount;
	memset(mp, 0, sizeof(struct fs_mount_t));
	mp->type = FS_FATFS;
	mp->flags |= FS_MOUNT_FLAG_NO_FORMAT;
	mp->fs_data = &s_fat_fs;
	mp->mnt_point = FS_MOUNT_POINT;
	int rc = fs_mount(mp);
	if (rc < 0) {
		printk("FAIL: mount id %" PRIuPTR " at %s: %d\n", (uintptr_t)mp->storage_dev,
		       mp->mnt_point, rc);
		return rc;
	}
	printk("%s mount: %d\n", mp->mnt_point, rc);
#endif
	fssys_show(mp);

	return 0;
}
#endif

void remote_runing_tester(int restartNumver)
{
	struct remoteproc *rproc = NULL;
	uint32_t testcnt = 0;
	uint32_t wait_ms = 0;
	int iret;

	rproc = remote_core_create(2);
	if (rproc == NULL) {
		LPRINTF("[%s %d]remote_core_create failed!\n", __FUNCTION__, __LINE__);
		return;
	}
	LPRINTF("Step1: Create rproc success!\n");

	iret = remote_core_loadFw(rproc);
	if (0 != iret) {
		LPRINTF("[%s %d]remote_core_loadFw failed!\n", __FUNCTION__, __LINE__);
		return;
	}
	LPRINTF("Step2: load second core firmware success!\n");

	for (testcnt = 0; testcnt < restartNumver; testcnt++) {
		LPRINTF("----tester loop cnt =%d---------\n", testcnt);
		if (testcnt > 0) {
			iret = remote_core_config(rproc, NULL);
			if (0 != iret) {
				LPRINTF("[%s %d]remote_core_start failed!\n", __FUNCTION__,
					__LINE__);
				return;
			}
			LPRINTF("Step3: reconfig second core success!\n");
		} else {
			LPRINTF("Step3: first jump config second core config success!\n");
		}
		k_msleep(200);
		iret = remote_core_start(rproc);
		if (0 != iret) {
			LPRINTF("[%s %d]remote_core_start failed!\n", __FUNCTION__, __LINE__);
			return;
		}
		LPRINTF("Step4: start second core success!\n");
		for (wait_ms = 10; wait_ms > 0; wait_ms--) {
			LPRINTF("[Master]Ctrl remote core goto stop wait (%d) millisecond...\n",
				wait_ms * 100);
			k_msleep(100);
		}
		iret = remote_core_stop(rproc);
		if (0 != iret) {
			LPRINTF("[%s %d]remote_core_stop failed!\n", __FUNCTION__, __LINE__);
			return;
		}
		LPRINTF("Step5: stop second core success!\n");
		//check cp stop state
		k_msleep(300);
		iret = remote_core_shutdown(rproc);
		if (0 != iret) {
			LPRINTF("[%s %d]remote_core_shutdown failed!\n", __FUNCTION__, __LINE__);
			return;
		}
		LPRINTF("Step6: shutdown second core success!\n");
	}
	iret = remote_core_remove(rproc);
	if (0 != iret) {
		LPRINTF("[%s %d]remote_core_remove failed!\n", __FUNCTION__, __LINE__);
		return;
	}
	LPRINTF("Step7: remove second core success!\n");
	LPRINTF("======The test success==========\n");
}

void main(void)
{
	int status = 0;
	int ret;
	struct metal_init_params metal_params = METAL_INIT_DEFAULTS;

	metal_params.log_level = METAL_LOG_DEBUG;
	status = metal_init(&metal_params);
	if (status != 0) {
		printk("metal_init: failed - error code %d\n", status);
		return;
	}
//file system init
#if defined(CONFIG_SECOND_CORE_LOAD_BY_LITTLEFS) || defined(CONFIG_SECOND_CORE_LOAD_BY_FATFS)
	fs_init();
	ret = usb_enable(NULL);
	if (ret != 0) {
		printk("Failed to enable USB");
		return;
	}
#endif
	
	remote_runing_tester(1);

	while (1) {
		k_msleep(1000);
	}
}

static int psram_init(const struct device *unused)
{
	ARG_UNUSED(unused);
	PSRAM_Initialize();
	return 0;
}

SYS_INIT(psram_init, PRE_KERNEL_2, 0);
