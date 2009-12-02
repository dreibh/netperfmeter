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
#include <vector>
#include <map>
#include <string>


using namespace std;


// ###### Read and process data file ########################################
size_t extractVectors(FILE*                     inFile,
                      BZFILE*                   inBZFile,
                      FILE*                     outFile,
                      BZFILE*                   outBZFile,
                      std::vector<std::string>& vectorsToExtract)
{
   std::map<unsigned int, const std::string> vectorToNameMap;
   std::map<unsigned int, const std::string> vectorToObjectMap;
   size_t       inputLine  = 0;
   size_t       outputLine = 0;
   char         outBuffer[16384 + 4096];
   char         inBuffer[16384];
   char         storage[sizeof(inBuffer) + 1];
   int          bzerror;
   size_t       bytesRead;
   size_t       storageSize = 0;
   unsigned int vectorID;
   unsigned int event;
   double       simTime;
   double       value;
   unsigned int n;
   unsigned int version;
   bool         versionOkay  = false;
   unsigned int foundVectors = 0;


   for(;;) {
      // ====== Read one line from input file ===============================
      memcpy((char*)&inBuffer, storage, storageSize);
      if(inBZFile) {
         bytesRead = BZ2_bzRead(&bzerror, inBZFile,
                                (char*)&inBuffer[storageSize], sizeof(inBuffer) - storageSize);
      }
      else {
         bytesRead = fread((char*)&inBuffer[storageSize], 1, sizeof(inBuffer) - storageSize, inFile);
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
      for(size_t i = 0;i < bytesRead;i++) {
         if(inBuffer[i] == '\n') {
            storageSize = bytesRead - i - 1;
            memcpy((char*)&storage, &inBuffer[i + 1], storageSize);
            inBuffer[i] = 0x00;
            break;
         }
      }
      inputLine++;


      // ====== Process line ================================================
      outBuffer[0] = 0x00;
      if(!versionOkay) {
         // ====== Handle version number ====================================
         if((inputLine == 1) &&
            (sscanf(inBuffer, "version %u", &version) == 1)) {
            if(version != 2) {
               cerr << "ERROR: Got unknown version number "
                    << version << " (expected 2)" << endl;
               exit(1);
            }
            versionOkay = true;
            snprintf((char*)&outBuffer, sizeof(outBuffer), "Time Event Object Vector Value\n");
         }
         else {
            cerr << "ERROR: Missing \"version\" entry in input file!" << endl;
            exit(1);
         }
      }
      else {
         // ====== Handle data line =========================================
         if(sscanf(inBuffer, "%u %u %lf %lf%n", &vectorID, &event, &simTime, &value, &n) == 4) {
            std::map<unsigned int, const std::string>::iterator found = vectorToNameMap.find(vectorID);
            if(found != vectorToNameMap.end()) {
               snprintf((char*)&outBuffer, sizeof(outBuffer), "%u\t%lf\t%u\t\"%s\"\t\"%s\"\t%lf\n",
                        (unsigned int)outputLine, simTime, event,
                        vectorToObjectMap[vectorID].c_str(), found->second.c_str(), value);
            }
         }
         
         // ====== Handle vector definition =================================
         else if(strncmp(inBuffer, "vector ", 7) == 0) {
            char objectName[1024];
            char vectorName[1024];
            if( (sscanf(inBuffer, "vector %u %1022s \"%1022[^\\\"]s\" ETV",
                        &vectorID, (char*)&objectName, (char*)&vectorName) == 3) ||
                (sscanf(inBuffer, "vector %u %1022s %1022[^\\\"]s ETV",
                        &vectorID, (char*)&objectName, (char*)&vectorName) == 3) ) {
               bool includeVector = (vectorsToExtract.size() == 0);
               if(!includeVector) {
                  for(vector<std::string>::iterator iterator = vectorsToExtract.begin();
                      iterator != vectorsToExtract.end(); iterator++) {
                     if(strncmp((*iterator).c_str(), vectorName, (*iterator).size()) == 0) {
                        includeVector = true;
                        foundVectors++;
                     }
                  }
               }
                  
               if(includeVector) {
                  cout << "Adding vector \"" << vectorName << "\" of object " << objectName << " ..." << endl;
                
                  vectorToNameMap.insert(std::pair<unsigned int, const std::string>(vectorID, std::string(vectorName)));
                  vectorToObjectMap.insert(std::pair<unsigned int, const std::string>(vectorID, std::string(objectName)));
               }
            }
            else {
               cerr << "ERROR: Unexpected vector definition on input file " << inputLine << "!" << endl;
               exit(1);
            }
         }
      }


      // ====== Write output line ===========================================
      if(outBuffer[0] != 0x00) {
         if(outBZFile) {
            BZ2_bzWrite(&bzerror, outBZFile, outBuffer, strlen(outBuffer));
            if(bzerror != BZ_OK) {
               cerr << "ERROR: Writing to BZip2 output file failed!" << endl;
               BZ2_bzWriteClose(&bzerror, outBZFile, 0, NULL, NULL);
               fclose(outFile);
               exit(1);
            }
         }
         else {
            if(fputs(outBuffer, outFile) < 0) {
               cerr << "ERROR: Writing to output file failed!" << endl;
               fclose(outFile);
               exit(1);
            }
         }
         outputLine++;
      }
   }

   if( (foundVectors < vectorsToExtract.size()) && (vectorsToExtract.size() != 0) ) {
      cerr << "WARNING: Found only " << foundVectors << " of "
           << vectorsToExtract.size() << " specified!" << endl;
   }

   return(outputLine);
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   int                      bzerror;
   std::vector<std::string> vectorsToExtract;

   if(argc < 4) {
      cerr << "Usage: " << argv[0]
           << " [Input File] [Output File] [Vector Name Prefix] ..." << endl;
      exit(1);
   }


   cout << "ExtractVectors - Version 1.00" << endl
        << "=============================" << endl << endl;


   // ====== Open files =====================================================
   const char*  inFileName       = argv[1];
   const size_t inFileNameLength = strlen(inFileName);
   FILE* inFile = fopen(inFileName, "r");
   if(inFile == NULL) {
      cerr << "ERROR: Unable to open vector file \"" << inFileName << "\"!" << endl;
      exit(1);
   }

   BZFILE* inBZFile = NULL;
   if((inFileNameLength > 4) &&
      (inFileName[inFileNameLength - 4] == '.') &&
      (toupper(inFileName[inFileNameLength - 3]) == 'B') &&
      (toupper(inFileName[inFileNameLength - 2]) == 'Z') &&
      (inFileName[inFileNameLength - 1] == '2')) {
      inBZFile = BZ2_bzReadOpen(&bzerror, inFile, 0, 0, NULL, 0);
      if(bzerror != BZ_OK) {
         cerr << "ERROR: Unable to initialize BZip2 decompression on file <" << inFileName << ">!" << endl;
         BZ2_bzReadClose(&bzerror, inBZFile);
         inBZFile = NULL;
      }
   }
    
   const char*  outFileName       = argv[2];
   const size_t outFileNameLength = strlen(outFileName);
   FILE* outFile = fopen(outFileName, "w");
   if(outFile == NULL) {
      cerr << "ERROR: Unable to create file <" << outFileName << ">!" << endl;
      exit(1);
   }

   BZFILE* outBZFile = NULL;
   if((outFileNameLength > 4) &&
      (outFileName[outFileNameLength - 4] == '.') &&
      (toupper(outFileName[outFileNameLength - 3]) == 'B') &&
      (toupper(outFileName[outFileNameLength - 2]) == 'Z') &&
      (outFileName[outFileNameLength - 1] == '2')) {
      outBZFile = BZ2_bzWriteOpen(&bzerror, outFile, 9, 0, 30);
      if(bzerror != BZ_OK) {
         cerr << "ERROR: Unable to initialize BZip2 compression on file <" << outFileName << ">!" << endl;
         BZ2_bzWriteClose(&bzerror, outBZFile, 0, NULL, NULL);
         exit(1);
      }
   }


   // ====== Extract vectors ================================================
   for(int i = 3;i < argc;i++) {
      vectorsToExtract.push_back(std::string(argv[i]));
   }
   const size_t lines = extractVectors(inFile, inBZFile, outFile, outBZFile, vectorsToExtract);


   // ====== Close files ====================================================
   if(inBZFile) {
      BZ2_bzReadClose(&bzerror, inBZFile);
   }
   fclose(inFile);

   if(outBZFile) {
      unsigned int in, out;
      BZ2_bzWriteClose(&bzerror, outBZFile, 0, &in, &out);
      cout << endl
           << "Results written to " << inFileName
           << " (" << lines << " lines, "
           << in << " -> " << out << " - " << ((double)out * 100.0 / in) << "%)" << endl;
   }
   if(fclose(outFile) != 0) {
      cerr << "ERROR: Closing output file failed!" << endl;
      exit(1);
   }
   return(0);
}
