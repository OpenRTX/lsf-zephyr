/*
 * Load firmware example
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if defined(CONFIG_SECOND_CORE_LOAD_BY_LITTLEFS) || defined(CONFIG_SECOND_CORE_LOAD_BY_FATFS)
#include <device.h>
#include <fs/fs.h>
#include <cache.h>
#elif defined(CONFIG_SECOND_CORE_LOAD_BY_FLASH_BINARY)
#include <cache.h>
#endif

#include <in_common.h>

static struct remoteproc rproc_inst;
static struct remoteproc_ops ops;

extern struct remoteproc_ops csk6_rproc_ops;
extern struct image_store_ops mem_image_store_ops;

struct remoteproc *remote_core_create(unsigned int cpu_id)
{
	struct remoteproc *rproc;
	ops = csk6_rproc_ops;
	rproc = remoteproc_init(&rproc_inst, &ops, &cpu_id);
	if (!rproc)
		return NULL;

	return &rproc_inst;
}

#if defined(CONFIG_SECOND_CORE_LOAD_BY_LITTLEFS) || defined(CONFIG_SECOND_CORE_LOAD_BY_FATFS)
extern struct fs_mount_t *p_g_fs_mount;

int remote_core_loadFw(struct remoteproc *rproc)
{
	char fname[64];
	int32_t rc;
	struct fs_file_t file;
	uint32_t fw_size;
	char *boot_address = (char *)PSRAM_FW_WR_ADDRESS;

	if (rproc == NULL)
		return -EINVAL;

	if ((p_g_fs_mount == NULL) || (p_g_fs_mount->mnt_point == NULL)) {
		printk("The mount is invailed!\n");
		return -EINVAL;
	}

	/* Configure remoteproc to get ready to load executable */
	remoteproc_config(rproc, NULL);

	//load fw
	snprintf(fname, sizeof(fname), "%s/%s", p_g_fs_mount->mnt_point, REMOTE_CORE_FW_NAME);
	struct fs_dirent dirent;
	rc = fs_stat(fname, &dirent);
	printk("%s stat: %d\n", fname, rc);
	if (rc < 0) {
		printk("FAIL: fs_stat %s: %d\n", fname, rc);
		return rc;
	}

	fw_size = dirent.size;
	printk("Get file '%s' size %zu\n", dirent.name, dirent.size);
	if (fw_size == 0) {
		printk("FAIL: The fw size is 0\n");
		return -1;
	}

	fs_file_t_init(&file);
	rc = fs_open(&file, fname, FS_O_READ);
	if (rc < 0) {
		printk("FAIL: open %s: %d\n", fname, rc);
		return rc;
	}

	rc = fs_read(&file, boot_address, fw_size);
	if (rc != fw_size) {
		printk("Failed:read size %d return %d write to %p\n", fw_size, rc, boot_address);
		return -EIO;
	}
	sys_cache_data_range(boot_address, fw_size, K_CACHE_WB);
	printk("[Upgrade]read size %d write to %p\n", rc, boot_address);
	printk("Load second core firmware success!\n");
	return 0;
}
#elif defined(CONFIG_SECOND_CORE_LOAD_BY_FLASH_BINARY)
int remote_core_loadFw(struct remoteproc *rproc)
{
	char *boot_address = (char *)PSRAM_FW_WR_ADDRESS;
	char *binary_address = (char*)REMOTE_CORE_IMAGE_BINARY_FLASH_ADDRESS;
	uint32_t fwsize = REMOTE_CORE_IMAGE_BINARY_SIZE;

	
	__ASSERT(boot_address,"Sencond core boot address error!");
	__ASSERT(binary_address,"Sencond core firmware load address error!");
	__ASSERT(fwsize,"Sencond core firmware size error!");

	memcpy(boot_address, binary_address, fwsize);
	sys_cache_data_range(boot_address, fwsize, K_CACHE_WB);
	printk("[Upgrade]read data from %p size %d write to %p\n", binary_address,fwsize, boot_address);
	printk("Load second core firmware success!\n");

	return 0;
}
#elif defined(CONFIG_SECOND_CORE_LOAD_BY_FLASH_ELF)

static struct mem_file fw_image = {
	.base = (void *)REMOTE_CORE_IMAGE_ELF_FLASH_ADDRESS,
};
int remote_core_loadFw(struct remoteproc *rproc)
{
	int ret;
	void *store = &fw_image;
	struct image_store_ops *store_ops = &mem_image_store_ops;
	if (rproc == NULL)
		return -EINVAL;
	/* Configure remoteproc to get ready to load executable */
	remoteproc_config(rproc, NULL);
	/* Load remoteproc executable */
	LPRINTF("Start to load executable with remoteproc_load() \r\n");
	ret = remoteproc_load(rproc, NULL, store, store_ops, NULL);
	if (ret) {
		LPRINTF("failed to load firmware\r\n");
		return ret;
	}
	return 0;
}
#else
#error "Unsupport the load fw method!"
#endif

int remote_core_config(struct remoteproc *rproc, void *data)
{
	int ret;
	/* Start the processor */
	ret = remoteproc_config(rproc, data);
	if (ret) {
		LPRINTF("failed to config processor\r\n");
		return ret;
	}
	return 0;
}
int remote_core_start(struct remoteproc *rproc)
{
	int ret;
	/* Start the processor */
	ret = remoteproc_start(rproc);
	if (ret) {
		LPRINTF("failed to start processor\r\n");
		return ret;
	}
	return 0;
}

int remote_core_stop(struct remoteproc *rproc)
{
	int ret;
	/* Stop the processor */
	ret = remoteproc_stop(rproc);
	if (ret) {
		LPRINTF("failed to start processor\r\n");
		return ret;
	}
	return 0;
}

int remote_core_shutdown(struct remoteproc *rproc)
{
	int ret;
	/* Shutdown the processor */
	ret = remoteproc_shutdown(rproc);
	if (ret) {
		LPRINTF("failed to start processor\r\n");
		return ret;
	}
	return 0;
}

int remote_core_remove(struct remoteproc *rproc)
{
	int ret;
	/* Shutdown the processor */
	ret = remoteproc_remove(rproc);
	if (ret) {
		LPRINTF("failed to remove processor\r\n");
		return ret;
	}
	return 0;
}