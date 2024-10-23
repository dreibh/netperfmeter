#include <stdlib.h>
#include <string.h>

#include "loglevel.h"
#include "assure.h"

int main(int argc, char** argv)
{
   for(int i = 1;i < argc;i++) {
      if(!(strncmp(argv[i], "-log" ,4))) {
         if(initLogging(argv[i]) == false) {
            return(0);
         }
      }
   }

   LOG_FATAL
   stdlog << "Fatal\n";
   LOG_END

   LOG_ERROR
   stdlog << "Error\n";
   LOG_END

   LOG_WARNING
   stdlog << "Warning\n";
   LOG_END

   LOG_INFO
   stdlog << "Info\n";
   LOG_END

   LOG_DEBUG
   stdlog << "Debug\n";
   LOG_END

   LOG_TRACE
   stdlog << "Trace\n";
   LOG_END

   assure(true);
   assure(false);

   return 0;
}
