// $Id$
// ###########################################################################
//             Thomas Dreibholz's R Simulation Scripts Collection
//                  Copyright (C) 2004-2011 Thomas Dreibholz
//
//           Author: Thomas Dreibholz, dreibh@iem.uni-due.de
// ###########################################################################
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY// without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Contact: dreibh@iem.uni-due.de

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <iostream>


int main(int argc, char** argv)
{
   if(argc < 4) {
      std::cerr << "Usage: " << argv[0] << " [Number 1] [Operator] [Number 2] {-exponential}" << std::endl;
      exit(1);
   }

   const long double number1 = strtold(argv[1], NULL);
   const long double number2 = strtold(argv[3], NULL);
   long double       result  = 0.0;

   if(strcmp(argv[2], "+") == 0) {
      result = number1 + number2;
   }
   else if(strcmp(argv[2], "-") == 0) {
      result = number1 - number2;
   }
   else if(strcmp(argv[2], "*") == 0) {
      result = number1 * number2;
   }
   else if(strcmp(argv[2], "/") == 0) {
      result = number1 / number2;
   }
   else {
      std::cerr << "ERROR: Invalid operator " << argv[2] << "!" << std::endl;
      exit(1);
   }

   char str[128];
   if( (argc > 4) && (strcmp(argv[4], "-exponential") == 0) ) {
      snprintf((char*)&str, sizeof(str), "%1.15Le", result);
   }
   else {
      snprintf((char*)&str, sizeof(str), "%1.15Lf", result);
   }
   std::cout << str << std::endl;

   return 0;
}
