/* $Id$
 * --------------------------------------------------------------------------
 *
 *              //===//   //=====   //===//   //       //   //===//
 *             //    //  //        //    //  //       //   //    //
 *            //===//   //=====   //===//   //       //   //===<<
 *           //   \\         //  //        //       //   //    //
 *          //     \\  =====//  //        //=====  //   //===//    Version II
 *
 * ------------- An Efficient RSerPool Prototype Implementation -------------
 *
 * Copyright (C) 2002-2013 by Thomas Dreibholz
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
#include <unistd.h>
#include <string.h>


int main(int argc, char** argv)
{
   const char* directory  = NULL;
   int         programArg = 1;
   int         i;

   if(argc < 2) {
      fprintf(stderr, "Usage: %s {-chdir=Directory} [Program] {Argument} ...\n", argv[0]);
      exit(1);
   }

   for(i = 1; i < argc; i++) {
      if(argv[i][0] == '-') {
         programArg = i + 1;
         if(strncmp(argv[i], "-chdir=", 7) == 0) {
            directory = (const char*)&argv[i][7];
         }
         else {
            fprintf(stderr, "ERROR: Bad parameter \"%s\".\n", argv[i]);
            exit(1);
         }
      }
      else {
         break;
      }
   }

   if( (setuid(0) == 0) && (setgid(0) == 0) ) {
      if(directory) {
         if(chdir(directory) != 0) {
            perror("Unable to change directory");
            exit(1);
         }
      }
      execve(argv[programArg], &argv[programArg], NULL);
      perror("Unable to start program");
   }
   else {
      perror("Unable obtain root permissions. Ensure that rootshell is owned by root and has setuid set! Error");
   }
   return(1);
}
