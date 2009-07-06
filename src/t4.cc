#include <stdio.h>
#include <stdlib.h>
#include <math.h>


int main(int argc, char** argv)
{
   double v1 = 1.0;
   double v2 = v1 * (double)0xffffffff;   
   double v3 = rint(v2);

   unsigned long long v4 = (unsigned long long)v3;
   unsigned int       v5 = (unsigned int)v3;

   printf("%1.0lf == %1.0lf == %llu == %u\n", v2, v3, v4, v5);

   return 0;
}
