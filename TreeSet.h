/* TreeSet
 * =======
 *  Implements a RedBlack Tree to store a wide and potentially sparse bitmap, limited only by range of an unsigned int (assumed to be 32 bits at least) when
 *  using just the CheckBit and SetBit interfaces (so a bitmap with a virtual size of  2^32 bits).  Interfaces are also offered to allow the upper and lower
 *  part of the key index to be used independently, allowing for potentially 32 bits times the maximum bitmap size per node. (ie: the key is the 'upper' 32 bits of
 *  the overall key and the sub_bit_offset is up to the max bitmap size per node). Given the default MAX_BITMAP_PER_NODE of 64, this could be up to 2^(32+64) or 2^96 bits (~79 octillion).
 */

 #define MAX_BITMAP_PER_NODE 64

// #define TESTSET_PROFILE 1 // Enabling this will enable memory and node information dumps. NOT THREAD SAFE.

 struct Tree;
 struct TreeNode;


// Create a tree structure that will contain the bitmap elements. The sizing per node can be manipulated to allow for operations over a subset of the bitmap
// as well as reduce the overall number of nodes used in the tree. IE: If a node can hold 60 bits worth of a bitmap, then a bitmap range of 2,400 bits would take
// 40 nodes. Practically, this will probably be scaled by the smallest 'factor' determining the total range. For example, a bitmap of seconds per hour per day would
// make sense to have 60 bits per node.

struct Tree *CreateTree (unsigned int bitmap_size_per_node);
void DestroyTree(struct Tree *tree);

unsigned int CheckBit (struct Tree *tree, unsigned int total_bit_offset);
void SetBit(struct Tree *tree, unsigned int total_bit_offset, unsigned int value, unsigned int *already_set);

// These Interfaces allow for creation and handling of nodes and their bitmap subsets
// indpendently of the total bitmap range..useful if your entire range is bigger than can be represented in 32 bits.

struct TreeNode *FindNode (struct Tree *tree, int unsigned key);
struct TreeNode *FindOrInsertNode (struct Tree *tree, unsigned int key);
unsigned int CheckSubBit(struct Tree *tree, struct TreeNode *tree_node, unsigned int bit_offset);
unsigned int SetSubBit(struct Tree *tree, struct TreeNode *tree_node, unsigned int bit_offset, unsigned int value, unsigned int *already_set);
void ClearSubBits(struct Tree *tree, struct TreeNode *tree_node);


// Utility to dump the tree.
void PrintTree (struct Tree *tree);

// Print tree statistics
void TreeInfo (struct Tree *tree);

// test interface to run a simple set of functions
void example_test();

// Utility to enable debug output.
void SetTSVerbose (unsigned int enable_disable);

#ifdef TESTSET_PROFILE

// Utility to dump memory usage and allocated nodes of system (not thread safe!)
size_t GetTSMemory();
unsigned int GetTSNodes();

#endif






















