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

#ifndef DEBUG_H
#define DEBUG_H


#include <stdio.h>
#include <stdlib.h>


/*
#define DEBUG
*/
/*
#define VERIFY
*/


#ifndef CHECK
#define CHECK(cond) if(!(cond)) { fprintf(stderr, "INTERNAL ERROR in %s, line %u: condition %s is not satisfied!\n", __FILE__, __LINE__, #cond); abort(); }
#endif

#if NDEBUG != 0
#define OPP_CHECK(cond) if(!(cond)) { throw new cException("INTERNAL ERROR in %s, line %u: condition %s is not satisfied!\n", __FILE__, __LINE__, #cond); }
#else
#define OPP_CHECK(cond) if(!(cond)) { fprintf(stderr, "INTERNAL ERROR in %s, line %u: condition %s is not satisfied!\n", __FILE__, __LINE__, #cond); abort(); }
#endif


#endif
