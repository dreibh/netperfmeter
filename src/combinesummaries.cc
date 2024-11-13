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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <iostream>
#include <fstream>

#include "inputfile.h"
#include "outputfile.h"


// ###### Read and process data file ########################################
void addDataFile(OutputFile&         outputFile,
                 unsigned long long& outputLineNumber,
                 const std::string&  varNames,
                 const std::string&  varValues,
                 const std::string&  inputFileName)
{
   // ====== Open input file ================================================
   InputFile       inputFile;
   InputFileFormat inputFileFormat = IFF_Plain;
   if( (inputFileName.rfind(".bz2") == inputFileName.size() - 4) ||
       (inputFileName.rfind(".BZ2") == inputFileName.size() - 4) ) {
       inputFileFormat = IFF_BZip2;
   }
   if(inputFile.initialize(inputFileName.c_str(), inputFileFormat) == false) {
      exit(1);
   }

   for(;;) {
      // ====== Read line from input file ===================================
      bool          eof;
      char          buffer[4097];
      const ssize_t bytesRead = inputFile.readLine((char*)&buffer, sizeof(buffer), eof);
      if((bytesRead < 0) || (eof)) {
         break;
      }

      // ====== Process line ================================================
      if(inputFile.getLine() == 1) {
         if(outputLineNumber == 0) {
            if(outputFile.printf("%s SubLineNo %s\n", varNames.c_str(), buffer) == false) {
               outputFile.finish();
               exit(1);
            }
         }
      }
      else {
         if(outputFile.printf("%07llu\t%s\t%s\n",
                              outputLineNumber, varValues.c_str(), buffer) == false) {
            outputFile.finish();
            exit(1);
         }
      }
      outputLineNumber++;
   }

   inputFile.finish();
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   bool         quiet            = false;
   unsigned int compressionLevel = 9;

   // ====== Process arguments ==============================================
   if(argc < 3) {
      std::cerr << "Usage: " << argv[0]
                << " [Output File] [Var Names] {-compress=0-9} {-quiet}\n";
      exit(1);
   }
   for(int i = 3;i < argc;i++) {
      if(!(strcmp(argv[i], "-quiet"))) {
         quiet = true;
      }
      else if(!(strncmp(argv[i], "-compress=", 10))) {
         compressionLevel = atol((char*)&argv[i][10]);
         if(compressionLevel > 9) {
            compressionLevel = 9;
         }
      }
   }


   // ====== Print information ==============================================
   if(!quiet) {
      std::cout << "CombineSummaries - Version 2.30\n"
                << "===============================\n\n";
   }


   // ====== Open output file ===============================================
   OutputFile        outputFile;
   const std::string outputFileName   = argv[1];
   OutputFileFormat  outputFileFormat = OFF_Plain;
   if( (outputFileName.rfind(".bz2") == outputFileName.size() - 4) ||
       (outputFileName.rfind(".BZ2") == outputFileName.size() - 4) ) {
      outputFileFormat = OFF_BZip2;
   }
   if(outputFile.initialize(outputFileName.c_str(),outputFileFormat,
                            compressionLevel)== false) {
      exit(1);
   }


   // ====== Process input ==================================================
   unsigned long long outputLineNumber = 0;
   std::string varNames                = argv[2];
   std::string varValues               = "";
   std::string simulationsDirectory    = ".";
   char        commandBuffer[4097];
   char*       command;

   if(!quiet) {
      std::cout << "Ready> ";
      std::cout.flush();
   }
   while((command = fgets((char*)&commandBuffer, sizeof(commandBuffer), stdin))) {
      command[strlen(command) - 1] = 0x00;
      if(command[0] == 0x00) {
         std::cout << "*** End of File ***\n";
         break;
      }
      if(!quiet) {
         std::cout << command << "\n";
      }

      if(!(strncmp(command, "--values=", 9))) {
         varValues = (const char*)&command[9];
         if(varValues[0] == '\"') {
            varValues = varValues.substr(1, varValues.size() - 2);
         }
      }
      else if(!(strncmp(command, "--input=", 8))) {
         if(varValues[0] == 0x00) {
            std::cerr << "ERROR: No values given (parameter --values=...)!\n";
            exit(1);
         }
         addDataFile(outputFile, outputLineNumber,
                     varNames, varValues, std::string((const char*)&command[8]));
         varValues = "";
      }
      else if(!(strncmp(command, "--simulationsdirectory=", 23))) {
         simulationsDirectory = (const char*)&simulationsDirectory[23];
      }
      else {
         std::cerr << "ERROR: Invalid command!\n";
         exit(1);
      }

      if(!quiet) {
         std::cout << "Ready> ";
         std::cout.flush();
      }
   }


   // ====== Close output file ==============================================
   unsigned long long in, out;
   if(!outputFile.finish(true, &in, &out)) {
      exit(1);
   }
   if(!quiet) {
      std::cout << "\n" << "Wrote " << outputLineNumber << " lines";
      if(in > 0) {
         std::cout << " (" << in << " -> " << out << " - "
                   << ((double)out * 100.0 / in) << "%)";
      }
      std::cout << "\n";
   }

   return 0;
}
