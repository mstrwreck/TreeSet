#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <direct.h>
#include "TreeSet.h"

#define BLACK 0
#define RED 1

static unsigned int verbose_enabled = 0;

#ifdef TESTSET_PROFILE
static size_t memory_usage = 0;
static unsigned int total_nodes = 0;
static unsigned int total_trees = 0;
#endif

typedef struct TreeNode {
    struct TreeNode *left;
    struct TreeNode *right;
    struct TreeNode *parent;
    unsigned int     key;         // upper N-bitmap_size_per_node of bitmap offset
    unsigned int     RedBlack:1;
    unsigned char   *payload;     //will be dynamically allocated as a byte array big enough to contain bitmap_size_in_bytes in the node.
} TreeNode;

typedef struct Tree {
    int size;
    unsigned int bitmap_size_per_node;
    unsigned int bitmap_size_in_bytes;
    unsigned int bitmap_idx_size;
    TreeNode *root;
} Tree;


/* Utilities*/

static void verbose_printf (int level, const char *format, ...)
{
    if (verbose_enabled >= level)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
    }
};

static void *memory_allocate (size_t size)
{
   void *mem_request = malloc(size);
   #ifdef TESTSET_PROFILE
   if (mem_request)
      memory_usage += size;
   #endif
   return mem_request;
};

static void memory_free (void *memory_to_free, size_t size)
{
   if (memory_to_free)
   {
      #ifdef TESTSET_PROFILE
      memory_usage -= size;
      #endif
      free(memory_to_free);
   }
}


void SetTSVerbose (unsigned int enable_disable)
{
  verbose_enabled = (enable_disable % 2);
}

unsigned int CountBitSize(unsigned int value)
{
   unsigned int count = 0;
   while (value > 0)
   {
       value = value >> 1;
       count++;
   }
   return count;
}

/* PrintTree, PrintTreeHelper - Printing a tree via PrintTree (using PrintTreeHelper recursively) outputs basic tree attributes an in-order print of the tree. */
void PrintTreeHelper (TreeNode *node, int depth, Tree *tree, char *str)
{
    if (node)
    {
        printf (" @depth %d (%s):\n", depth, str);
        PrintTreeHelper(node->left, depth+1, tree, "left");
        printf (" (%d)  node %p: payload key:%04x (%d) color:%s\n (l=%p[%d], r=%p[%d], p=%p[%d])\n", depth, node, node->key, node->key,
                (node->RedBlack == RED)?"RED":"BLACK",
                node->left, (node->left)?node->left->key:-1,
                node->right, (node->right)?node->right->key:-1,
                node->parent, (node->parent)?node->parent->key:-1);
        if (node->payload)
        {
           printf ("   bitmap[%p]:", node->payload);
           for (int idx=tree->bitmap_size_in_bytes-1;idx>=0;idx--)
            printf ("%02x:", node->payload[idx]);
           printf ("\n");
        }

        PrintTreeHelper(node->right, depth+1, tree, "right");
    }

}

void PrintTree (Tree *tree)
{
    if (tree)
    {
        printf ("Tree size:%d perNodeBitsetSize:%d perNodeBitsetByteSize:%d BitsForIdx:%d\n ",
                tree->size, tree->bitmap_size_per_node, tree->bitmap_size_in_bytes, tree->bitmap_idx_size);
        PrintTreeHelper(tree->root, 0, tree, "root");
    }
    else
    {
        printf ("Tree not created.\n");
    }
}

/* TreeInfo, FindMaxDepth, TreeInfoHelper - Investigating a tree via TreeInfo (using the TreeInfoHelper procedure recursively) will output basic tree statistics. */

int TreeInfoHelper(TreeNode *node, unsigned int depth)
{
    int left_depth = 0;
    int right_depth = 0;

    if (node->left)
    {
        left_depth = TreeInfoHelper(node->left, depth+1);
    }

    if (node->right)
    {
        right_depth = TreeInfoHelper(node->right, depth+1);
    }
    return (left_depth + right_depth + depth);
}

/* Finds maximum depth of a tree's left and right sides. */
unsigned int FindMaxDepth (TreeNode *node, unsigned int depth)
{
   if (!node)
   {
       //verbose_printf (1, "@leaf depth:%d\n", depth);
       return (depth);
   }
   else
   {
       unsigned int left_depth = FindMaxDepth(node->left, depth+1);
       unsigned int right_depth = FindMaxDepth(node->right, depth+1);
       verbose_printf (3, "At node %d - left node tree is depth %d, right node tree is depth %d.\n", node->key, left_depth, right_depth);
       if (left_depth > right_depth)
           return left_depth;
       else
           return right_depth;
   }
}

/* Print Tree Info per average depth of tree and node memory allocation if TESTSET_PROFILE is enabled. */
void TreeInfo (Tree *tree)
{
   unsigned int running_depth_sum = 0;
   if (tree)
   {
      running_depth_sum = TreeInfoHelper(tree->root, 0);
      if (tree->size > 0)
      {
             printf ("size:%d left_depth:%d right_depth:%d Avg depth:(%d/%d) = %f\n", tree->size, FindMaxDepth(tree->root->left, 0), FindMaxDepth(tree->root->right, 0), running_depth_sum, tree->size, (double) running_depth_sum/tree->size);
#ifdef TESTSET_PROFILE
             printf ("Tree header size:%I64d TreeNode size: %I64d+%d (node+payload)\n", sizeof(Tree), sizeof(TreeNode), tree->bitmap_size_in_bytes);
#endif
      }
   }
}





/* CreateTree - Create and initialized the base tree structure that will contain all nodes and return a reference to the tree.
 *bitmap_size_per_node is the size of the allocated bitmap window onto the larger virtual bitmap */

struct Tree *CreateTree (unsigned int bitmap_size_per_node)
{
    struct Tree *tree = NULL;

    if (bitmap_size_per_node < MAX_BITMAP_PER_NODE)
    {
        tree = memory_allocate(sizeof(Tree));
        if (tree)
        {
           tree->size = 0;
           tree->bitmap_size_per_node = bitmap_size_per_node;
           tree->bitmap_size_in_bytes = (bitmap_size_per_node + 7) / 8;
           tree->bitmap_idx_size = CountBitSize(bitmap_size_per_node);
           tree->root = NULL;
           #ifdef TESTSET_PROFILE
           total_trees++;
           #endif
        }
    }
    else
    {
        printf("bitmap size of %d is invalid, must be between 0 and %d bits.\n", bitmap_size_per_node, MAX_BITMAP_PER_NODE);
    }
    return tree;
}

/* DestroyNode, DestroyTree - Procedures to destroy tree nodes and tree containers */
void DestroyNode (Tree *tree, TreeNode *tree_node)
{
    if (tree && tree_node)
    {
        if (tree_node->left)
            DestroyNode(tree, tree_node->left);
        if (tree_node->right)
            DestroyNode(tree, tree_node->right);
        if (tree_node->payload != NULL)
            memory_free(tree_node->payload, tree->bitmap_size_in_bytes);
        memory_free(tree_node, sizeof(TreeNode));
        tree->size--;
        #ifdef TESTSET_PROFILE
        total_nodes--;
        #endif
    }
}

void DestroyTree(Tree *tree)
{
    if (tree)
    {
       DestroyNode(tree, tree->root);
       memory_free(tree, sizeof(Tree));
       #ifdef TESTSET_PROFILE
       total_trees--;
       #endif

    }
}

/* RightRotate, LeftRotate, FixUpTree - Internal utilities for RedBlack Tree balancing on a given node in the tre. Note delete is not implemented as bitset may only grow. */
void RightRotate (Tree *t, TreeNode *partial_tree)
{
   TreeNode *left = partial_tree->left;
   partial_tree->left = left->right;
   if (partial_tree->left)
       partial_tree->left->parent = partial_tree;
   left->parent = partial_tree->parent;

   if (!partial_tree->parent)
   {
       t->root = left;
   }
   else if (partial_tree == partial_tree->parent->left)
   {
       partial_tree->parent->left = left;
   }
   else

   {
       partial_tree->parent->right = left;
   }
   left->right = partial_tree;
   partial_tree->parent = left;
}

void LeftRotate (Tree *t, TreeNode *partial_tree)
{
   TreeNode *right = partial_tree->right;
   partial_tree->right = right->left;
   if (partial_tree->right)
       partial_tree->right->parent = partial_tree;
   right->parent = partial_tree->parent;

   if (!partial_tree->parent)
   {
       t->root = right;
   }
   else if (partial_tree == partial_tree->parent->left)
   {
       partial_tree->parent->left = right;
   }
   else
   {
       partial_tree->parent->right = right;
   }
   right->left = partial_tree;
   partial_tree->parent = right;
}

int FixUpTree (Tree *t, TreeNode *partial_tree)
{
   TreeNode *parent = NULL;
   TreeNode *grandparent = NULL;
   int work_done = 0;

    verbose_printf (1,"Root = %p\n", t->root);
    while ((partial_tree != t->root) && (partial_tree->RedBlack != BLACK) && (partial_tree->parent->RedBlack == RED))
    {
       work_done = 1;
       parent = partial_tree->parent;
       grandparent = partial_tree->parent->parent;

       if (!parent || !grandparent)
       {
          break;
       }

       verbose_printf (1," partial_tree:%p[%d], parent:%p[%d], grandparent:%p[%d]\n", partial_tree, (partial_tree!=NULL)?partial_tree->key:-1, parent,
                        (parent!=NULL)?parent->key:-1, grandparent, (grandparent!=NULL)?grandparent->key:-1);
       // Case A: Parent of partial_tree is left child of grand-parent of partial_tree
       if (parent == grandparent->left)
       {
           TreeNode *uncle_partial_tree = grandparent->right;


           if ((uncle_partial_tree != NULL) && (uncle_partial_tree->RedBlack == RED))
           {
               verbose_printf (1," A1 Recolor");
               // Case 1: Uncle is red, only recoloring required
               grandparent->RedBlack = RED;
               parent->RedBlack = BLACK;
               uncle_partial_tree->RedBlack = BLACK;
               partial_tree = grandparent;
           } else {

              // Case 2: Do rotation in the opposite direction of which side of the parent we are on
              if (partial_tree == parent->right)
              {
                 verbose_printf (1," Case A2: Left Rotate ");
                 LeftRotate(t, parent);
                 partial_tree = parent;
                 parent = partial_tree->parent;
              }
              verbose_printf (1," Case A3: Right Rotate.");
              //  partial_tree is left child so, do counter-rotate rotate (Case 3 is when we start in this state
              RightRotate(t, grandparent);

              int type = parent->RedBlack;
              parent->RedBlack = grandparent->RedBlack;
              grandparent->RedBlack = type;

              /*
              parent->RedBlack = BLACK;
              grandparent->RedBlack = RED;
              */
              partial_tree = parent;
           }
           verbose_printf (1,"\n");
       }
       // Case B: Parent of partial_tree is right child of grand-parent or partial_tree
       else
       {
          TreeNode *uncle_partial_tree = grandparent->left;


           if ((uncle_partial_tree != NULL) && (uncle_partial_tree->RedBlack == RED))
           {
               verbose_printf (1,"B1 Recolor ");
               // Case 1: Uncle is red, only recoloring required
               grandparent->RedBlack = RED;
               parent->RedBlack = BLACK;
               uncle_partial_tree->RedBlack = BLACK;
               partial_tree = grandparent;
           } else {

              // Case 2: Do rotation in the opposite direction of which side of the parent we are on
              if (partial_tree == parent->left)
              {
                 verbose_printf (1,"B2 Right Rotate ");

                 RightRotate(t, parent);
                 partial_tree = parent;
                 parent = partial_tree->parent;
              }


              verbose_printf (1,"B3 Left Rotate");

              // partial_tree is left child so, do counter-rotate rotate (Case 3 is when we start in this state)
              LeftRotate(t, grandparent);

              int type = parent->RedBlack;
              parent->RedBlack = grandparent->RedBlack;
              grandparent->RedBlack = type;

              /*
              parent->RedBlack = BLACK;
              grandparent->RedBlack = RED;
              */

              partial_tree = parent;
           }
           verbose_printf (1,"\n");
       }
    }

    //ensure root node is always black after rotations.
    if (t->root && (t->root->RedBlack == RED))
    {
        t->root->RedBlack = BLACK;
    }
    return work_done;
}

/* FindOrInsertNode - Given a valid tree, attempt to find or insert the node reference passed in. The helper will track the found node (if not inserting) in nodeFound (if given),
insert_depth will track the depth the node is inserted at (if variable reference is not NULL), and depth is a helper field to track how far down the tree we go. */

TreeNode *FindNodeHelper (TreeNode *node, int key)
{
    if (node)
    {
       verbose_printf (1,"FindNode: Checking node(%p) with key=%d (looking for %d)\n", node, node->key, key);

       if (node->key == key)
        return node;
       else if (key < node->key)
        return FindNodeHelper (node->left, key);
       else if (key > node->key)
        return FindNodeHelper (node->right, key);
    }
    return NULL;
}

TreeNode *FindNode (Tree *tree, unsigned int key)
{
   return FindNodeHelper(tree->root, key);
}



TreeNode *FindOrInsertNodeHelper (TreeNode *node, TreeNode *node_to_insert, TreeNode **nodeFound, unsigned int *insert_depth, unsigned int depth)
{
    if (node == NULL)
    {
        if (insert_depth)
           *insert_depth = depth-1;
        return node_to_insert;
    }
    else if (node_to_insert->key < node->key)
    {
        node->left = FindOrInsertNodeHelper (node->left, node_to_insert, nodeFound, insert_depth, depth+1);
        if (node->left == node_to_insert)
           node_to_insert->parent = node;
    }
    else if (node_to_insert->key > node->key)
    {
        node->right = FindOrInsertNodeHelper (node->right, node_to_insert, nodeFound, insert_depth, depth+1);
        if (node->right == node_to_insert)
           node_to_insert->parent = node;
    }
    else if ((node_to_insert->key == node->key) && (nodeFound))
    {
        *nodeFound = node;
    }
    return node;
}

TreeNode *FindOrInsertNode (struct Tree *tree, unsigned int key)
{
  TreeNode *found_node = NULL; // This will track if the node is found and return a pointer to it. If it is NULL, this means node created was inserted.
  TreeNode *node_to_insert = NULL;

  unsigned int insert_depth = 0;

  if (tree)
  {
      node_to_insert = memory_allocate(sizeof(TreeNode));
      if (node_to_insert)
      {
         node_to_insert->left = node_to_insert->right = node_to_insert->parent = NULL;
         node_to_insert->RedBlack = RED;
         node_to_insert->key = key;
         if (tree->bitmap_size_in_bytes > 0)
         {
            node_to_insert->payload = memory_allocate(tree->bitmap_size_in_bytes);
            if (!node_to_insert->payload)
            {
                memory_free(node_to_insert, sizeof(TreeNode));
                node_to_insert = NULL;
            }
            else
            {
               memset(node_to_insert->payload, 0, tree->bitmap_size_in_bytes);
            }
         }
         else
         {
            node_to_insert->payload = NULL;
         }
      }

      if (tree->root == NULL)
      {
        node_to_insert->RedBlack = BLACK;
        node_to_insert->parent = NULL;
        tree->root = node_to_insert;
      }
      else
      {
         node_to_insert->RedBlack = RED;
         FindOrInsertNodeHelper (tree->root, node_to_insert, &found_node, &insert_depth, 1);
      }
  }

  if (verbose_enabled >= 3)
  {
     printf ("PreFix Tree (after %d %s at depth %d):\n===========\n", node_to_insert->key, found_node?"found":"inserted", insert_depth);
     PrintTree (tree);
  }

  // If no node found, return node_to_insert as the found node and update tree size.
  if (found_node == NULL)
  {
     found_node = node_to_insert;
     tree->size++;
     #ifdef TESTSET_PROFILE
     total_nodes++;
     #endif

     // Check Red/Black balance, if we are deep enough in the tree. As root is black,
     // any child of root is good on insert.
     int fixed = FixUpTree(tree, node_to_insert);

     if ((verbose_enabled >= 3) && fixed)
     {
        printf ("PostFix Tree:\n============\n");
        PrintTree (tree);
     }
  }
  else
  {
      // this entity was already insert into the tree. Release memory for the insert attempt and return the node found.
      verbose_printf (1,"Found node:%p\n", found_node);
      if (node_to_insert->payload)
        memory_free(node_to_insert->payload, tree->bitmap_size_in_bytes);
      memory_free(node_to_insert, sizeof(TreeNode));
      node_to_insert = NULL;
  }
  return found_node;
}


/* CheckSubBits, SetSubBits - If a tree node has been found, allow access to the internal bitmap with a bit offset within range of the bitmap within the node.
    CheckSubBit expects a bit range from 0 to the size of the bits stored per notde, as dones SetSubBit. already_present will return true of the bit was
    already set before the interface was called. */
unsigned int CheckSubBit(Tree *tree, TreeNode *tree_node, unsigned int sub_bit_offset)
{
    unsigned int return_code = 0;
    if (tree && tree_node && (tree_node->payload) && (sub_bit_offset < tree->bitmap_size_per_node))
    {
        unsigned int set_bit_idx = sub_bit_offset / 8;
        unsigned int set_bit_offset = sub_bit_offset % 8;
        return_code = ((tree_node->payload[set_bit_idx] & (1<<set_bit_offset)) != 0);
    }
    return return_code;
}

unsigned int SetSubBit(Tree * tree, TreeNode *tree_node, unsigned int sub_bit_offset, unsigned int value, unsigned int *already_present)
{
    unsigned int return_code = 0;

    if (already_present)
        *already_present = 0;

    if (tree && tree_node && (tree_node->payload) && (sub_bit_offset < tree->bitmap_size_per_node))
    {
        unsigned int set_bit_idx = sub_bit_offset / 8;
        unsigned int set_bit_offset = sub_bit_offset % 8;
        if ((value % 2) == 1)
        {
           if (already_present)
              *already_present = ((tree_node->payload[set_bit_idx] & (1 << set_bit_offset)) != 0);
           tree_node->payload[set_bit_idx] |= 1 << set_bit_offset;
        }
        else
        {
            tree_node->payload[set_bit_idx] &= ~(1 << set_bit_offset);
        };
        return_code = 1;
    }
    return return_code;
}

/* ClearSubBits - Set all bits from 0 to the number of bits per node to 0 */
void ClearSubBits(Tree *tree, TreeNode *tree_node)
{
    if (tree && tree_node)
    {
        memset(tree_node->payload, 0, tree->bitmap_size_in_bytes);
    }
}

/* CheckBit, SetBit - If a tree is valid, allow access to the internal bitmap with a bit offset within total bit offset range (effectively (key << bitmap_size_per_node) | sub_bit_offset).
 This is the standard access to the TreeSet unless the key range independent of the sub_bit_index plus the sub_bit_range is greater than 32 bits. It uses the CheckSubBit and SetSubBit interfaces
  introduced previously. */

unsigned int CheckBit (Tree *tree, unsigned int total_bit_offset)
{
   unsigned int key = total_bit_offset >> tree->bitmap_idx_size;
   unsigned int sub_bit_offset =  total_bit_offset & ((1<<tree->bitmap_idx_size)-1);
   TreeNode *check_node = FindNode(tree, key);
   if (check_node)
   {
       return CheckSubBit(tree, check_node, sub_bit_offset);
   }
   return 0;
}

void SetBit (Tree *tree, unsigned int total_bit_offset, unsigned int value, unsigned int *already_set)
{
   unsigned int key = total_bit_offset >> tree->bitmap_idx_size;
   unsigned int sub_bit_offset =  total_bit_offset & ((1<<tree->bitmap_idx_size)-1);

   verbose_printf(1, "SetBit: total_bit_offset %d(%04x) => key %d(%04x), sub_bit_offset: %d(%04x)\n", total_bit_offset, total_bit_offset, key, key, sub_bit_offset, sub_bit_offset);
   TreeNode *check_node = FindOrInsertNode(tree, key);
   if (check_node)
   {
       SetSubBit(tree, check_node, sub_bit_offset, value, already_set);
   }
}


// sample usage code

void example_test()
{
   Tree *test_tree = CreateTree(60);
   // PrintTree(test_tree);
   if (test_tree)
   {
          TreeNode *node = FindOrInsertNode(test_tree, 10);
          printf ("Setting bit 2 of node 10...\n");
          SetSubBit(test_tree, node, 2, 1, NULL);
          printf ("Bit 2 is %d, bit 3 is %d\n", CheckSubBit(test_tree, node, 2), CheckSubBit(test_tree, node, 3));
          printf ("Setting bit 3 with value that maps to node (1 << bitsize)+subidx(%d):%d\n", (10<<6)+2, CheckBit(test_tree, (10<<6)+2));
          SetBit (test_tree, (10<<6)+3, 1, NULL);
          printf ("Bit 2 is %d, bit 3 is %d\n", CheckSubBit(test_tree, node, 2), CheckSubBit(test_tree, node, 3));
          printf ("Clearing bit 3 only...\n");
          SetBit (test_tree, (10<<6)+3, 0, NULL);
          printf ("Bit 2 is %d, bit 3 is %d\n", CheckSubBit(test_tree, node, 2), CheckSubBit(test_tree, node, 3));
          ClearSubBits(test_tree, node);
          printf ("After clear Bit 2 is %d, bit 3 is %d\n", CheckSubBit(test_tree, node, 2), CheckSubBit(test_tree, node, 3));
          FindOrInsertNode(test_tree, 9);

          FindOrInsertNode(test_tree, 1);
          FindOrInsertNode(test_tree, 2);
          FindOrInsertNode(test_tree, 3);
          FindOrInsertNode(test_tree, 5);
          FindOrInsertNode(test_tree, 4);
          FindOrInsertNode(test_tree, 6);
          FindOrInsertNode(test_tree, 7);
          FindOrInsertNode(test_tree, 8);

          FindOrInsertNode(test_tree, 10);
          FindOrInsertNode(test_tree, 11);
          FindOrInsertNode(test_tree, 12);
          FindOrInsertNode(test_tree, 13);
          FindOrInsertNode(test_tree, 14);
          FindOrInsertNode(test_tree, 15);
          FindOrInsertNode(test_tree, 16);
          FindOrInsertNode(test_tree, 17);
          FindOrInsertNode(test_tree, 18);
          FindOrInsertNode(test_tree, 19);
          FindOrInsertNode(test_tree, 20);
          FindOrInsertNode(test_tree, 21);
          FindOrInsertNode(test_tree, 22);
          PrintTree(test_tree);
          printf ("\nTreeInfo:\n");
          TreeInfo(test_tree);
   }
}

#ifdef TESTSET_PROFILE

// Profiling node and memory usage - NOT thread safe!

size_t GetTSMemory()
{
  return memory_usage;
}

unsigned int GetTSNodes()
{
    return total_nodes;
}

unsigned int GetTSTrees()
{
    return total_trees;
}



#endif
