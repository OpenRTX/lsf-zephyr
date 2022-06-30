#ifndef __BT_ORG__
#define __BT_ORG__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


//#define PRINT_TREE_MODE				1

#define PRINT_ENABLE
#ifdef PRINT_ENABLE
#define PRINT(format,...) printf("" format" ", ##__VA_ARGS__)
#else
#define PRINT(format,...)
#endif

#define PRINT_E_ENABLE
#ifdef PRINT_E_ENABLE
#define PRINT_E(format,...) printf("error***" format"***/n", ##__VA_ARGS__)
#else
#define PRINT_E(format,...)
#endif

#define FSAT_MALLOC(ptr_base, sz, usz) (void*)ptr_base; ptr_base += (((sz+3)>>2)*4); usz += (((sz+3)>>2)*4)
#define FSAT_OFFSET(addr, org_addr) (int)((char *)addr - (char *)org_addr)
#define FSAT_ADDR(org_addr, offset) ((char *)org_addr + offset)

// Default order num
#define DEFAULT_ORDER			13

// Minimum order is necessarily 3.  We set the maximum
// order arbitrarily.  You may change the maximum order.
#define MIN_ORDER				3
#define MAX_ORDER				20

#ifdef __cplusplus
extern "C"
{
#endif

/* error code */
typedef enum {
    BT_NO_ERR,
	BT_NORMAL_ERR,
    BT_READ_ERR,
    BT_WRITE_ERR,
    BT_NOT_FOUND,
    BT_SAVED_FULL,
    BT_INIT_FAILED,
	BT_DEINIT_FAILED,
} bt_err_e;

// TYPES.

typedef struct record {
	unsigned short value;
	uint32_t offset;
	unsigned int len;
} record;

typedef struct node {
	int pointers;
	int keys;
	int parent;
	short is_leaf;
	short num_keys;
#if PRINT_TREE_MODE
	int next; // Used for queue.
#endif
} node;

typedef struct mem_info {
	char *org_addr;
	char *addr;
	unsigned int size;
} mem_info;

// FUNCTION PROTOTYPES.
#if PRINT_TREE_MODE
void enqueue(void *addr, node * new_node);
node * dequeue(void *addr);
#endif
int height(void *addr, node * const root);
int path_to_root(void *addr, node * const root, node * child);
void print_leaves(void *addr, node * const root);
#if PRINT_TREE_MODE
void print_tree(void *addr, node * const root);
#endif
void find_and_print(void *addr, node * const root, void *key, bool verbose);
//void find_and_print_range(void *addr, node * const root, int range1, int range2, bool verbose);
int find_range(void *addr, node * const root, void *key_start, void *key_end, bool verbose,
		void *returned_keys[], void * returned_pointers[]);
int find_leaf(void *addr, node *root, void *key, bool verbose);
int find(void *addr, node * root, void *key, bool verbose, node ** leaf_out);
int cut(int length);

// Insertion.

int make_record(void *addr, record value);
int make_node(void *addr);
int make_leaf(void *addr);
int get_left_index(void *addr, node * parent, node * left);
int insert_into_leaf(void *addr, node * leaf, void *key, record * pointer);
int insert_into_leaf_after_splitting(void *addr, node * root, node * leaf, void *key,
                                        record * pointer);
int insert_into_node(void *addr, node * root, node * parent,
		int left_index, void *key, node * right);

int insert_into_node_after_splitting(void *addr, node * root, node * parent,
                                        int left_index, void *key, node * right);
int insert_into_parent(void *addr, node * root, node * left, void *key, node * right);
int insert_into_new_root(void *addr, node * left, void *key, node * right);
int start_new_tree(void *addr, void *key, record * pointer);
int insert(void *addr, node * root, void *key, record value);

// Deletion.
#if 0
int get_neighbor_index(node * n);
node * adjust_root(node * root);
node * coalesce_nodes(node * root, node * n, node * neighbor,
                      int neighbor_index, int k_prime);
node * redistribute_nodes(node * root, node * n, node * neighbor,
                          int neighbor_index,
		int k_prime_index, int k_prime);
node * delete_entry(node * root, node * n, int key, void * pointer);
node * delete_(node * root, int key);
#endif

unsigned int ls_crc32(const unsigned char *buf, unsigned int size);

#ifdef __cplusplus
};
#endif

#endif /* __BT_ORG__ */

