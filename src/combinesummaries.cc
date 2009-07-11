// $Id$
// ###########################################################################
//             Thomas Dreibholz's R Simulation Scripts Collection
//                  Copyright (C) 2004-2009 Thomas Dreibholz
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
#include <unistd.h>
#include <string.h>
#include <bzlib.h>

#include <iostream>
#include <fstream>


using namespace std;


// ###### Read and process data file ########################################
void addDataFile(const char*   varNames,
                 const char*   varValues,
                 const char*   inputFileName,
                 FILE*         outFile,
                 BZFILE*       outBZFile,
                 unsigned int& outPos)
{
   unsigned int line;
   const size_t bufferSize = 16384;
   char         outBuffer[bufferSize + 512];
   char         inBuffer[bufferSize + 16];
   char         storage[bufferSize + 16];
   int          bzerror;
   size_t       bytesRead;
   size_t       storageSize = 0;
   size_t       i;


   FILE* inFile = fopen(inputFileName, "r");
   if(inFile == NULL) {
      cerr << "ERROR: Unable to open input file \"" << inputFileName << "\"!" << endl;
      exit(1);
   }

   size_t fileNameLength = strlen(inputFileName);
   BZFILE* inBZFile = NULL;
   if((fileNameLength > 4) &&
      (inputFileName[fileNameLength - 4] == '.') &&
      (toupper(inputFileName[fileNameLength - 3]) == 'B') &&
      (toupper(inputFileName[fileNameLength - 2]) == 'Z') &&
      (inputFileName[fileNameLength - 1] == '2')) {
      inBZFile = BZ2_bzReadOpen(&bzerror, inFile, 0, 0, NULL, 0);
      if(bzerror != BZ_OK) {
         cerr << "ERROR: Unable to initialize BZip2 decompression on file <" << inputFileName << ">!" << endl;
         BZ2_bzReadClose(&bzerror, inBZFile);
         inBZFile = NULL;
      }
   }


   line = 0;
   for(;;) {
      memcpy((char*)&inBuffer, storage, storageSize);
      if(inBZFile) {
         bytesRead = BZ2_bzRead(&bzerror, inBZFile, (char*)&inBuffer[storageSize], bufferSize - storageSize);
      }
      else {
         bytesRead = fread((char*)&inBuffer[storageSize], 1, bufferSize - storageSize, inFile);
      }
      if(bytesRead <= 0) {
         bytesRead = 0;
      }
      bytesRead += storageSize;
      inBuffer[bytesRead] = 0x00;

      if(bytesRead == 0) {
         break;
      }

      storageSize = 0;
      for(i = 0;i < bytesRead;i++) {
         if(inBuffer[i] == '\n') {
            storageSize = bytesRead - i - 1;
            memcpy((char*)&storage, &inBuffer[i + 1], storageSize);
            inBuffer[i] = 0x00;
            break;
         }
      }

      line++;

      outBuffer[0] = 0x00;
      if(line == 1) {
         if(outPos == 0) {
            snprintf((char*)&outBuffer, sizeof(outBuffer),
                     "%s SubLineNo %s\n",
                     varNames, inBuffer);
         }
      }
      else {
         snprintf((char*)&outBuffer, sizeof(outBuffer),
                  "%u %s %s\n",
                  outPos, varValues, inBuffer);
      }

      if(outBuffer[0] != 0x00) {
         outPos++;

         BZ2_bzWrite(&bzerror, outBZFile, outBuffer, strlen(outBuffer));
         if(bzerror != BZ_OK) {
            cerr << "ERROR: Writing to output file failed!" << endl;
            BZ2_bzWriteClose(&bzerror, outBZFile, 0, NULL, NULL);
            fclose(outFile);
            exit(1);
         }
      }
   }


   if(inBZFile) {
      BZ2_bzReadClose(&bzerror, inBZFile);
   }
   fclose(inFile);
}




// ###### Main program ######################################################
int main(int argc, char** argv)
{
   const char*  varNames;
   char         varValues[1024];
   char*        varValuesPtr = NULL;
   char         simulationsDirectory[4096];
   char         inBuffer[1024];
   char*        command;
   int          bzerror;
   bool         quiet = false;

   if(argc < 3) {
      cerr << "Usage: " << argv[0]
           << " [Output File] [Var Names] {-quiet}" << endl;
      exit(1);
   }
   for(int i = 3;i < argc;i++) {
      if(!(strcmp(argv[i], "-quiet"))) {
         quiet = true;
      }
   }


   if(!quiet) {
      cout << "CombineSummaries - Version 2.10" << endl
           << "===============================" << endl << endl;
   }


   FILE* outFile = fopen(argv[1], "w");
   if(outFile == NULL) {
      cerr << "ERROR: Unable to create file <" << argv[1] << ">!" << endl;
      exit(1);
   }
   BZFILE* outBZFile = BZ2_bzWriteOpen(&bzerror, outFile, 9, 0, 30);
   if(bzerror != BZ_OK) {
      cerr << "ERROR: Unable to initialize BZip2 compression on file <" << argv[1] << ">!" << endl;
      BZ2_bzWriteClose(&bzerror, outBZFile, 0, NULL, NULL);
      exit(1);
   }


   unsigned int outPos = 0;
   varNames     = argv[2];
   varValues[0] = 0x00;
   strcpy((char*)&simulationsDirectory, ".");
   if(!quiet) {
      cout << "Ready> ";
      cout.flush();
   }
   while((command = fgets((char*)&inBuffer, sizeof(inBuffer), stdin))) {
      command[strlen(command) - 1] = 0x00;
      if(command[0] == 0x00) {
         cout << "*** End of File ***" << endl;
         break;
      }

      if(!quiet) {
         cout << command << endl;
      }

      if(!(strncmp(command, "--values=", 9))) {
         snprintf((char*)&varValues, sizeof(varValues), "%s", (char*)&command[9]);
         varValuesPtr = strdup(varValues);
         if(varValuesPtr == NULL) {
            cerr << "ERROR: Out of memory!" << endl;
            exit(1);
         }
         if(varValuesPtr[0] == '\"') {
            varValuesPtr = (char*)&varValuesPtr[1];
            size_t l = strlen(varValuesPtr);
            if(l > 0) {
               varValuesPtr[l - 1] = 0x00;
            }
         }
      }
      else if(!(strncmp(command, "--input=", 8))) {
         if(varValues[0] == 0x00) {
            cerr << "ERROR: No values given (parameter --values=...)!" << endl;
            exit(1);
         }
         addDataFile(varNames, varValuesPtr, (char*)&command[8], outFile, outBZFile, outPos);
         varValues[0] = 0x00;
         varValuesPtr = NULL;
      }
      else if(!(strncmp(command, "--simulationsdirectory=", 23))) {
         snprintf((char*)&simulationsDirectory, sizeof(simulationsDirectory), "%s", (char*)&command[23]);
      }
      else {
         cerr << "ERROR: Invalid command!" << endl;
         exit(1);
      }

      if(!quiet) {
         cout << "Ready> ";
         cout.flush();
      }
   }


   unsigned int in, out;
   BZ2_bzWriteClose(&bzerror, outBZFile, 0, &in, &out);
   if(!quiet) {
      cout << endl << endl
         << "Results written to " << argv[1]
         << " (" << outPos << " lines, "
         << in << " -> " << out << " - " << ((double)out * 100.0 / in) << "%)" << endl;
   }
   fclose(outFile);

   return(0);
}
