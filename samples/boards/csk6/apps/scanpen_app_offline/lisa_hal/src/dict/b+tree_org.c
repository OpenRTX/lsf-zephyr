/*
 *
 *  auther	:  wangyang
 *  date	:  2021.08.27
 *  version	:  dict.v1000.2.0.0
 *  info	:  B+ Tree Implementation
 *
 *  Usage:  bpt [order]
 *  where order is an optional argument
 *  (integer MIN_ORDER <= order <= MAX_ORDER)
 *  defined as the maximal number of pointers in any node.
 *
 */

#include "b+tree_org.h"

// GLOBALS.

/* The order determines the maximum and minimum
 * number of entries (keys and pointers) in any
 * node.  Every node has at most order - 1 keys and
 * at least (roughly speaking) half that number.
 * Every leaf has as many pointers to data as keys,
 * and every internal node has one more pointer
 * to a subtree than the number of keys.
 * This global variable is initialized to the
 * default value.
 */
int order = DEFAULT_ORDER;

/* The queue is used to print the tree in
 * level order, starting from the root
 * printing each entire rank on a separate
 * line, finishing with the leaves.
 */
node * queue = NULL;

/* The user can toggle on and off the "verbose"
 * property, which causes the pointer addresses
 * to be printed out in hexadecimal notation
 * next to their corresponding keys.
 */
bool verbose_output = false;


// FUNCTION DEFINITIONS.

#if PRINT_TREE_MODE
/* Helper function for printing the
 * tree out.  See print_tree.
 */
void enqueue(void *addr, node * new_node) {
	node * c;
	mem_info *bt_addr = addr;
	if (queue == NULL) {
		queue = new_node;
		queue->next = 0;
	}
	else {
		c = queue;
		while(c->next != 0) {
			c = FSAT_ADDR(bt_addr->org_addr, c->next);
		}
		c->next = FSAT_OFFSET(new_node, bt_addr->org_addr);
		new_node->next = 0;
	}
}


/* Helper function for printing the
 * tree out.  See print_tree.
 */
node * dequeue(void *addr) {
	node * n = queue;
	mem_info *bt_addr = addr;
	if (queue->next > 0)
		queue = FSAT_ADDR(bt_addr->org_addr, queue->next);
	else
		queue = NULL;
	n->next = 0;
	return n;
}
#endif


static const unsigned int crc32tab[] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL,
	0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
	0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L,
	0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
	0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
	0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL,
	0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L,
	0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L,
	0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L,
	0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
	0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L,
	0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
	0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
	0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL,
	0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
	0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L,
	0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL,
	0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
	0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL,
	0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
	0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
	0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L,
	0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
	0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L,
	0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L,
	0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
	0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL,
	0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
	0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
	0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL,
	0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
	0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL,
	0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L,
	0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
	0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L,
	0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
	0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
	0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L,
	0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
	0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL,
	0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L,
	0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
	0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL,
	0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
	0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
	0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L,
	0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
	0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L,
	0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L,
	0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
	0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L,
	0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL
};

unsigned int ls_crc32(const unsigned char *buf, unsigned int size)
{
	unsigned int i, crc;
	crc = 0xFFFFFFFF;

	for (i = 0; i < size; i++)
		crc = crc32tab[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);

	return crc ^ 0xFFFFFFFF;
}

/* Prints the bottom row of keys
 * of the tree (with their respective
 * pointers, if the verbose_output flag is set.
 */
void print_leaves(void *addr, node * const root) {
	if (root == NULL) {
		PRINT("Empty tree.\n");
		return;
	}
	int i;
	mem_info *bt_addr = addr;
	node * c = root;
	while (!c->is_leaf)
	{
		int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, c->pointers);
		int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, c->keys);
		c = (node *)FSAT_ADDR(bt_addr->org_addr, pointers[0]);
	}
	while (true) {
		int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, c->pointers);
		int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, c->keys);
		for (i = 0; i < c->num_keys; i++) {
			if (verbose_output)
				PRINT("%p ", FSAT_ADDR(bt_addr->org_addr, pointers[i]));
			PRINT("%s ", FSAT_ADDR(bt_addr->org_addr, keys[i]));
		}
		if (verbose_output)
			PRINT("%p ", FSAT_ADDR(bt_addr->org_addr, pointers[order - 1]));
		if (FSAT_ADDR(bt_addr->org_addr, pointers[order - 1]) != 0) {
			PRINT(" | ");
			c = (node *)FSAT_ADDR(bt_addr->org_addr, pointers[order - 1]);
		}
		else
			break;
	}
	PRINT("\n");
}


/* Utility function to give the height
 * of the tree, which length in number of edges
 * of the path from the root to any leaf.
 */
int height(void *addr, node * const root) {
	int h = 0;
	mem_info *bt_addr = addr;
	node * c = root;
	while (!c->is_leaf) {
		int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, c->pointers);
		c = (node *)FSAT_ADDR(bt_addr->org_addr, pointers[0]);
		h++;
	}
	return h;
}


/* Utility function to give the length in edges
 * of the path from any node to the root.
 */
int path_to_root(void *addr, node * const root, node * child) {
	int length = 0;
	mem_info *bt_addr = addr;
	node * c = child;
	while (c != root) {
		
		c = (node *)FSAT_ADDR(bt_addr->org_addr, c->parent);
		length++;
	}
	return length;
}

#if PRINT_TREE_MODE
/* Prints the B+ tree in the command
 * line in level (rank) order, with the 
 * keys in each node and the '|' symbol
 * to separate nodes.
 * With the verbose_output flag set.
 * the values of the pointers corresponding
 * to the keys also appear next to their respective
 * keys, in hexadecimal notation.
 */
void print_tree(void *addr, node * const root) {

	node * n = NULL;
	int n_offset = 0;
	int i = 0;
	int rank = 0;
	int new_rank = 0;
	mem_info *bt_addr = addr;

	if (root == NULL) {
		PRINT("Empty tree.\n");
		return;
	}
	queue = NULL;
	enqueue(bt_addr, root);
	while(queue != NULL) {
		//n_offset = dequeue(bt_addr);
		//n = FSAT_ADDR(bt_addr->org_addr, n_offset);
		n = dequeue(bt_addr);
		node * parent = (node *)FSAT_ADDR(bt_addr->org_addr, n->parent);
		int *pointers = FSAT_ADDR(bt_addr->org_addr, n->pointers);
		int *keys = FSAT_ADDR(bt_addr->org_addr, n->keys);
		int *parent_pointers = FSAT_ADDR(bt_addr->org_addr, parent->pointers);
		int *parent_keys = FSAT_ADDR(bt_addr->org_addr, parent->keys);

		n_offset = FSAT_OFFSET(n, bt_addr->org_addr);
		if (n->parent != 0 && n_offset == parent_pointers[0]) {
			new_rank = path_to_root(bt_addr, root, n);
			if (new_rank != rank) {
				rank = new_rank;
				PRINT("\n");
			}
		}
		if (verbose_output) 
			PRINT("(%p)", n);
		for (i = 0; i < n->num_keys; i++) {
			if (verbose_output)
				PRINT("%p ", FSAT_ADDR(bt_addr->org_addr, pointers[i]));
			PRINT("%s ", (char *)FSAT_ADDR(bt_addr->org_addr, keys[i]));
		}
		if (!n->is_leaf)
			for (i = 0; i <= n->num_keys; i++)
				enqueue(bt_addr, FSAT_ADDR(bt_addr->org_addr, pointers[i]));
		if (verbose_output) {
			if (n->is_leaf) 
				PRINT("%p ", FSAT_ADDR(bt_addr->org_addr, pointers[order - 1]));
			else
				PRINT("%p ", FSAT_ADDR(bt_addr->org_addr, pointers[n->num_keys]));
		}
		PRINT("| ");
	}
	PRINT("\n");
}
#endif

/* Finds the record under a given key and prints an
 * appropriate message to stdout.
 */
void find_and_print(void *addr, node * const root, void *key, bool verbose) {
    node * leaf = NULL;
	mem_info *bt_addr = addr;
	int r_offset = find(bt_addr, root, key, verbose, NULL);
	if (r_offset == -1)
	{
		PRINT("Record not found under key %s.\n", (char *)key);
	}
	else
	{
		record * r = (record *)FSAT_ADDR(bt_addr->org_addr, r_offset);
		PRINT("Record at %p -- key %s, num %d offset %d .\n",
			r, (char *)key, r->value, r->offset);
	}

}

#if 0
/* Finds and prints the keys, pointers, and values within a range
 * of keys between key_start and key_end, including both bounds.
 */
void find_and_print_range(node * const root, int key_start, int key_end,
		bool verbose) {
	int i;
	int array_size = key_end - key_start + 1;
	int returned_keys[array_size];
	void * returned_pointers[array_size];
	int num_found = find_range(root, key_start, key_end, verbose,
			returned_keys, returned_pointers);
	if (!num_found)
		PRINT("None found.\n");
	else {
		for (i = 0; i < num_found; i++)
			PRINT("Key: %d   Location: %p  Value: %d\n",
					returned_keys[i],
					returned_pointers[i],
					((record *)
					 returned_pointers[i])->value);
	}
}


/* Finds keys and their pointers, if present, in the range specified
 * by key_start and key_end, inclusive.  Places these in the arrays
 * returned_keys and returned_pointers, and returns the number of
 * entries found.
 */
int find_range(void *addr, node * const root, void *key_start, void *key_end, bool verbose,
		void * returned_keys[], void * returned_pointers[]) {
	int i, num_found;
	num_found = 0;
	mem_info *bt_addr = addr;

	int n_offset = find_leaf(bt_addr, root, key_start, verbose);
	if (n_offset == 0) return 0;
	node * n = FSAT_ADDR(bt_addr->org_addr, n_offset);

	for (i = 0; i < n->num_keys && strcmp(FSAT_ADDR(bt_addr->org_addr, n->keys[i]), key_start) < 0/*n->keys[i] < key_start*/; i++) ;
	if (i == n->num_keys) return 0;
	while (n != 0) {
		for (; i < n->num_keys && strcmp(FSAT_ADDR(bt_addr->org_addr, n->keys[i]), key_end) <= 0/*n->keys[i] <= key_end*/; i++) {
			returned_keys[num_found] = FSAT_ADDR(bt_addr->org_addr, n->keys[i]);
			returned_pointers[num_found] = FSAT_ADDR(bt_addr->org_addr, n->pointers[i]);
			num_found++;
		}
		n = FSAT_ADDR(bt_addr->org_addr, n->pointers[order - 1]);
		i = 0;
	}
	return num_found;
}
#endif

/* Traces the path from the root to a leaf, searching
 * by key.  Displays information about the path
 * if the verbose flag is set.
 * Returns the leaf containing the given key.
 */
int find_leaf(void *addr, node *root, void *key, bool verbose) {
	if (root == NULL) {
		if (verbose) 
			PRINT("Empty tree.\n");
		return 0;
	}
	int i = 0;
	mem_info *bt_addr = (mem_info *)addr;

	node * c = root;
	while (!c->is_leaf) {
		int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, c->pointers);
		int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, c->keys);
		if (verbose) {
			PRINT("[");
			for (i = 0; i < c->num_keys - 1; i++)
				PRINT("%s ", (char *)FSAT_ADDR(bt_addr->org_addr, keys[i]));
			PRINT("%s] ", (char *)FSAT_ADDR(bt_addr->org_addr, keys[i]));
		}
		i = 0;
		while (i < c->num_keys) {
			if (strcmp(key, (char *)FSAT_ADDR(bt_addr->org_addr, keys[i])) >= 0 /*key >= c->keys[i]*/) i++;
			else break;
		}
		if (verbose)
			PRINT("%d ->\n", i);
		c = (node *)FSAT_ADDR(bt_addr->org_addr, pointers[i]);
	}

	int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, c->pointers);
	int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, c->keys);
	if (verbose) {
		PRINT("Leaf [");
		for (i = 0; i < c->num_keys - 1; i++)
			PRINT("%s ", (char *)FSAT_ADDR(bt_addr->org_addr, keys[i]));
		PRINT("%s] ->\n", (char *)FSAT_ADDR(bt_addr->org_addr, keys[i]));
	}
	return FSAT_OFFSET(c, bt_addr->org_addr);
}


/* Finds and returns the record to which
 * a key refers.
 */
int find(void *addr, node *root, void *key, bool verbose, node ** leaf_out) {
    if (root == NULL) {
        if (leaf_out != NULL) {
            *leaf_out = NULL;
        }
        return -1;
    }

	int i = 0;
	mem_info *bt_addr = addr;
    node * leaf = NULL;
	int leaf_offset = 0;

	leaf_offset = find_leaf(bt_addr, root, key, verbose);
	leaf = (node *)FSAT_ADDR(bt_addr->org_addr, leaf_offset);
    /* If root != NULL, leaf must have a value, even
     * if it does not contain the desired key.
     * (The leaf holds the range of keys that would
     * include the desired key.) 
     */
	int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, leaf->pointers);
	int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, leaf->keys);

	for (i = 0; i < leaf->num_keys; i++)
	{
		if (strcmp(FSAT_ADDR(bt_addr->org_addr, keys[i]), key) == 0/*leaf->keys[i] == key*/)
		{
			break;
		}
	}
    if (leaf_out != NULL) {
        *leaf_out = leaf;
    }
	if (i == leaf->num_keys)
		return -1;
	else
		return pointers[i];
}

/* Finds the appropriate place to
 * split a node that is too big into two.
 */
int cut(int length) {
	if (length % 2 == 0)
		return length/2;
	else
		return length/2 + 1;
}


// INSERTION

/* Creates a new record to hold the value
 * to which a key refers.
 */
int make_record(void *addr, record value) {
	mem_info *bt_addr = addr;
	record * new_record = (record *)FSAT_MALLOC(bt_addr->addr, sizeof(record), bt_addr->size);// (record *)malloc(sizeof(record));
	if (new_record == NULL) {
		PRINT_E("Record creation.");
		return -1;
	}
	else {
		new_record->value = value.value;
		new_record->offset = value.offset;
		new_record->len = value.len;
	}
	return FSAT_OFFSET(new_record, bt_addr->org_addr);
}


/* Creates a new general node, which can be adapted
 * to serve as either a leaf or an internal node.
 */
int make_node(void *addr) {
	node * new_node;
	mem_info *bt_addr = addr;
	new_node = (node *)FSAT_MALLOC(bt_addr->addr, sizeof(node), bt_addr->size); //malloc(sizeof(node));
	if (new_node == NULL) {
		PRINT_E("Node creation.");
		return 0;
	}
	int *keys = FSAT_MALLOC(bt_addr->addr, (order - 1) * sizeof(int), bt_addr->size);
	for (int i = 0; i < (order - 1); i++)
	{
		keys[i] = 0;// FSAT_OFFSET(keys, bt_addr->org_addr) + i * sizeof(int);
	}
	new_node->keys = FSAT_OFFSET(keys, bt_addr->org_addr);
	//new_node->keys = FSAT_OFFSET(keys, bt_addr->org_addr);//keys - bt_addr->org_addr;
	if (keys == NULL) {
		PRINT_E("New node keys array.");
		return 0;
	}
	int *pointers = FSAT_MALLOC(bt_addr->addr, order * sizeof(int), bt_addr->size);
	for (int i = 0; i < (order - 1); i++)
	{
		pointers[i] = 0;// FSAT_OFFSET(pointers, bt_addr->org_addr) + i * sizeof(int);
	}
	new_node->pointers = FSAT_OFFSET(pointers, bt_addr->org_addr);
	//new_node->pointers = FSAT_OFFSET(pointers, bt_addr->org_addr);//pointers - bt_addr->org_addr;
	if (pointers == NULL) {
		PRINT_E("New node pointers array.");
		return 0;
	}
	new_node->is_leaf = false;
	new_node->num_keys = 0;
	new_node->parent = 0;
#if PRINT_TREE_MODE
	new_node->next = 0;
#endif
	return FSAT_OFFSET(new_node, bt_addr->org_addr); //new_node - bt_addr->org_addr;
}

/* Creates a new leaf by creating a node
 * and then adapting it appropriately.
 */
int make_leaf(void *addr) {
	mem_info *bt_addr = addr;
	int leaf_offset = make_node(bt_addr);
	node *leaf = (node *)FSAT_ADDR(bt_addr->org_addr, leaf_offset);//get_node_from_offset(leaf_offset, bt_addr->org_addr);
	leaf->is_leaf = true;
	return leaf_offset;
}


/* Helper function used in insert_into_parent
 * to find the index of the parent's pointer to 
 * the node to the left of the key to be inserted.
 */
int get_left_index(void *addr, node * parent, node * left) {

	int left_index = 0;
	mem_info *bt_addr = addr;

	int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, parent->pointers);
	while (left_index <= parent->num_keys && 
		(node *)FSAT_ADDR(bt_addr->org_addr, pointers[left_index]) != left)
		left_index++;
	return left_index;
}

/* Inserts a new pointer to a record and its corresponding
 * key into a leaf.
 * Returns the altered leaf.
 */
int insert_into_leaf(void *addr, node * leaf, void *key, record * pointer) {

	int i, insertion_point;
	mem_info *bt_addr = addr;

	insertion_point = 0;
	int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, leaf->keys);
	while (insertion_point < leaf->num_keys && strcmp((char *)FSAT_ADDR(bt_addr->org_addr, keys[insertion_point]), key) < 0/*leaf->keys[insertion_point] < key*/)
		insertion_point++;

	int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, leaf->pointers);
	for (i = leaf->num_keys; i > insertion_point; i--) {
		keys[i] = keys[i - 1];
		pointers[i] = pointers[i - 1];
	}

	char *keys_buff = FSAT_MALLOC(bt_addr->addr, strlen((char *)key) + 1, bt_addr->size);
	strcpy(keys_buff, key);
	keys[insertion_point] = FSAT_OFFSET(keys_buff, bt_addr->org_addr);
	
	pointers[insertion_point] = FSAT_OFFSET(pointer, bt_addr->org_addr);
	leaf->num_keys++;
	return FSAT_OFFSET(leaf, bt_addr->org_addr); //leaf;
}


/* Inserts a new key and pointer
 * to a new record into a leaf so as to exceed
 * the tree's order, causing the leaf to be split
 * in half.
 */
int insert_into_leaf_after_splitting(void *addr, node * root, node * leaf, void *key, record * pointer) {

	mem_info *bt_addr = addr;
	node * new_leaf;
	int *temp_keys;	
	int *temp_pointers;
	void * new_key;
	int insertion_index, split, i, j;

	int new_leaf_offset = make_leaf(bt_addr);
	new_leaf = (node *)FSAT_ADDR(bt_addr->org_addr, new_leaf_offset);

	temp_keys = malloc(order * sizeof(int));
	if (temp_keys == NULL) {
		PRINT_E("Temporary keys array.");
		return -1;
	}

	temp_pointers = malloc(order * sizeof(int));
	if (temp_pointers == NULL) {
		PRINT_E("Temporary pointers array.");
		return -1;
	}

	insertion_index = 0;
	int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, leaf->keys);
	while (insertion_index < order - 1 && strcmp((char *)FSAT_ADDR(bt_addr->org_addr, keys[insertion_index]), key) < 0/*leaf->keys[insertion_index] < key*/)
		insertion_index++;

	int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, leaf->pointers);
	for (i = 0, j = 0; i < leaf->num_keys; i++, j++) {
		if (j == insertion_index) j++;
		temp_keys[j] = keys[i];
		temp_pointers[j] = pointers[i];
	}
 
	char *keys_buff= FSAT_MALLOC(bt_addr->addr, strlen((char *)key) + 1, bt_addr->size);	
	strcpy(keys_buff, key);
	temp_keys[insertion_index] = FSAT_OFFSET(keys_buff, bt_addr->org_addr);
	temp_pointers[insertion_index] = FSAT_OFFSET(pointer, bt_addr->org_addr);

	leaf->num_keys = 0;

	split = cut(order - 1);

	for (i = 0; i < split; i++) {
		pointers[i] = temp_pointers[i];
		keys[i] = temp_keys[i];
		leaf->num_keys++;
	}

	int *new_leaf_pointers = (int *)FSAT_ADDR(bt_addr->org_addr, new_leaf->pointers);
	int *new_leaf_keys = (int *)FSAT_ADDR(bt_addr->org_addr, new_leaf->keys);
	for (i = split, j = 0; i < order; i++, j++) {
		new_leaf_pointers[j] = temp_pointers[i];
		new_leaf_keys[j] = temp_keys[i];
		new_leaf->num_keys++;
	}

	free(temp_pointers);
	free(temp_keys);

	new_leaf_pointers[order - 1] = pointers[order - 1];
	pointers[order - 1] = new_leaf_offset;//new_leaf;

	for (i = leaf->num_keys; i < order - 1; i++)
		pointers[i] = 0;
	for (i = new_leaf->num_keys; i < order - 1; i++)
		new_leaf_pointers[i] = 0;

	new_leaf->parent = leaf->parent;
	new_key = FSAT_ADDR(bt_addr->org_addr, new_leaf_keys[0]);

	return insert_into_parent(bt_addr, root, leaf, new_key, new_leaf);
}


/* Inserts a new key and pointer to a node
 * into a node into which these can fit
 * without violating the B+ tree properties.
 */
int insert_into_node(void *addr, node * root, node * n,
		int left_index, void *key, node * right) {
	int i;
	mem_info *bt_addr = addr;

	int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, n->keys);
	int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, n->pointers);

	for (i = n->num_keys; i > left_index; i--) {
		pointers[i + 1] = pointers[i];
		keys[i] = keys[i - 1];
	}
	pointers[left_index + 1] = FSAT_OFFSET(right, bt_addr->org_addr);
	char *keys_buff = FSAT_MALLOC(bt_addr->addr, strlen((char *)key) + 1, bt_addr->size);
	keys[left_index] = FSAT_OFFSET(keys_buff, bt_addr->org_addr);
	strcpy(keys_buff, key);
	n->num_keys++;
	return FSAT_OFFSET(root, bt_addr->org_addr);
}


/* Inserts a new key and pointer to a node
 * into a node, causing the node's size to exceed
 * the order, and causing the node to split into two.
 */
int insert_into_node_after_splitting(void *addr, node * root, 
	node * old_node, int left_index, void *key, node * right) {

	mem_info *bt_addr = addr;
	int i, j, split, new_node_offset;
	node * new_node, *child;
	void *k_prime;
	int *temp_keys;
	int *temp_pointers;

	/* First create a temporary set of keys and pointers
	 * to hold everything in order, including
	 * the new key and pointer, inserted in their
	 * correct places. 
	 * Then create a new node and copy half of the 
	 * keys and pointers to the old node and
	 * the other half to the new.
	 */

	temp_pointers = malloc((order + 1) * sizeof(int));
	if (temp_pointers == NULL) {
		PRINT_E("Temporary pointers array for splitting nodes.");
		return -1;
	}
	temp_keys = malloc(order * sizeof(int));
	if (temp_keys == NULL) {
		PRINT_E("Temporary keys array for splitting nodes.");
		return -1;
	}

	int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, old_node->keys);
	int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, old_node->pointers);

	for (i = 0, j = 0; i < old_node->num_keys + 1; i++, j++) {
		if (j == left_index + 1) j++;
		temp_pointers[j] = pointers[i];
	}

	for (i = 0, j = 0; i < old_node->num_keys; i++, j++) {
		if (j == left_index) j++;
		temp_keys[j] = keys[i];
	}

	temp_pointers[left_index + 1] = FSAT_OFFSET(right, bt_addr->org_addr);
	char *keys_buff = FSAT_MALLOC(bt_addr->addr, strlen((char *)key) + 1, bt_addr->size);
	strcpy(keys_buff, key);
	temp_keys[left_index] = FSAT_OFFSET(keys_buff, bt_addr->org_addr);
	
	/* Create the new node and copy
	 * half the keys and pointers to the
	 * old and half to the new.
	 */  
	split = cut(order);
	
	new_node_offset = make_node(bt_addr);
	new_node = (node *)FSAT_ADDR(bt_addr->org_addr, new_node_offset);
	old_node->num_keys = 0;
	for (i = 0; i < split - 1; i++) {
		pointers[i] = temp_pointers[i];
		keys[i] = temp_keys[i];
		old_node->num_keys++;
	}
	pointers[i] = temp_pointers[i];

	k_prime = FSAT_ADDR(bt_addr->org_addr, temp_keys[split - 1]);
	int *new_node_keys = (int *)FSAT_ADDR(bt_addr->org_addr, new_node->keys);
	int *new_node_pointers = (int *)FSAT_ADDR(bt_addr->org_addr, new_node->pointers);
	for (++i, j = 0; i < order; i++, j++) {
		new_node_pointers[j] = temp_pointers[i];
		new_node_keys[j] = temp_keys[i];
		new_node->num_keys++;
	}
	new_node_pointers[j] = temp_pointers[i];
	free(temp_pointers);
	free(temp_keys);
	new_node->parent = old_node->parent;
	for (i = 0; i <= new_node->num_keys; i++) {
		child = (node *)FSAT_ADDR(bt_addr->org_addr, new_node_pointers[i]);
		child->parent = FSAT_OFFSET(new_node, bt_addr->org_addr);
	}

	/* Insert a new key into the parent of the two
	 * nodes resulting from the split, with
	 * the old node to the left and the new to the right.
	 */

	return insert_into_parent(bt_addr, root, old_node, k_prime, new_node);
}



/* Inserts a new node (leaf or internal node) into the B+ tree.
 * Returns the root of the tree after insertion.
 */
int insert_into_parent(void *addr, node * root, node * left, void *key, node * right) {

	mem_info *bt_addr = addr;
	int left_index;
	node * parent;

	int parent_offset = left->parent;
	parent = (node *)FSAT_ADDR(bt_addr->org_addr, left->parent);

	/* Case: new root. */

	if (parent_offset == 0)
		return insert_into_new_root(bt_addr, left, key, right);

	/* Case: leaf or node. (Remainder of
	 * function body.)  
	 */

	/* Find the parent's pointer to the left 
	 * node.
	 */

	left_index = get_left_index(bt_addr, parent, left);


	/* Simple case: the new key fits into the node. 
	 */

	if (parent->num_keys < order - 1)
		return insert_into_node(bt_addr, root, parent, left_index, key, right);

	/* Harder case:  split a node in order 
	 * to preserve the B+ tree properties.
	 */

	return insert_into_node_after_splitting(bt_addr, root, parent, left_index, key, right);
}


/* Creates a new root for two subtrees
 * and inserts the appropriate key into
 * the new root.
 */
int insert_into_new_root(void *addr, node * left, void *key, node * right) {

	mem_info *bt_addr = addr;
	int root_offset = make_node(bt_addr);
	node * root = (node *)FSAT_ADDR(bt_addr->org_addr, root_offset);
	char *keys_buff = FSAT_MALLOC(bt_addr->addr, strlen((char *)key) + 1, bt_addr->size);	
	strcpy(keys_buff, key);
	int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, root->keys);
	int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, root->pointers);
	keys[0] = FSAT_OFFSET(keys_buff, bt_addr->org_addr);
	pointers[0] = FSAT_OFFSET(left, bt_addr->org_addr);
	pointers[1] = FSAT_OFFSET(right, bt_addr->org_addr);
	root->num_keys++;
	root->parent = 0;
	left->parent = FSAT_OFFSET(root, bt_addr->org_addr);
	right->parent = FSAT_OFFSET(root, bt_addr->org_addr);
	return root_offset;
}



/* First insertion:
 * start a new tree.
 */
int start_new_tree(void *addr, void *key, record * pointer) {

	mem_info *bt_addr = addr;
	int root_offset = make_leaf(bt_addr);
	node *root = (node *)FSAT_ADDR(bt_addr->org_addr, root_offset);
	char *keys_buff = FSAT_MALLOC(bt_addr->addr, strlen((char *)key) + 1, bt_addr->size);
	strcpy(keys_buff, key);

	int *keys = (int *)FSAT_ADDR(bt_addr->org_addr, root->keys);
	int *pointers = (int *)FSAT_ADDR(bt_addr->org_addr, root->pointers);

	keys[0] = FSAT_OFFSET(keys_buff, bt_addr->org_addr);
	pointers[0] = FSAT_OFFSET(pointer, bt_addr->org_addr);
	pointers[order - 1] = 0;
	root->parent = 0;
	root->num_keys++;
	return root_offset;
}



/* Master insertion function.
 * Inserts a key and an associated value into
 * the B+ tree, causing the tree to be adjusted
 * however necessary to maintain the B+ tree
 * properties.
 */
int insert(void *addr, node * root, void *key, record value) {

	mem_info *bt_addr = addr;
	record * record_pointer = NULL;
	node * leaf = NULL;
	int leaf_offset = 0;

	/* The current implementation ignores
	 * duplicates.
	 */

	int record_pointer_offset = find(bt_addr, root, key, false, NULL);
	record_pointer = (record *)FSAT_ADDR(bt_addr->org_addr, record_pointer_offset);
    if (record_pointer_offset != -1) {

        /* If the key already exists in this tree, update
         * the value and return the tree.
         */		
        record_pointer->value = value.value;
		record_pointer->offset = value.offset;
		record_pointer->len = value.len;
        return FSAT_OFFSET(root, bt_addr->org_addr);
    }

	/* Create a new record for the
	 * value.
	 */
	record_pointer_offset = make_record(bt_addr, value);
	record_pointer = (record *)FSAT_ADDR(bt_addr->org_addr, record_pointer_offset);

	/* Case: the tree does not exist yet.
	 * Start a new tree.
	 */

	if (root == NULL) 
		return start_new_tree(bt_addr, key, record_pointer);


	/* Case: the tree already exists.
	 * (Rest of function body.)
	 */

	leaf_offset = find_leaf(bt_addr, root, key, false);

	/* Case: leaf has room for key and record_pointer.
	 */
	leaf = (node *)FSAT_ADDR(bt_addr->org_addr, leaf_offset);
	if (leaf->num_keys < order - 1) {
		leaf_offset = insert_into_leaf(bt_addr, leaf, key, record_pointer);
		return FSAT_OFFSET(root, bt_addr->org_addr);
	}


	/* Case:  leaf must be split.
	 */

	return insert_into_leaf_after_splitting(bt_addr, root, leaf, key, record_pointer);
}



#if 0
// DELETION.

/* Utility function for deletion.  Retrieves
 * the index of a node's nearest neighbor (sibling)
 * to the left if one exists.  If not (the node
 * is the leftmost child), returns -1 to signify
 * this special case.
 */
int get_neighbor_index(node * n) {

	int i;

	/* Return the index of the key to the left
	 * of the pointer in the parent pointing
	 * to n.  
	 * If n is the leftmost child, this means
	 * return -1.
	 */
	for (i = 0; i <= n->parent->num_keys; i++)
		if (n->parent->pointers[i] == n)
			return i - 1;

	// Error state.
	PRINT("Search for nonexistent pointer to node in parent.\n");
	PRINT("Node:  %#lx\n", (unsigned long)n);
	return BT_NORMAL_ERR;
}


node * remove_entry_from_node(node * n, int key, node * pointer) {

	int i, num_pointers;

	// Remove the key and shift other keys accordingly.
	i = 0;
	while (n->keys[i] != key)
		i++;
	for (++i; i < n->num_keys; i++)
		n->keys[i - 1] = n->keys[i];

	// Remove the pointer and shift other pointers accordingly.
	// First determine number of pointers.
	num_pointers = n->is_leaf ? n->num_keys : n->num_keys + 1;
	i = 0;
	while (n->pointers[i] != pointer)
		i++;
	for (++i; i < num_pointers; i++)
		n->pointers[i - 1] = n->pointers[i];


	// One key fewer.
	n->num_keys--;

	// Set the other pointers to NULL for tidiness.
	// A leaf uses the last pointer to point to the next leaf.
	if (n->is_leaf)
		for (i = n->num_keys; i < order - 1; i++)
			n->pointers[i] = NULL;
	else
		for (i = n->num_keys + 1; i < order; i++)
			n->pointers[i] = NULL;

	return n;
}


node * adjust_root(node * root) {

	node * new_root;

	/* Case: nonempty root.
	 * Key and pointer have already been deleted,
	 * so nothing to be done.
	 */

	if (root->num_keys > 0)
		return root;

	/* Case: empty root. 
	 */

	// If it has a child, promote 
	// the first (only) child
	// as the new root.

	if (!root->is_leaf) {
		new_root = root->pointers[0];
		new_root->parent = NULL;
	}

	// If it is a leaf (has no children),
	// then the whole tree is empty.

	else
		new_root = NULL;

	free(root->keys);
	free(root->pointers);
	free(root);

	return new_root;
}


/* Coalesces a node that has become
 * too small after deletion
 * with a neighboring node that
 * can accept the additional entries
 * without exceeding the maximum.
 */
node * coalesce_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime) {

	int i, j, neighbor_insertion_index, n_end;
	node * tmp;

	/* Swap neighbor with node if node is on the
	 * extreme left and neighbor is to its right.
	 */

	if (neighbor_index == -1) {
		tmp = n;
		n = neighbor;
		neighbor = tmp;
	}

	/* Starting point in the neighbor for copying
	 * keys and pointers from n.
	 * Recall that n and neighbor have swapped places
	 * in the special case of n being a leftmost child.
	 */

	neighbor_insertion_index = neighbor->num_keys;

	/* Case:  nonleaf node.
	 * Append k_prime and the following pointer.
	 * Append all pointers and keys from the neighbor.
	 */

	if (!n->is_leaf) {

		/* Append k_prime.
		 */

		neighbor->keys[neighbor_insertion_index] = k_prime;
		neighbor->num_keys++;


		n_end = n->num_keys;

		for (i = neighbor_insertion_index + 1, j = 0; j < n_end; i++, j++) {
			neighbor->keys[i] = n->keys[j];
			neighbor->pointers[i] = n->pointers[j];
			neighbor->num_keys++;
			n->num_keys--;
		}

		/* The number of pointers is always
		 * one more than the number of keys.
		 */

		neighbor->pointers[i] = n->pointers[j];

		/* All children must now point up to the same parent.
		 */

		for (i = 0; i < neighbor->num_keys + 1; i++) {
			tmp = (node *)neighbor->pointers[i];
			tmp->parent = neighbor;
		}
	}

	/* In a leaf, append the keys and pointers of
	 * n to the neighbor.
	 * Set the neighbor's last pointer to point to
	 * what had been n's right neighbor.
	 */

	else {
		for (i = neighbor_insertion_index, j = 0; j < n->num_keys; i++, j++) {
			neighbor->keys[i] = n->keys[j];
			neighbor->pointers[i] = n->pointers[j];
			neighbor->num_keys++;
		}
		neighbor->pointers[order - 1] = n->pointers[order - 1];
	}

	root = delete_entry(root, n->parent, k_prime, n);
	free(n->keys);
	free(n->pointers);
	free(n); 
	return root;
}


/* Redistributes entries between two nodes when
 * one has become too small after deletion
 * but its neighbor is too big to append the
 * small node's entries without exceeding the
 * maximum
 */
node * redistribute_nodes(node * root, node * n, node * neighbor, int neighbor_index, 
		int k_prime_index, int k_prime) {  

	int i;
	node * tmp;

	/* Case: n has a neighbor to the left. 
	 * Pull the neighbor's last key-pointer pair over
	 * from the neighbor's right end to n's left end.
	 */

	if (neighbor_index != -1) {
		if (!n->is_leaf)
			n->pointers[n->num_keys + 1] = n->pointers[n->num_keys];
		for (i = n->num_keys; i > 0; i--) {
			n->keys[i] = n->keys[i - 1];
			n->pointers[i] = n->pointers[i - 1];
		}
		if (!n->is_leaf) {
			n->pointers[0] = neighbor->pointers[neighbor->num_keys];
			tmp = (node *)n->pointers[0];
			tmp->parent = n;
			neighbor->pointers[neighbor->num_keys] = NULL;
			n->keys[0] = k_prime;
			n->parent->keys[k_prime_index] = neighbor->keys[neighbor->num_keys - 1];
		}
		else {
			n->pointers[0] = neighbor->pointers[neighbor->num_keys - 1];
			neighbor->pointers[neighbor->num_keys - 1] = NULL;
			n->keys[0] = neighbor->keys[neighbor->num_keys - 1];
			n->parent->keys[k_prime_index] = n->keys[0];
		}
	}

	/* Case: n is the leftmost child.
	 * Take a key-pointer pair from the neighbor to the right.
	 * Move the neighbor's leftmost key-pointer pair
	 * to n's rightmost position.
	 */

	else {  
		if (n->is_leaf) {
			n->keys[n->num_keys] = neighbor->keys[0];
			n->pointers[n->num_keys] = neighbor->pointers[0];
			n->parent->keys[k_prime_index] = neighbor->keys[1];
		}
		else {
			n->keys[n->num_keys] = k_prime;
			n->pointers[n->num_keys + 1] = neighbor->pointers[0];
			tmp = (node *)n->pointers[n->num_keys + 1];
			tmp->parent = n;
			n->parent->keys[k_prime_index] = neighbor->keys[0];
		}
		for (i = 0; i < neighbor->num_keys - 1; i++) {
			neighbor->keys[i] = neighbor->keys[i + 1];
			neighbor->pointers[i] = neighbor->pointers[i + 1];
		}
		if (!n->is_leaf)
			neighbor->pointers[i] = neighbor->pointers[i + 1];
	}

	/* n now has one more key and one more pointer;
	 * the neighbor has one fewer of each.
	 */

	n->num_keys++;
	neighbor->num_keys--;

	return root;
}


/* Deletes an entry from the B+ tree.
 * Removes the record and its key and pointer
 * from the leaf, and then makes all appropriate
 * changes to preserve the B+ tree properties.
 */
node * delete_entry(node * root, node * n, int key, void * pointer) {

	int min_keys;
	node * neighbor;
	int neighbor_index;
	int k_prime_index, k_prime;
	int capacity;

	// Remove key and pointer from node.

	n = remove_entry_from_node(n, key, pointer);

	/* Case:  deletion from the root. 
	 */

	if (n == root) 
		return adjust_root(root);


	/* Case:  deletion from a node below the root.
	 * (Rest of function body.)
	 */

	/* Determine minimum allowable size of node,
	 * to be preserved after deletion.
	 */

	min_keys = n->is_leaf ? cut(order - 1) : cut(order) - 1;

	/* Case:  node stays at or above minimum.
	 * (The simple case.)
	 */

	if (n->num_keys >= min_keys)
		return root;

	/* Case:  node falls below minimum.
	 * Either coalescence or redistribution
	 * is needed.
	 */

	/* Find the appropriate neighbor node with which
	 * to coalesce.
	 * Also find the key (k_prime) in the parent
	 * between the pointer to node n and the pointer
	 * to the neighbor.
	 */

	neighbor_index = get_neighbor_index(n);
	k_prime_index = neighbor_index == -1 ? 0 : neighbor_index;
	k_prime = n->parent->keys[k_prime_index];
	neighbor = neighbor_index == -1 ? n->parent->pointers[1] : 
		n->parent->pointers[neighbor_index];

	capacity = n->is_leaf ? order : order - 1;

	/* Coalescence. */

	if (neighbor->num_keys + n->num_keys < capacity)
		return coalesce_nodes(root, n, neighbor, neighbor_index, k_prime);

	/* Redistribution. */

	else
		return redistribute_nodes(root, n, neighbor, neighbor_index, k_prime_index, k_prime);
}



/* Master deletion function.
 */
node * delete_(node * root, int key) {

	node * key_leaf = NULL;
	record * key_record = NULL;

	key_record = find(root, key, false, &key_leaf);

    /* CHANGE */

	if (key_record != NULL && key_leaf != NULL) {
		root = delete_entry(root, key_leaf, key, key_record);
		free(key_record);
	}
	return root;
}


void destroy_tree_nodes(node * root) {
	int i;
	if (root->is_leaf)
		for (i = 0; i < root->num_keys; i++)
			free(root->pointers[i]);
	else
		for (i = 0; i < root->num_keys + 1; i++)
			destroy_tree_nodes(root->pointers[i]);
	free(root->pointers);
	free(root->keys);
	free(root);
}


node * destroy_tree(node * root) {
	destroy_tree_nodes(root);
	return NULL;
}

#endif

#if 0
// MAIN
int main(int argc, char ** argv) {

	char * input_file;
	FILE * fp;
	node * root;
	int input_key, input_key_2;
	char instruction;

	root = NULL;
	verbose_output = false;

	if (argc > 1) {
		order = atoi(argv[1]);
		if (order < MIN_ORDER || order > MAX_ORDER) {
			fprintf(stderr, "Invalid order: %d .\n\n", order);
			usage_3();
			return BT_NORMAL_ERR;
		}
	}

    if (argc < 3) {
        license_notice();
        usage_1();  
        usage_2();
    }

	if (argc > 2) {
		input_file = argv[2];
		fp = fopen(input_file, "r");
		if (fp == NULL) {
			PRINT_E("Failure to open input file.");
			return BT_NORMAL_ERR;
		}
		while (!feof(fp)) {
			fscanf(fp, "%d\n", &input_key);
			root = insert(root, input_key, input_key);
		}
		fclose(fp);
		print_tree(root);
        return BT_NO_ERR;
	}

	PRINT("> ");
    char buffer[BUFFER_SIZE];
    int count = 0;
    bool line_consumed = false;
	while (scanf("%c", &instruction) != EOF) {
        line_consumed = false;
		switch (instruction) {
		case 'd':
			scanf("%d", &input_key);
			root = delete_(root, input_key);
			print_tree(root);
			break;
		case 'i':
            fgets(buffer, BUFFER_SIZE, stdin);
            line_consumed = true;
            count = sscanf(buffer, "%d %d", &input_key, &input_key_2);
            if (count == 1)
              input_key_2 = input_key;
			root = insert(root, input_key, input_key_2);
			print_tree(root);
			break;
		case 'f':
		case 'p':
			scanf("%d", &input_key);
			find_and_print(root, input_key, instruction == 'p');
			break;
		case 'r':
			scanf("%d %d", &input_key, &input_key_2);
			if (input_key > input_key_2) {
				int tmp = input_key_2;
				input_key_2 = input_key;
				input_key = tmp;
			}
			find_and_print_range(root, input_key, input_key_2, instruction == 'p');
			break;
		case 'l':
			print_leaves(root);
			break;
		case 'q':
			while (getchar() != (int)'\n');
			return BT_NO_ERR;
			break;
		case 't':
			print_tree(root);
			break;
		case 'v':
			verbose_output = !verbose_output;
			break;
		case 'x':
			if (root)
				root = destroy_tree(root);
			print_tree(root);
			break;
		default:
			usage_2();
			break;
		}
        if (!line_consumed)
           while (getchar() != (int)'\n');
		PRINT("> ");
	}
	PRINT("\n");

	return BT_NO_ERR;
}
#endif
