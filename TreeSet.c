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

/* Printing a node using a recursive helper function */
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

/* Print tree structure */
void PrintTree (Tree *tree)
{
    if (tree)
    {
        printf ("Tree size:%d perNodeBitsetSize:%d perNodeBitsetByteSize:%d BitsForIdx:%d\n", tree->size, tree->bitmap_size_per_node, tree->bitmap_size_in_bytes, tree->bitmap_idx_size);
        PrintTreeHelper(tree->root, 0, tree, "root");
    }
    else
    {
        printf ("Tree not created.\n");
    }
}


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
        right_depth = TreeInfoHelper(node->left, depth+1);
    }
    return (left_depth + right_depth + depth);
}

/* Print Tree Info per average dept of tree */
void TreeInfo (Tree *tree)
{
   unsigned int running_depth_sum = 0;
   if (tree)
   {
      running_depth_sum = TreeInfoHelper(tree->root, 0);
      if (tree->size > 1)
        printf ("size:%d Avg depth:%f\n", tree->size, (double) running_depth_sum/tree->size);
   }
}

/* Create the base tree structure that will contain all nodes and return a reference to the tree. */
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
        }
    }
    else
    {
        printf("bitmap size of %d is invalid, must be between 0 and 64 bits.\n", bitmap_size_per_node);
    }
    return tree;
}

/* Clean up code to destroy a tree and subtending nodes via recursion */
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
    }
}

/* Internal utilities for RedBlack Tree balancing on a given node in the tre. Note delete is not implemented as bitset may only grow. */
void RightRotate (Tree *t, TreeNode *temp)
{
   TreeNode *left = temp->left;
   temp->left = left->right;
   if (temp->left)
       temp->left->parent = temp;
   left->parent = temp->parent;

   if (!temp->parent)
   {
       t->root = left;
   }
   else if (temp == temp->parent->left)
   {
       temp->parent->left = left;
   }
   else
   {
       temp->parent->right = left;
   }
   left->right = temp;
   temp->parent = left;
}

void LeftRotate (Tree *t, TreeNode *temp)
{
   TreeNode *right = temp->right;
   temp->right = right->left;
   if (temp->right)
       temp->right->parent = temp;
   right->parent = temp->parent;

   if (!temp->parent)
   {
       t->root = right;
   }
   else if (temp == temp->parent->left)
   {
       temp->parent->left = right;
   }
   else
   {
       temp->parent->right = right;
   }
   right->left = temp;
   temp->parent = right;
}

void FixUpTree (Tree *t, TreeNode *pt)
{
   TreeNode *parent = NULL;
   TreeNode *grandparent = NULL;

    verbose_printf (1,"Root = %p\n", t->root);
    while ((pt != t->root) && (pt->RedBlack != BLACK) && (pt->parent->RedBlack == RED))
    {

       parent = pt->parent;
       grandparent = pt->parent->parent;

       if (!parent || !grandparent)
       {
          break;
       }

       verbose_printf (1," pt:%p, parent:%p, grandparent:%p\n", pt, parent, grandparent);
       // Case A: Parent of pt is left child of grand-parent of pt
       if (parent == grandparent->left)
       {
           TreeNode *uncle_pt = grandparent->right;


           if ((uncle_pt != NULL) && (uncle_pt->RedBlack == RED))
           {
               verbose_printf (1," A1 Recolor");
               // Case 1: Uncle is red, only recoloring required
               grandparent->RedBlack = RED;
               parent->RedBlack = BLACK;
               uncle_pt->RedBlack = BLACK;
               pt = grandparent;
           } else {
              // Case 2: Do rotation in the opposite direction of which side of the parent we are on
              if (pt == parent->right)
              {
                 verbose_printf (1," Case A2 ");
                 LeftRotate(t, parent);
                 pt = parent;
                 parent = pt->parent;
              }
              verbose_printf (1," Case A3.");
              //  pt is left child so, do counter-rotate rotate (Case 3 is when we start in this state
              RightRotate(t, grandparent);
              int type = parent->RedBlack;
              parent->RedBlack = grandparent->RedBlack;
              grandparent->RedBlack = type;
              pt = parent;
           }
           verbose_printf (1,"\n");
       }
       // Case B: Parent of pt is right child of grand-parent or pt
       else
       {
          TreeNode *uncle_pt = grandparent->left;


           if ((uncle_pt != NULL) && (uncle_pt->RedBlack == RED))
           {
               verbose_printf (1,"B1 Recolor ");
               // Case 1: Uncle is red, only recoloring required
               grandparent->RedBlack = RED;
               parent->RedBlack = BLACK;
               uncle_pt->RedBlack = BLACK;
               pt = grandparent;
           } else {
              // Case 2: Do rotation in the opposite direction of which side of the parent we are on
              if (pt == parent->left)
              {
                 verbose_printf (1,"B2 ");

                 RightRotate(t, parent);
                 pt = parent;
                 parent = pt->parent;
              }

              verbose_printf (1,"B3 ");

              // pt is left child so, do counter-rotate rotate (Case 3 is when we start in this state)
              LeftRotate(t, grandparent);
              int type = parent->RedBlack;
              parent->RedBlack = grandparent->RedBlack;
              grandparent->RedBlack = type;
              pt = parent;
           }
           verbose_printf (1,"\n");
       }
    }

    //ensure root node is always black, even if rotated.
    if (t->root && (t->root->RedBlack == RED))
    {
        t->root->RedBlack = BLACK;
    }
}

/* Given a valid tree, attempt to lookup a node per the key */
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

/* Given a valid tree, attempt to find or insert the node supplied */

TreeNode *FindOrInsertNodeHelper (TreeNode *node, TreeNode *node_to_insert, TreeNode **nodeFound, int depth)
{
    verbose_printf (1,"Input key is %d, checking %d\n", node_to_insert->key, (node==NULL)?-1:node->key);
    if (node == NULL)
    {
        return node_to_insert;
    }
    else if (node_to_insert->key == node->key)
    {
        verbose_printf (1,"Set node found to %p\n", node);
        *nodeFound = node;
    }
    else if (node_to_insert->key < node->key)
    {
        verbose_printf (1,"Checking node->left\n");
        node->left = FindOrInsertNodeHelper (node->left, node_to_insert, nodeFound, depth+1);
        if (node->left == node_to_insert)
           node_to_insert->parent = node;
        verbose_printf (1,"Node->left set to %p\n", node->left);
    }
    else if (node_to_insert->key > node->key)
    {
        verbose_printf (1,"Checking node->right\n");
        node->right = FindOrInsertNodeHelper (node->right, node_to_insert, nodeFound, depth+1);
        if (node->right == node_to_insert)
           node_to_insert->parent = node;
        verbose_printf (1,"Node->right set to %p\n", node->right);
    }
    return node;
}

TreeNode *FindOrInsertNode (struct Tree *tree, unsigned int key)
{
  TreeNode *found_node = NULL; // This will track if the node is found and return a pointer to it. If it is NULL, this means node created was inserted.
  TreeNode *node_to_insert = NULL;

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
         FindOrInsertNodeHelper (tree->root, node_to_insert, &found_node, 0);
      }
  }

  verbose_printf (1,"found node on insert:%p\n", found_node);
  if (found_node == NULL)
  {
     found_node = node_to_insert;
     tree->size++;
     #ifdef TESTSET_PROFILE
     total_nodes++;
     #endif

     // Check Red/Black balance, if we are deep enough in the tree. As root is black,
     // any child of root is good on insert.
     FixUpTree(tree, node_to_insert);
     if (verbose_enabled)
        PrintTree (tree);
  }
  else
  {
      verbose_printf (1,"Found node:%p\n", found_node);
      if (node_to_insert->payload)
        memory_free(node_to_insert->payload, tree->bitmap_size_in_bytes);
      memory_free(node_to_insert, sizeof(TreeNode));
      node_to_insert = NULL;
  }
  return found_node;
}


/* If a tree node has been found, allow access to the internal bitmap with a bit offset within range of the bitmap within the node */
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

void ClearSubBits(Tree *tree, TreeNode *tree_node)
{
    if (tree && tree_node)
    {
        memset(tree_node->payload, 0, tree->bitmap_size_in_bytes);
    }
}

/* If a tree is valid, allow access to the internal bitmap with a bit offset within total bit offset range (practically (key << bitmap_size_per_node) | sub_bit_offset) */

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
   PrintTree(test_tree);
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
          FindOrInsertNode(test_tree, 1);
          FindOrInsertNode(test_tree, 2);
          FindOrInsertNode(test_tree, 3);
          FindOrInsertNode(test_tree, 5);
          FindOrInsertNode(test_tree, 4);
          FindOrInsertNode(test_tree, 6);
          FindOrInsertNode(test_tree, 7);
          FindOrInsertNode(test_tree, 8);
          FindOrInsertNode(test_tree, 9);
          FindOrInsertNode(test_tree, 10);
          PrintTree(test_tree);
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



#endif
