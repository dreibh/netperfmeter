#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "cpustatus.h"

int main(int argc, char** argv)
{
   CPUStatus cpuStatus;

   // ====== Initialise =====================================================
   const unsigned int ncpus   = cpuStatus.getNumberOfCPUs();
   const unsigned int nstates = cpuStatus.getCpuStates();

   // ====== Measure ========================================================
   puts("Measuring ...");
   usleep(1000000);
   cpuStatus.update();

   // ====== Print per-core CPU status ======================================
   printf("\x1b[34m%6s\t\x1b[33m", "CPU");
   for(unsigned int s = 0; s <nstates; s++) {
      printf("%12s\t", cpuStatus.getCpuStateName(s));
   }
   puts("\x1b[37mUtilisation\x1b[0m");

   char buffer[128];
   for(unsigned int c = 0; c <= ncpus; c++) {
      if(c > 0) {
         printf("\x1b[34m%6u\t\x1b[33m", c);
      }
      else {
         printf("\x1b[34m%6s\t\x1b[33m", "TOTAL");
      }

      for(unsigned int s = 0; s <nstates; s++) {
         float statePercentage = cpuStatus.getCpuStatePercentage(c, s);
         snprintf(buffer, sizeof(buffer), "%1.3f %%", statePercentage);
         printf("%12s\t", buffer);
      }

      float utilisationPercentage = cpuStatus.getCpuUtilization(c);
      printf("\x1b[37m%1.3f %%\x1b[0m\n", utilisationPercentage);
   }

   return 0;
}
