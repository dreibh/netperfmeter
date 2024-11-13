/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2025 by Thomas Dreibholz
 * ==========================================================================
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#include <stdlib.h>
#include <stdio.h>

#include "redblacktree.h"
#include "debug.h"


#ifdef __cplusplus
extern "C" {
#endif


static struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeInternalFindPrev)(
                                                  const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                                  const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode);
static struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeInternalFindNext)(
                                                  const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                                  const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode);


/* ###### Initialize ##################################################### */
void RB_FUNCTION(RedBlackTreeNodeNew)(
        struct RB_DEFINITION(RedBlackTreeNode)* node)
{
#ifdef USE_LEAFLINKED
   doubleLinkedRingListNodeNew(&node->ListNode);
#endif
   node->Parent       = NULL;
   node->LeftSubtree  = NULL;
   node->RightSubtree = NULL;
   node->Color        = Black;
   node->Value        = 0;
   node->ValueSum     = 0;
}


/* ###### Invalidate ##################################################### */
void RB_FUNCTION(RedBlackTreeNodeDelete)(
        struct RB_DEFINITION(RedBlackTreeNode)* node)
{
   node->Parent       = NULL;
   node->LeftSubtree  = NULL;
   node->RightSubtree = NULL;
   node->Color        = Black;
   node->Value        = 0;
   node->ValueSum     = 0;
#ifdef USE_LEAFLINKED
   doubleLinkedRingListNodeDelete(&node->ListNode);
#endif
}


/* ###### Is node linked? ################################################ */
int RB_FUNCTION(RedBlackTreeNodeIsLinked)(
       const struct RB_DEFINITION(RedBlackTreeNode)* node)
{
   return node->LeftSubtree != NULL;
}


/* ##### Initialize ###################################################### */
void RB_FUNCTION(RedBlackTreeNew)(
        struct RB_DEFINITION(RedBlackTree)* rbt,
        void                                (*printFunction)(const void* node, FILE* fd),
        int                                 (*comparisonFunction)(const void* node1, const void* node2))
{
#ifdef USE_LEAFLINKED
   doubleLinkedRingListNew(&rbt->List);
#endif
   rbt->PrintFunction         = printFunction;
   rbt->ComparisonFunction    = comparisonFunction;
   rbt->NullNode.Parent       = &rbt->NullNode;
   rbt->NullNode.LeftSubtree  = &rbt->NullNode;
   rbt->NullNode.RightSubtree = &rbt->NullNode;
   rbt->NullNode.Color        = Black;
   rbt->NullNode.Value        = 0;
   rbt->NullNode.ValueSum     = 0;
   rbt->Elements              = 0;
}


/* ##### Invalidate ###################################################### */
void RB_FUNCTION(RedBlackTreeDelete)(
        struct RB_DEFINITION(RedBlackTree)* rbt)
{
   rbt->Elements              = 0;
   rbt->NullNode.Parent       = NULL;
   rbt->NullNode.LeftSubtree  = NULL;
   rbt->NullNode.RightSubtree = NULL;
#ifdef USE_LEAFLINKED
   doubleLinkedRingListDelete(&rbt->List);
#endif
}


/* ##### Update value sum ################################################ */
inline static void RB_FUNCTION(RedBlackTreeUpdateValueSum)(
                 struct RB_DEFINITION(RedBlackTreeNode)* node)
{
   node->ValueSum = node->LeftSubtree->ValueSum + node->Value + node->RightSubtree->ValueSum;
}


/* ##### Update value sum for node and all parents up to tree root ####### */
static void RB_FUNCTION(RedBlackTreeUpdateValueSumsUpToRoot)(
               struct RB_DEFINITION(RedBlackTree)*     rbt,
               struct RB_DEFINITION(RedBlackTreeNode)* node)
{
   while(node != &rbt->NullNode) {
       RB_FUNCTION(RedBlackTreeUpdateValueSum)(node);
       node = node->Parent;
   }
}


/* ###### Internal method for printing a node ############################# */
static void RB_FUNCTION(RedBlackTreePrintNode)(
               const struct RB_DEFINITION(RedBlackTree)*     rbt,
               const struct RB_DEFINITION(RedBlackTreeNode)* node,
               FILE*                                         fd)
{
   rbt->PrintFunction(node, fd);
#ifdef DEBUG
   fprintf(fd, " ptr=%p c=%s v=%llu vsum=%llu",
           node, ((node->Color == Red) ? "Red" : "Black"),
           node->Value, node->ValueSum);
   if(node->LeftSubtree != &rbt->NullNode) {
      fprintf(fd, " l=%p[", node->LeftSubtree);
      rbt->PrintFunction(node->LeftSubtree, fd);
      fprintf(fd, "]");
   }
   else {
      fprintf(fd, " l=()");
   }
   if(node->RightSubtree != &rbt->NullNode) {
      fprintf(fd, " r=%p[", node->RightSubtree);
      rbt->PrintFunction(node->RightSubtree, fd);
      fprintf(fd, "]");
   }
   else {
      fprintf(fd, " r=()");
   }
   if(node->Parent != &rbt->NullNode) {
      fprintf(fd, " p=%p[", node->Parent);
      rbt->PrintFunction(node->Parent, fd);
      fprintf(fd, "]   ");
   }
   else {
      fprintf(fd, " p=())   ");
   }
   fputs("\n", fd);
#endif
}


/* ##### Internal printing function ###################################### */
void RB_FUNCTION(RedBlackTreeInternalPrint)(
        const struct RB_DEFINITION(RedBlackTree)*     rbt,
        const struct RB_DEFINITION(RedBlackTreeNode)* node,
        FILE*                                         fd)
{
   if(node != &rbt->NullNode) {
      RB_FUNCTION(RedBlackTreeInternalPrint)(rbt, node->LeftSubtree, fd);
      RB_FUNCTION(RedBlackTreePrintNode)(rbt, node, fd);
      RB_FUNCTION(RedBlackTreeInternalPrint)(rbt, node->RightSubtree, fd);
   }
}


/* ###### Print tree ##################################################### */
void RB_FUNCTION(RedBlackTreePrint)(
        const struct RB_DEFINITION(RedBlackTree)* rbt,
        FILE*                                     fd)
{
#ifdef DEBUG
   fprintf(fd, "\n\nroot=%p[", rbt->NullNode.LeftSubtree);
   if(rbt->NullNode.LeftSubtree != &rbt->NullNode) {
      rbt->PrintFunction(rbt->NullNode.LeftSubtree, fd);
   }
   fprintf(fd, "] null=%p   \n", &rbt->NullNode);
#endif
   RB_FUNCTION(RedBlackTreeInternalPrint)(rbt, rbt->NullNode.LeftSubtree, fd);
   fputs("\n", fd);
}


/* ###### Is tree empty? ################################################# */
int RB_FUNCTION(RedBlackTreeIsEmpty)(
       const struct RB_DEFINITION(RedBlackTree)* rbt)
{
   return rbt->NullNode.LeftSubtree == &rbt->NullNode;
}


/* ###### Get first node ################################################## */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetFirst)(
                                           const struct RB_DEFINITION(RedBlackTree)* rbt)
{
#ifdef USE_LEAFLINKED
   struct DoubleLinkedRingListNode* node = rbt->List.Node.Next;
   if(node != rbt->List.Head) {
      return (struct RB_DEFINITION(RedBlackTreeNode)*)node;
   }
   return NULL;
#else
   const struct RB_DEFINITION(RedBlackTreeNode)* node = rbt->NullNode.LeftSubtree;
   if(node == &rbt->NullNode) {
      node = rbt->NullNode.RightSubtree;
   }
   while(node->LeftSubtree != &rbt->NullNode) {
      node = node->LeftSubtree;
   }
   if(node != &rbt->NullNode) {
      return (struct RB_DEFINITION(RedBlackTreeNode)*)node;
   }
   return NULL;
#endif
}


/* ###### Get last node ################################################### */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetLast)(
                                           const struct RB_DEFINITION(RedBlackTree)* rbt)
{
#ifdef USE_LEAFLINKED
   struct DoubleLinkedRingListNode* node = rbt->List.Node.Prev;
   if(node != rbt->List.Head) {
      return (struct RB_DEFINITION(RedBlackTreeNode)*)node;
   }
   return NULL;
#else
   const struct RB_DEFINITION(RedBlackTreeNode)* node = rbt->NullNode.RightSubtree;
   if(node == &rbt->NullNode) {
      node = rbt->NullNode.LeftSubtree;
   }
   while(node->RightSubtree != &rbt->NullNode) {
      node = node->RightSubtree;
   }
   if(node != &rbt->NullNode) {
      return (struct RB_DEFINITION(RedBlackTreeNode)*)node;
   }
   return NULL;
#endif
}


/* ###### Get previous node ############################################### */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetPrev)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* node)
{
#ifdef USE_LEAFLINKED
   struct DoubleLinkedRingListNode* prev = node->ListNode.Prev;
   if(prev != rbt->List.Head) {
      return (struct RB_DEFINITION(RedBlackTreeNode)*)prev;
   }
   return NULL;
#else
   struct RB_DEFINITION(RedBlackTreeNode)* result;
   result = RB_FUNCTION(RedBlackTreeInternalFindPrev)(rbt, node);
   if(result != &rbt->NullNode) {
      return result;
   }
   return NULL;
#endif
}


/* ###### Get next node ################################################## */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetNext)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* node)
{
#ifdef USE_LEAFLINKED
   struct DoubleLinkedRingListNode* next = node->ListNode.Next;
   if(next != rbt->List.Head) {
      return (struct RB_DEFINITION(RedBlackTreeNode)*)next;
   }
   return NULL;
#else
   struct RB_DEFINITION(RedBlackTreeNode)* result;
   result = RB_FUNCTION(RedBlackTreeInternalFindNext)(rbt, node);
   if(result != &rbt->NullNode) {
      return result;
   }
   return NULL;
#endif
}


/* ###### Find nearest previous node ##################################### */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetNearestPrev)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode)
{
   struct RB_DEFINITION(RedBlackTreeNode)*const* nodePtr;
   struct RB_DEFINITION(RedBlackTreeNode)*const* parentPtr;
   const struct RB_DEFINITION(RedBlackTreeNode)* node;
   const struct RB_DEFINITION(RedBlackTreeNode)* parent;
   int                                           cmpResult = 0;

#ifdef DEBUG
   printf("nearest prev: ");
   rbt->PrintFunction(cmpNode, stdout);
   printf("\n");
   RB_FUNCTION(RedBlackTreePrint)(rbt, stdout);
#endif

   parentPtr = NULL;
   nodePtr   = &rbt->NullNode.LeftSubtree;
   while(*nodePtr != &rbt->NullNode) {
      cmpResult = rbt->ComparisonFunction(cmpNode, *nodePtr);
      if(cmpResult < 0) {
         parentPtr = nodePtr;
         nodePtr   = &(*nodePtr)->LeftSubtree;
      }
      else if(cmpResult > 0) {
         parentPtr = nodePtr;
         nodePtr   = &(*nodePtr)->RightSubtree;
      }
      if(cmpResult == 0) {
         return RB_FUNCTION(RedBlackTreeGetPrev)(rbt, *nodePtr);
      }
   }

   if(parentPtr == NULL) {
      if(cmpResult > 0) {
         return rbt->NullNode.LeftSubtree;
      }
      return NULL;
   }
   else {
      /* The new node would be the right child of its parent.
         => The parent is the nearest previous node! */
      if(nodePtr == &(*parentPtr)->RightSubtree) {
         return *parentPtr;
      }
      else {
         parent = *parentPtr;

         /* If there is a left subtree, the nearest previous node is the
            rightmost child of the left subtree. */
         if(parent->LeftSubtree != &rbt->NullNode) {
            node = parent->LeftSubtree;
            while(node->RightSubtree != &rbt->NullNode) {
               node = node->RightSubtree;
            }
            if(node != &rbt->NullNode) {
               return (struct RB_DEFINITION(RedBlackTreeNode)*)node;
            }
         }

         /* If there is no left subtree, the nearest previous node is an
            ancestor node which has the node on its right side. */
         else {
            node   = parent;
            parent = node->Parent;
            while((parent != &rbt->NullNode) && (node == parent->LeftSubtree)) {
               node   = parent;
               parent = parent->Parent;
            }
            if(parent != &rbt->NullNode) {
               return (struct RB_DEFINITION(RedBlackTreeNode)*)parent;
            }
         }
      }
   }
   return NULL;
}


/* ###### Find nearest next node ######################################### */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetNearestNext)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode)
{
   struct RB_DEFINITION(RedBlackTreeNode)*const* nodePtr;
   struct RB_DEFINITION(RedBlackTreeNode)*const* parentPtr;
   const struct RB_DEFINITION(RedBlackTreeNode)* node;
   const struct RB_DEFINITION(RedBlackTreeNode)* parent;
   int                                           cmpResult = 0;

#ifdef DEBUG
   printf("nearest next: ");
   rbt->PrintFunction(cmpNode, stdout);
   printf("\n");
   RB_FUNCTION(RedBlackTreePrint)(bt, stdout);
#endif

   parentPtr = NULL;
   nodePtr   = &rbt->NullNode.LeftSubtree;
   while(*nodePtr != &rbt->NullNode) {
      cmpResult = rbt->ComparisonFunction(cmpNode, *nodePtr);
      if(cmpResult < 0) {
         parentPtr = nodePtr;
         nodePtr   = &(*nodePtr)->LeftSubtree;
      }
      else if(cmpResult > 0) {
         parentPtr = nodePtr;
         nodePtr   = &(*nodePtr)->RightSubtree;
      }
      if(cmpResult == 0) {
         return RB_FUNCTION(RedBlackTreeGetNext)(rbt, *nodePtr);
      }
   }

   if(parentPtr == NULL) {
      if(cmpResult < 0) {
         return rbt->NullNode.LeftSubtree;
      }
      return NULL;
   }
   else {
      /* The new node would be the left child of its parent.
         => The parent is the nearest next node! */
      if(nodePtr == &(*parentPtr)->LeftSubtree) {
         return *parentPtr;
      }
      else {
         parent = *parentPtr;

         /* If there is a right subtree, the nearest next node is the
            leftmost child of the right subtree. */
         if(parent->RightSubtree != &rbt->NullNode) {
            node = parent->RightSubtree;
            while(node->LeftSubtree != &rbt->NullNode) {
               node = node->LeftSubtree;
            }
            if(node != &rbt->NullNode) {
               return (struct RB_DEFINITION(RedBlackTreeNode)*)node;
            }
         }

         /* If there is no right subtree, the nearest next node is an
            ancestor node which has the node on its left side. */
         else {
            node   = parent;
            parent = node->Parent;
            while((parent != &rbt->NullNode) && (node == parent->RightSubtree)) {
               node   = parent;
               parent = parent->Parent;
            }
            if(parent != &rbt->NullNode) {
               return (struct RB_DEFINITION(RedBlackTreeNode)*)parent;
            }
         }
      }
   }
   return NULL;
}


/* ###### Get number of elements ########################################## */
size_t RB_FUNCTION(RedBlackTreeGetElements)(
          const struct RB_DEFINITION(RedBlackTree)* rbt)
{
   return rbt->Elements;
}


/* ###### Get prev node by walking through the tree (does *not* use list!) */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeInternalFindPrev)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode)
{
   const struct RB_DEFINITION(RedBlackTreeNode)* node = cmpNode->LeftSubtree;
   const struct RB_DEFINITION(RedBlackTreeNode)* parent;

   if(node != &rbt->NullNode) {
      while(node->RightSubtree != &rbt->NullNode) {
         node = node->RightSubtree;
      }
      return (struct RB_DEFINITION(RedBlackTreeNode)*)node;
   }
   else {
      node   = cmpNode;
      parent = cmpNode->Parent;
      while((parent != &rbt->NullNode) && (node == parent->LeftSubtree)) {
         node   = parent;
         parent = parent->Parent;
      }
      return (struct RB_DEFINITION(RedBlackTreeNode)*)parent;
   }
}


/* ###### Get next node by walking through the tree (does *not* use list!) */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeInternalFindNext)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode)
{
   const struct RB_DEFINITION(RedBlackTreeNode)* node = cmpNode->RightSubtree;
   const struct RB_DEFINITION(RedBlackTreeNode)* parent;

   if(node != &rbt->NullNode) {
      while(node->LeftSubtree != &rbt->NullNode) {
         node = node->LeftSubtree;
      }
      return (struct RB_DEFINITION(RedBlackTreeNode)*)node;
   }
   else {
      node   = cmpNode;
      parent = cmpNode->Parent;
      while((parent != &rbt->NullNode) && (node == parent->RightSubtree)) {
         node   = parent;
         parent = parent->Parent;
      }
      return (struct RB_DEFINITION(RedBlackTreeNode)*)parent;
   }
}


/* ###### Find node ####################################################### */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeFind)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode)
{
#ifdef DEBUG
   printf("find: ");
   rbt->PrintFunction(cmpNode, stdout);
   printf("\n");
#endif

   struct RB_DEFINITION(RedBlackTreeNode)* node = rbt->NullNode.LeftSubtree;
   while(node != &rbt->NullNode) {
      const int cmpResult = rbt->ComparisonFunction(cmpNode, node);
      if(cmpResult == 0) {
         return node;
      }
      else if(cmpResult < 0) {
         node = node->LeftSubtree;
      }
      else {
         node = node->RightSubtree;
      }
   }
   return NULL;
}


/* ###### Get value sum from root node ################################### */
RedBlackTreeNodeValueType RB_FUNCTION(RedBlackTreeGetValueSum)(
                             const struct RB_DEFINITION(RedBlackTree)* rbt)
{
   return rbt->NullNode.LeftSubtree->ValueSum;
}


/* ##### Rotation with left subtree ###################################### */
static void RB_FUNCTION(RedBlackTreeRotateLeft)(
               struct RB_DEFINITION(RedBlackTreeNode)* node)
{
   struct RB_DEFINITION(RedBlackTreeNode)* lower;
   struct RB_DEFINITION(RedBlackTreeNode)* lowleft;
   struct RB_DEFINITION(RedBlackTreeNode)* upparent;

   lower = node->RightSubtree;
   node->RightSubtree = lowleft = lower->LeftSubtree;
   lowleft->Parent = node;
   lower->Parent = upparent = node->Parent;

   if(node == upparent->LeftSubtree) {
      upparent->LeftSubtree = lower;
   } else {
      CHECK (node == upparent->RightSubtree);
      upparent->RightSubtree = lower;
   }

   lower->LeftSubtree = node;
   node->Parent = lower;

   RB_FUNCTION(RedBlackTreeUpdateValueSum)(node);
   RB_FUNCTION(RedBlackTreeUpdateValueSum)(node->Parent);
}


/* ##### Rotation with ripht subtree ##################################### */
static void RB_FUNCTION(RedBlackTreeRotateRight)(
               struct RB_DEFINITION(RedBlackTreeNode)* node)
{
   struct RB_DEFINITION(RedBlackTreeNode)* lower;
   struct RB_DEFINITION(RedBlackTreeNode)* lowright;
   struct RB_DEFINITION(RedBlackTreeNode)* upparent;

   lower = node->LeftSubtree;
   node->LeftSubtree = lowright = lower->RightSubtree;
   lowright->Parent = node;
   lower->Parent = upparent = node->Parent;

   if(node == upparent->RightSubtree) {
      upparent->RightSubtree = lower;
   } else {
      CHECK(node == upparent->LeftSubtree);
      upparent->LeftSubtree = lower;
   }

   lower->RightSubtree = node;
   node->Parent = lower;

   RB_FUNCTION(RedBlackTreeUpdateValueSum)(node);
   RB_FUNCTION(RedBlackTreeUpdateValueSum)(node->Parent);
}


/* ###### Insert ######################################################### */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeInsert)(
                                           struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           struct RB_DEFINITION(RedBlackTreeNode)* node)
{
   int                                     cmpResult = -1;
   struct RB_DEFINITION(RedBlackTreeNode)* where     = rbt->NullNode.LeftSubtree;
   struct RB_DEFINITION(RedBlackTreeNode)* parent    = &rbt->NullNode;
   struct RB_DEFINITION(RedBlackTreeNode)* result;
   struct RB_DEFINITION(RedBlackTreeNode)* uncle;
   struct RB_DEFINITION(RedBlackTreeNode)* grandpa;
#ifdef USE_LEAFLINKED
   struct RB_DEFINITION(RedBlackTreeNode)* prev;
#endif
#ifdef DEBUG
   printf("insert: ");
   rbt->PrintFunction(node, stdout);
   printf("\n");
#endif


   /* ====== Find location of new node =================================== */
   while(where != &rbt->NullNode) {
      parent = where;
      cmpResult = rbt->ComparisonFunction(node, where);
      if(cmpResult < 0) {
         where = where->LeftSubtree;
      }
      else if(cmpResult > 0) {
         where = where->RightSubtree;
      }
      else {
         /* Node with same key is already available -> return. */
         result = where;
         goto finished;
      }
   }
   CHECK(where == &rbt->NullNode);

   if(cmpResult < 0) {
      parent->LeftSubtree = node;
   }
   else {
      parent->RightSubtree = node;
   }


   /* ====== Link node =================================================== */
   node->Parent       = parent;
   node->LeftSubtree  = &rbt->NullNode;
   node->RightSubtree = &rbt->NullNode;
   node->ValueSum     = node->Value;
#ifdef USE_LEAFLINKED
   prev = RB_FUNCTION(RedBlackTreeInternalFindPrev)(rbt, node);
   if(prev != &rbt->NullNode) {
      doubleLinkedRingListAddAfter(&prev->ListNode, &node->ListNode);
   }
   else {
      doubleLinkedRingListAddHead(&rbt->List, &node->ListNode);
   }
#endif
   rbt->Elements++;
   result = node;


   /* ====== Update parent's value sum =================================== */
   RB_FUNCTION(RedBlackTreeUpdateValueSumsUpToRoot)(rbt, node->Parent);


   /* ====== Ensure red-black tree properties ============================ */
   node->Color = Red;
   while (parent->Color == Red) {
      grandpa = parent->Parent;
      if(parent == grandpa->LeftSubtree) {
         uncle = grandpa->RightSubtree;
         if(uncle->Color == Red) {
            parent->Color  = Black;
            uncle->Color   = Black;
            grandpa->Color = Red;
            node           = grandpa;
            parent         = grandpa->Parent;
         } else {
            if(node == parent->RightSubtree) {
               RB_FUNCTION(RedBlackTreeRotateLeft)(parent);
               parent = node;
               CHECK(grandpa == parent->Parent);
            }
            parent->Color  = Black;
            grandpa->Color = Red;
            RB_FUNCTION(RedBlackTreeRotateRight)(grandpa);
            break;
         }
      } else {
         uncle = grandpa->LeftSubtree;
         if(uncle->Color == Red) {
            parent->Color  = Black;
            uncle->Color   = Black;
            grandpa->Color = Red;
            node           = grandpa;
            parent         = grandpa->Parent;
         } else {
            if(node == parent->LeftSubtree) {
               RB_FUNCTION(RedBlackTreeRotateRight)(parent);
               parent = node;
               CHECK(grandpa == parent->Parent);
            }
            parent->Color  = Black;
            grandpa->Color = Red;
            RB_FUNCTION(RedBlackTreeRotateLeft)(grandpa);
            break;
         }
      }
   }
   rbt->NullNode.LeftSubtree->Color = Black;


finished:
#ifdef DEBUG
   RB_FUNCTION(RedBlackTreePrint)(rbt, stdout);
#endif
#ifdef VERIFY
   RB_FUNCTION(RedBlackTreeVerify)(rbt);
#endif
   return result;
}


/* ###### Remove ######################################################### */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeRemove)(
                                           struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           struct RB_DEFINITION(RedBlackTreeNode)* node)
{
   struct RB_DEFINITION(RedBlackTreeNode)* child;
   struct RB_DEFINITION(RedBlackTreeNode)* delParent;
   struct RB_DEFINITION(RedBlackTreeNode)* parent;
   struct RB_DEFINITION(RedBlackTreeNode)* sister;
   struct RB_DEFINITION(RedBlackTreeNode)* next;
   struct RB_DEFINITION(RedBlackTreeNode)* nextParent;
   enum RedBlackTreeNodeColorType          nextColor;
#ifdef DEBUG
   printf("remove: ");
   rbt->PrintFunction(node, stdout);
   printf("\n");
#endif

   CHECK(RB_FUNCTION(RedBlackTreeNodeIsLinked)(node));

   /* ====== Unlink node ================================================= */
   if((node->LeftSubtree != &rbt->NullNode) && (node->RightSubtree != &rbt->NullNode)) {
      next       = RB_FUNCTION(RedBlackTreeGetNext)(rbt, node);
      nextParent = next->Parent;
      nextColor  = next->Color;

      CHECK(next != &rbt->NullNode);
      CHECK(next->Parent != &rbt->NullNode);
      CHECK(next->LeftSubtree == &rbt->NullNode);

      child         = next->RightSubtree;
      child->Parent = nextParent;
      if(nextParent->LeftSubtree == next) {
         nextParent->LeftSubtree = child;
      } else {
         CHECK(nextParent->RightSubtree == next);
         nextParent->RightSubtree = child;
      }


      delParent                  = node->Parent;
      next->Parent               = delParent;
      next->LeftSubtree          = node->LeftSubtree;
      next->RightSubtree         = node->RightSubtree;
      next->LeftSubtree->Parent  = next;
      next->RightSubtree->Parent = next;
      next->Color                = node->Color;
      node->Color                = nextColor;

      if(delParent->LeftSubtree == node) {
         delParent->LeftSubtree = next;
      } else {
         CHECK(delParent->RightSubtree == node);
         delParent->RightSubtree = next;
      }

      /* ====== Update parent's value sum ================================ */
      RB_FUNCTION(RedBlackTreeUpdateValueSumsUpToRoot)(rbt, next);
      RB_FUNCTION(RedBlackTreeUpdateValueSumsUpToRoot)(rbt, nextParent);
   } else {
      CHECK(node != &rbt->NullNode);
      CHECK((node->LeftSubtree == &rbt->NullNode) || (node->RightSubtree == &rbt->NullNode));

      child         = (node->LeftSubtree != &rbt->NullNode) ? node->LeftSubtree : node->RightSubtree;
      child->Parent = delParent = node->Parent;

      if(node == delParent->LeftSubtree) {
         delParent->LeftSubtree = child;
      } else {
         CHECK(node == delParent->RightSubtree);
         delParent->RightSubtree = child;
      }

      /* ====== Update parent's value sum ================================ */
      RB_FUNCTION(RedBlackTreeUpdateValueSumsUpToRoot)(rbt, delParent);
   }


   /* ====== Unlink node from list and invalidate pointers =============== */
   node->Parent       = NULL;
   node->RightSubtree = NULL;
   node->LeftSubtree  = NULL;
#ifdef USE_LEAFLINKED
   doubleLinkedRingListRemNode(&node->ListNode);
   node->ListNode.Prev = NULL;
   node->ListNode.Next = NULL;
#endif
   CHECK(rbt->Elements > 0);
   rbt->Elements--;


   /* ====== Ensure red-black properties ================================= */
   if(node->Color == Black) {
      rbt->NullNode.LeftSubtree->Color = Red;

      while (child->Color == Black) {
         parent = child->Parent;
         if(child == parent->LeftSubtree) {
            sister = parent->RightSubtree;
            CHECK(sister != &rbt->NullNode);
            if(sister->Color == Red) {
               sister->Color = Black;
               parent->Color = Red;
               RB_FUNCTION(RedBlackTreeRotateLeft)(parent);
               sister = parent->RightSubtree;
               CHECK(sister != &rbt->NullNode);
            }
            if((sister->LeftSubtree->Color == Black) &&
               (sister->RightSubtree->Color == Black)) {
               sister->Color = Red;
               child = parent;
            } else {
               if(sister->RightSubtree->Color == Black) {
                  CHECK(sister->LeftSubtree->Color == Red);
                  sister->LeftSubtree->Color = Black;
                  sister->Color = Red;
                  RB_FUNCTION(RedBlackTreeRotateRight)(sister);
                  sister = parent->RightSubtree;
                  CHECK(sister != &rbt->NullNode);
               }
               sister->Color = parent->Color;
               sister->RightSubtree->Color = Black;
               parent->Color = Black;
               RB_FUNCTION(RedBlackTreeRotateLeft)(parent);
               break;
            }
         } else {
            CHECK(child == parent->RightSubtree);
            sister = parent->LeftSubtree;
            CHECK(sister != &rbt->NullNode);
            if(sister->Color == Red) {
               sister->Color = Black;
               parent->Color = Red;
               RB_FUNCTION(RedBlackTreeRotateRight)(parent);
               sister = parent->LeftSubtree;
               CHECK(sister != &rbt->NullNode);
            }
            if((sister->RightSubtree->Color == Black) &&
               (sister->LeftSubtree->Color == Black)) {
               sister->Color = Red;
               child = parent;
            } else {
               if(sister->LeftSubtree->Color == Black) {
                  CHECK(sister->RightSubtree->Color == Red);
                  sister->RightSubtree->Color = Black;
                  sister->Color = Red;
                  RB_FUNCTION(RedBlackTreeRotateLeft)(sister);
                  sister = parent->LeftSubtree;
                  CHECK(sister != &rbt->NullNode);
               }
               sister->Color = parent->Color;
               sister->LeftSubtree->Color = Black;
               parent->Color = Black;
               RB_FUNCTION(RedBlackTreeRotateRight)(parent);
               break;
            }
         }
      }
      child->Color = Black;
      rbt->NullNode.LeftSubtree->Color = Black;
   }


#ifdef DEBUG
    RB_FUNCTION(RedBlackTreePrint)(rbt, stdout);
#endif
#ifdef VERIFY
    RB_FUNCTION(RedBlackTreeVerify)(rbt);
#endif
   return node;
}


/* ##### Get node by value ############################################### */
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetNodeByValue)(
                                           const struct RB_DEFINITION(RedBlackTree)* rbt,
                                           RedBlackTreeNodeValueType                 value)
{
   const struct RB_DEFINITION(RedBlackTreeNode)* node = rbt->NullNode.LeftSubtree;
   for(;;) {
      if(value < node->LeftSubtree->ValueSum) {
         if(node->LeftSubtree != &rbt->NullNode) {
            node = node->LeftSubtree;
         }
         else {
            break;
         }
      }
      else if(value < node->LeftSubtree->ValueSum + node->Value) {
         break;
      }
      else {
         if(node->RightSubtree != &rbt->NullNode) {
            value -= node->LeftSubtree->ValueSum + node->Value;
            node = node->RightSubtree;
         }
         else {
            break;
         }
      }
   }

   if(node !=  &rbt->NullNode) {
      return (struct RB_DEFINITION(RedBlackTreeNode)*)node;
   }
   return NULL;
}


/* ##### Internal verification function ################################## */
static size_t RB_FUNCTION(RedBlackTreeInternalVerify)(
                 struct RB_DEFINITION(RedBlackTree)*      rbt,
                 struct RB_DEFINITION(RedBlackTreeNode)*  parent,
                 struct RB_DEFINITION(RedBlackTreeNode)*  node,
                 struct RB_DEFINITION(RedBlackTreeNode)** lastRedBlackTreeNode,
#ifdef USE_LEAFLINKED
                 struct DoubleLinkedRingListNode**        lastListNode,
#endif
                 size_t*                                  counter)
{
#ifdef USE_LEAFLINKED
   struct RB_DEFINITION(RedBlackTreeNode)* prev;
   struct RB_DEFINITION(RedBlackTreeNode)* next;
#endif
   size_t                                  leftHeight;
   size_t                                  rightHeight;

   if(node != &rbt->NullNode) {
      /* ====== Print node =============================================== */
#ifdef DEBUG
      printf("verifying ");
      RB_FUNCTION(RedBlackTreePrintNode)(rbt, node, stdout);
      puts("");
#endif

      /* ====== Correct parent? ========================================== */
      CHECK(node->Parent == parent);

      /* ====== Correct tree and heap properties? ======================== */
      if(node->LeftSubtree != &rbt->NullNode) {
         CHECK(rbt->ComparisonFunction(node, node->LeftSubtree) > 0);
      }
      if(node->RightSubtree != &rbt->NullNode) {
         CHECK(rbt->ComparisonFunction(node, node->RightSubtree) < 0);
      }

      /* ====== Is value sum okay? ======================================= */
      CHECK(node->ValueSum == node->LeftSubtree->ValueSum +
                              node->Value +
                              node->RightSubtree->ValueSum);

      /* ====== Is left subtree okay? ==================================== */
      leftHeight = RB_FUNCTION(RedBlackTreeInternalVerify)(
                      rbt, node, node->LeftSubtree, lastRedBlackTreeNode,
#ifdef USE_LEAFLINKED
                      lastListNode,
#endif
                      counter);

#ifdef USE_LEAFLINKED
      /* ====== Is ring list okay? ======================================= */
      CHECK((*lastListNode)->Next != rbt->List.Head);
      *lastListNode = (*lastListNode)->Next;
      CHECK(*lastListNode == &node->ListNode);
#endif

#ifdef USE_LEAFLINKED
      /* ====== Is linking working correctly? ============================ */
      prev = RB_FUNCTION(RedBlackTreeInternalFindPrev)(rbt, node);
      if(prev != &rbt->NullNode) {
         CHECK((*lastListNode)->Prev == &prev->ListNode);
      }
      else {
         CHECK((*lastListNode)->Prev == rbt->List.Head);
      }

      next = RB_FUNCTION(RedBlackTreeInternalFindNext)(rbt, node);
      if(next != &rbt->NullNode) {
         CHECK((*lastListNode)->Next == &next->ListNode);
      }
      else {
         CHECK((*lastListNode)->Next == rbt->List.Head);
      }
#endif

      /* ====== Count elements =========================================== */
      (*counter)++;

      /* ====== Is right subtree okay? =================================== */
      rightHeight = RB_FUNCTION(RedBlackTreeInternalVerify)(
                       rbt, node, node->RightSubtree, lastRedBlackTreeNode,
#ifdef USE_LEAFLINKED
                       lastListNode,
#endif
                       counter);

      /* ====== Verify red-black property ================================ */
      CHECK((leftHeight != 0) || (rightHeight != 0));
      CHECK(leftHeight == rightHeight);
      if(node->Color == Red) {
         CHECK(node->LeftSubtree->Color == Black);
         CHECK(node->RightSubtree->Color == Black);
         return leftHeight;
      }
      CHECK(node->Color == Black);
      return leftHeight + 1;
   }
   return 1;
}


/* ##### Verify structures ############################################### */
void RB_FUNCTION(RedBlackTreeVerify)(
        struct RB_DEFINITION(RedBlackTree)* rbt)
{
   size_t                                  counter              = 0;
   struct RB_DEFINITION(RedBlackTreeNode)* lastRedBlackTreeNode = NULL;
#ifdef USE_LEAFLINKED
   struct DoubleLinkedRingListNode*        lastListNode         = &rbt->List.Node;
#endif

   CHECK(rbt->NullNode.Color == Black);
   CHECK(rbt->NullNode.Value == 0);
   CHECK(rbt->NullNode.ValueSum == 0);

   CHECK(RB_FUNCTION(RedBlackTreeInternalVerify)(rbt, &rbt->NullNode,
                     rbt->NullNode.LeftSubtree, &lastRedBlackTreeNode,
#ifdef USE_LEAFLINKED
                     &lastListNode,
#endif
                     &counter) != 0);
   CHECK(counter == rbt->Elements);
}


#ifdef __cplusplus
}
#endif
