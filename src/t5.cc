#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/proc.h>

#include <iostream>

class CPUStatus
{
   public:
   CPUStatus();
   ~CPUStatus();

   void update();

   inline unsigned int getNumberOfCPUs() const {
      return(CPUs);
   }
   inline unsigned int getCpuStates() const {
      return(CpuStates);
   }
   inline const char* getCpuStateName(const unsigned int index) const {
      assert(index < CpuStates);
      return(CpuStateNames[index]);
   }
   inline float getCpuStatePercentage(const unsigned int cpuIndex,
                                      const unsigned int stateIndex) const {
      assert(cpuIndex <= CPUs);
      assert(stateIndex < CpuStates);
      const unsigned int index = (cpuIndex * CpuStates) + stateIndex;
      return(Percentages[index]);
   }

   private:
#ifdef __FreeBSD__
   static bool getSysCtl(const char* name, void* ptr, size_t len);
#endif

   unsigned int        CPUs;
   long*               CpuTimes;
   long*               OldCpuTimes;
   float*              Percentages;
   unsigned int        CpuStates;
   static const char*  CpuStateNames[];
};


#ifdef __FreeBSD__
const char* CPUStatus::CpuStateNames[] = {
   "User", "Nice", "System", "Interrupt", "Idle"
};

bool CPUStatus::getSysCtl(const char* name, void* ptr, size_t len)
{
   size_t nlen = len;
   if(sysctlbyname(name, ptr, &nlen, NULL, 0) < 0) {
      std::cerr << "ERROR: sysctlbyname(" << name << ") failed: " << strerror(errno) << std::endl;
      exit(1);
   }
   if(nlen != len) {
      return(false);
   }
   return(true);
}
#endif


// ###### Constructor #######################################################
CPUStatus::CPUStatus()
{
   int maxCPUs;
#ifdef __FreeBSD__
   CpuStates = CPUSTATES;
   getSysCtl("kern.smp.maxcpus", &maxCPUs, sizeof(maxCPUs));
#else
#error FIXME: I do not know what to do here!
#endif
   size_t cpuTimesSize = sizeof(long) * (maxCPUs + 1) * CpuStates;
   CpuTimes = (long*)calloc(1, cpuTimesSize);
   assert(CpuTimes != NULL);

#ifdef __FreeBSD__
   if(sysctlbyname("kern.cp_times", CpuTimes, &cpuTimesSize, NULL, 0) < 0) {
      std::cerr << "ERROR: sysctlbyname(kern.cp_times) failed: "
                << strerror(errno) << std::endl;
      exit(1);
   }
   CPUs = cpuTimesSize / CpuStates / sizeof(long);
#else
#error FIXME: I do not know what to do here!
#endif

   cpuTimesSize = sizeof(long) * (CPUs + 1) * CpuStates;
   OldCpuTimes = (long*)malloc(cpuTimesSize);
   assert(OldCpuTimes != NULL);
   memcpy(OldCpuTimes, CpuTimes, cpuTimesSize);

   const size_t percentagesSize = sizeof(float) * (CPUs + 1) * CpuStates;
   Percentages = (float*)malloc(percentagesSize);
   assert(Percentages != NULL);
   for(unsigned int i = 0; i < (CPUs + 1) * CpuStates; i++) {
      Percentages[i] = 100.0 / CpuStates;
   }
}


// ###### Destructor ########################################################
CPUStatus::~CPUStatus()
{
   delete CpuTimes;
   CpuTimes = NULL;
   delete OldCpuTimes;
   OldCpuTimes = NULL;
   delete Percentages;
   Percentages = NULL;
}


// ###### Update status #####################################################
void CPUStatus::update()
{
   // ====== Save old values ================================================
   size_t cpuTimesSize = sizeof(long) * (CPUs + 1) * CpuStates;
   memcpy(OldCpuTimes, CpuTimes, cpuTimesSize);


   // ====== Get counters ===================================================
#ifdef __FreeBSD__
   unsigned int i, j;
   cpuTimesSize = sizeof(long) * CPUs * CpuStates;   /* Total is calculated later! */
   if(sysctlbyname("kern.cp_times", (long*)&CpuTimes[CpuStates], &cpuTimesSize, NULL, 0) < 0) {
      std::cerr << "ERROR: sysctlbyname(kern.cp_times) failed: " << strerror(errno) << std::endl;
      exit(1);
   }
   // ------ Compute total values -------------------------
   for(j = 0; j < CpuStates; j++) {
      CpuTimes[j] = 0;
   }
   for(i = 0; i < CPUs; i++) {
      for(j = 0; j < CpuStates; j++) {
         CpuTimes[j] += CpuTimes[((i + 1) * CpuStates) + j];
      }
   }
#else
#error FIXME: I do not know what to do here!
#endif


   // ====== Calculate percentages ==========================================
   for(i = 0; i < CPUs + 1; i++) {
      long diffTotal = 0;
      long diff[CpuStates];
      for(j = 0; j < CpuStates; j++) {
         const unsigned int index = (i * CpuStates) + j;
         diff[j] = CpuTimes[index] - OldCpuTimes[index];
         if(diff[j] < 0) {   // Counter wrap!
            diff[j] = OldCpuTimes[index] - CpuTimes[index];
         }
         diffTotal += diff[j];
      }
      for(j = 0; j < CpuStates; j++) {
         const unsigned int index = (i * CpuStates) + j;
         Percentages[index] = 100.0 * (float)diff[j] / (float)diffTotal;
      }
   }
}



int main(int argc, char** argv)
{
   CPUStatus status;

   status.update();
   usleep(1000000);
   status.update();

  for(unsigned int i = 1; i <= status.getNumberOfCPUs(); i++) {
     printf("CPU #%d:\t", i);
     for(unsigned int j = 0; j < status.getCpuStates(); j++) {
        printf("\t%s=%1.2f%%",
               status.getCpuStateName(j),
               status.getCpuStatePercentage(i, j));
     }
     puts("");
  }

   return 0;
}
