/*
 * Copyright (c) 2019 Peter Bigot Consulting, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Sample which uses the filesystem API with littlefs */

#include <stdio.h>

#include <zephyr.h>
#include <device.h>
// #include <fs/fs.h>
// #include <fs/littlefs.h>
#include "csk_fs.h"
#include <storage/flash_map.h>
#include "sqlite3.h"

/* Matches LFS_NAME_MAX */
#define MAX_PATH_LEN 255

#define PARTITION_NODE DT_NODELABEL(lfs1)

#if DT_NODE_EXISTS(PARTITION_NODE)
FS_FSTAB_DECLARE_ENTRY(PARTITION_NODE);
#else /* PARTITION_NODE */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FLASH_AREA_ID(storage),
	.mnt_point = "/lfs",
};
#endif /* PARTITION_NODE */

const char *data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName)
{
	int i;
	printk("%s: ", (const char *)data);
	for (i = 0; i < argc; i++) {
		printk("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	printk("\n");
	return 0;
}

int db_open(const char *filename, sqlite3 **db)
{
	int rc = sqlite3_open(filename, db);
	if (rc) {
		printk("Can't open database: %s\n", sqlite3_errmsg(*db));
		return rc;
	} else {
		printk("Opened database successfully\n");
	}
	return rc;
}

char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql)
{
	printk("%s\n", sql);

	uint32_t start = k_cycle_get_32();
	int rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);
	if (rc != SQLITE_OK) {
		printk("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	} else {
		printk("Operation done successfully\n");
	}
	uint32_t time =
		(double)(k_cycle_get_32() - start) / sys_clock_hw_cycles_per_sec() * 1000 * 1000;
	printk("Time taken: %d us\n", time);
	return rc;
}

void main(void)
{
	sqlite3 *db1;
	sqlite3 *db2;
	struct fs_mount_t *mp =
#if DT_NODE_EXISTS(PARTITION_NODE)
		&FS_FSTAB_ENTRY(PARTITION_NODE)
#else
		&lfs_storage_mnt
#endif
		;
	int rc = 0;
	/* Do not mount if auto-mount has been enabled */
#if !DT_NODE_EXISTS(PARTITION_NODE) ||                                                             \
	!(FSTAB_ENTRY_DT_MOUNT_FLAGS(PARTITION_NODE) & FS_MOUNT_FLAG_AUTOMOUNT)
	rc = fs_mount(mp);
	if (rc < 0) {
		printk("FAIL: mount id %" PRIuPTR " at %s: %d\n", (uintptr_t)mp->storage_dev,
		       mp->mnt_point, rc);
		return;
	}
	printk("%s mount: %d\n", mp->mnt_point, rc);
#else
	printk("%s automounted\n", mp->mnt_point);
#endif

	struct fs_file_t file;

	fs_file_t_init(&file);

	sqlite3_initialize();

	struct fs_dirent dirent;
	if (fs_stat("/lfs1/test1.db", &dirent) >= 0) {
		rc = fs_unlink("/lfs1/test1.db");
		printk("fs_unlink: rc = %d\n", rc);
	}
	if (fs_stat("/lfs1/test2.db", &dirent) >= 0) {
		rc = fs_unlink("/lfs1/test2.db");
		printk("fs_unlink: rc = %d\n", rc);
	}

	if (db_open("/lfs1/test1.db", &db1))
		return;
	if (db_open("/lfs1/test2.db", &db2))
		return;

	rc = db_exec(db1, "CREATE TABLE test1 (id INTEGER, content);");
	if (rc != SQLITE_OK) {
		sqlite3_close(db1);
		sqlite3_close(db2);
		return;
	}
	rc = db_exec(db2, "CREATE TABLE test2 (id INTEGER, content);");
	if (rc != SQLITE_OK) {
		sqlite3_close(db1);
		sqlite3_close(db2);
		return;
	}

	rc = db_exec(db1, "INSERT INTO test1 VALUES (1, 'Hello, World from test1');");
	if (rc != SQLITE_OK) {
		sqlite3_close(db1);
		sqlite3_close(db2);
		return;
	}
	// rc = db_exec(db1, "INSERT INTO test1 VALUES (2, 'over, World from test1');");
	// if (rc != SQLITE_OK) {
	// 	sqlite3_close(db1);
	// 	sqlite3_close(db2);
	// 	return;
	// }
	rc = db_exec(db2, "INSERT INTO test2 VALUES (1, 'Hello, World from test2');");
	if (rc != SQLITE_OK) {
		sqlite3_close(db1);
		sqlite3_close(db2);
		return;
	}

	rc = db_exec(db1, "SELECT * FROM test1");
	if (rc != SQLITE_OK) {
		sqlite3_close(db1);
		sqlite3_close(db2);
		return;
	}
	rc = db_exec(db2, "SELECT * FROM test2");
	if (rc != SQLITE_OK) {
		sqlite3_close(db1);
		sqlite3_close(db2);
		return;
	}

	sqlite3_close(db1);
	sqlite3_close(db2);

	rc = fs_unmount(mp);
	printk("%s unmount: %d\n", mp->mnt_point, rc);
	printk("CSK sqlite3 test complete!\n");
}
