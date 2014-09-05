#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "tools.h"



int main(int argc, char** argv)
{
   double valueArray[] = {
      500, 1.5
   };
   
   for(int i = 0; i < 5000000; i++) {
      const double v = getRandomValue((const double*)&valueArray, RANDOM_PARETO);
      
      printf("%06d\t%1.9lf\n", i, v);
   }
   
   return 0;
}
