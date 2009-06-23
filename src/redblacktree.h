/* $Id$
 * --------------------------------------------------------------------------
 *
 *              //===//   //=====   //===//   //=====  //   //      //
 *             //    //  //        //    //  //       //   //=/  /=//
 *            //===//   //=====   //===//   //====   //   //  //  //
 *           //   \\         //  //             //  //   //  //  //
 *          //     \\  =====//  //        =====//  //   //      //  Version V
 *
 * ------------- An Open Source RSerPool Simulation for OMNeT++ -------------
 *
 * Copyright (C) 2003-2009 by Thomas Dreibholz
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
 * Contact: dreibh@iem.uni-due.de
 */

#include <stdio.h>
#include <stdlib.h>


#undef RB_DEFINITION
#undef RB_FUNCTION

#ifdef USE_LEAFLINKED
#include "doublelinkedringlist.h"
#define RB_DEFINITION(x) LeafLinked##x
#define RB_FUNCTION(x)   leafLinked##x
#else
#define RB_DEFINITION(x) Simple##x
#define RB_FUNCTION(x)   simple##x
#endif


#if (!defined(LEAFLINKED_REDBLACKTREE_H) && defined(USE_LEAFLINKED)) || (!defined(REGULAR_REDBLACKTREE_H) && !defined(USE_LEAFLINKED))
#ifdef USE_LEAFLINKED
#define LEAFLINKED_REDBLACKTREE_H
#else
#define REGULAR_REDBLACKTREE_H
#endif

#ifdef __cplusplus
extern "C" {
#endif


#ifndef REDBLACKTREE_H_CONSTANTS
#define REDBLACKTREE_H_CONSTANTS
typedef unsigned long long RedBlackTreeNodeValueType;

enum RedBlackTreeNodeColorType
{
   Red   = 1,
   Black = 2
};
#endif


struct RB_DEFINITION(RedBlackTreeNode)
{
#ifdef USE_LEAFLINKED
   struct DoubleLinkedRingListNode         ListNode;
#endif
   struct RB_DEFINITION(RedBlackTreeNode)* Parent;
   struct RB_DEFINITION(RedBlackTreeNode)* LeftSubtree;
   struct RB_DEFINITION(RedBlackTreeNode)* RightSubtree;
   enum RedBlackTreeNodeColorType          Color;
   RedBlackTreeNodeValueType               Value;
   RedBlackTreeNodeValueType               ValueSum;  /* ValueSum := LeftSubtree->Value + Value + RightSubtree->Value */
};

struct RB_DEFINITION(RedBlackTree)
{
   struct RB_DEFINITION(RedBlackTreeNode) NullNode;
#ifdef USE_LEAFLINKED
   struct DoubleLinkedRingList            List;
#endif
   size_t                                 Elements;
   void                                   (*PrintFunction)(const void* node, FILE* fd);
   int                                    (*ComparisonFunction)(const void* node1, const void* node2);
};


void RB_FUNCTION(RedBlackTreeNodeNew)(struct RB_DEFINITION(RedBlackTreeNode)* node);
void RB_FUNCTION(RedBlackTreeNodeDelete)(struct RB_DEFINITION(RedBlackTreeNode)* node);
int RB_FUNCTION(RedBlackTreeNodeIsLinked)(const struct RB_DEFINITION(RedBlackTreeNode)* node);


void RB_FUNCTION(RedBlackTreeNew)(struct RB_DEFINITION(RedBlackTree)* rbt,
                                  void                                (*printFunction)(const void* node, FILE* fd),
                                  int                                 (*comparisonFunction)(const void* node1, const void* node2));
void RB_FUNCTION(RedBlackTreeDelete)(struct RB_DEFINITION(RedBlackTree)* rbt);
void RB_FUNCTION(RedBlackTreeVerify)(struct RB_DEFINITION(RedBlackTree)* rbt);
void RB_FUNCTION(RedBlackTreePrint)(const struct RB_DEFINITION(RedBlackTree)* rbt,
                                    FILE*                                     fd);
int RB_FUNCTION(RedBlackTreeIsEmpty)(const struct RB_DEFINITION(RedBlackTree)* rbt);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetFirst)(
                                           const struct RB_DEFINITION(RedBlackTree)* rbt);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetLast)(
                                           const struct RB_DEFINITION(RedBlackTree)* rbt);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetPrev)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* node);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetNext)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* node);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetNearestPrev)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetNearestNext)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode);
size_t RB_FUNCTION(RedBlackTreeGetElements)(const struct RB_DEFINITION(RedBlackTree)* rbt);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeInsert)(
                                           struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           struct RB_DEFINITION(RedBlackTreeNode)* node);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeRemove)(
                                           struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           struct RB_DEFINITION(RedBlackTreeNode)* node);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeFind)(
                                           const struct RB_DEFINITION(RedBlackTree)*     rbt,
                                           const struct RB_DEFINITION(RedBlackTreeNode)* cmpNode);
RedBlackTreeNodeValueType RB_FUNCTION(RedBlackTreeGetValueSum)(
                                           const struct RB_DEFINITION(RedBlackTree)* rbt);
struct RB_DEFINITION(RedBlackTreeNode)* RB_FUNCTION(RedBlackTreeGetNodeByValue)(
                                           const struct RB_DEFINITION(RedBlackTree)* rbt,
                                           RedBlackTreeNodeValueType                 value);


#ifdef __cplusplus
}
#endif

#endif
