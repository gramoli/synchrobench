#include <iostream>



#define NUM_LEVELS 2
#define NODE_LEVEL 0
#define INODE_LEVEL 1

typedef unsigned long sl_key_t;
typedef void* val_t;
typedef unsigned int uint;

#define VOLATILE volatile



/* bottom-level nodes */
typedef VOLATILE struct sl_node node_t;
struct sl_node {
        sl_key_t key;
	        val_t val;
		        struct sl_node *prev;
			        struct sl_node *next;
				        unsigned int level;
					        unsigned int marker;
						};

						/* index-level nodes */
						typedef VOLATILE struct sl_inode inode_t;
						struct sl_inode {
						        struct sl_inode *right;
							        struct sl_inode *down;
								        struct sl_node  *node;
									};



 main (int argc, char** argv) {


	std::cout << "Node Sizes: ";
	std::cout << "\nDnode: " << sizeof(node_t);
	std::cout << "\nInode: " << sizeof(inode_t);
	std::cout << "\n\n";


}
