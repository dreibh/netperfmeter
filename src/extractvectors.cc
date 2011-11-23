// $Id$
// ###########################################################################
//             Thomas Dreibholz's R Simulation Scripts Collection
//                  Copyright (C) 2004-2012 Thomas Dreibholz
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

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>

#include "inputfile.h"
#include "outputfile.h"


class VectorInfo
{
   public:
   VectorInfo(const std::string& vectorPrefix, const bool splitMode) :
   VectorPrefix(vectorPrefix), SplitMode(splitMode) { }
   std::string VectorPrefix;
   bool        SplitMode;
};


// ###### Read and process data file ########################################
static unsigned long long extractVectors(InputFile&               inputFile,
                                         OutputFile&              outputFile,
                                         const bool               splitAll,
                                         std::vector<VectorInfo>& vectorsToExtract)
{
   std::map<unsigned int, const std::string> vectorToNameMap;
   std::map<unsigned int, const std::string> vectorToSplitMap;
   std::map<unsigned int, const std::string> vectorToObjectMap;
   unsigned long long                        outputLine = 0;
   char                                      inBuffer[4096];
   char                                      outBuffer[sizeof(inBuffer) + 4096];
   unsigned int                              foundVectors = 0;
   bool                                      versionOkay  = false;

   for(;;) {
      // ====== Read line from input file ===================================
      bool          eof;
      const ssize_t bytesRead = inputFile.readLine((char*)&inBuffer, sizeof(inBuffer), eof);
      if((bytesRead < 0) || (eof)) {
         break;
      }

      // ====== Process line ================================================
      outBuffer[0] = 0x00;
      if(!versionOkay) {
         // ====== Handle version number ====================================
         unsigned int version;
         if((inputFile.getLine() == 1) &&
            (sscanf(inBuffer, "version %u", &version) == 1)) {
            if(version != 2) {
               std::cerr << "ERROR: Got unknown version number "
                         << version << " (expected 2)" << std::endl;
               exit(1);
            }
            versionOkay = true;
            snprintf((char*)&outBuffer, sizeof(outBuffer),
                     "Time Event Object Vector Split Value\n");
         }
         else {
            std::cerr << "ERROR: Missing \"version\" entry in input file!"
                      << std::endl;
            exit(1);
         }
      }
      else {
         // ====== Handle data line =========================================
         unsigned int vectorID;
         unsigned int event;
         double       simTime;
         double       value;
         unsigned int n;
         if(sscanf(inBuffer, "%u %u %lf %lf%n",
                   &vectorID, &event, &simTime, &value, &n) == 4) {
            std::map<unsigned int, const std::string>::iterator found =
               vectorToNameMap.find(vectorID);
            if(found != vectorToNameMap.end()) {
               snprintf((char*)&outBuffer, sizeof(outBuffer),
                        "%u\t%lf\t%u\t\"%s\"\t\"%s\" \"%s\"\t%lf\n",
                        (unsigned int)outputLine, simTime, event,
                        vectorToObjectMap[vectorID].c_str(),
                        found->second.c_str(),
                        vectorToSplitMap[vectorID].c_str(), value);
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
               bool splitMode     = splitAll;
               if(!includeVector) {
                  for(std::vector<VectorInfo>::iterator iterator = vectorsToExtract.begin();
                      iterator != vectorsToExtract.end(); iterator++) {
                     const VectorInfo& vectorInfo = *iterator;
                     if(strncmp(vectorInfo.VectorPrefix.c_str(),
                                vectorName, vectorInfo.VectorPrefix.size()) == 0) {
                        includeVector = true;
                        foundVectors++;
                     }
                  }
               }

               if(includeVector) {
                  const char* splitName = "";
                  if(splitMode) {
                     for(int i = strlen(vectorName); i >= 0; i--) {
                        if(vectorName[i] == ' ') {
                           splitName     = (const char*)&vectorName[i + 1];
                           vectorName[i] = 0x00;
                           break;
                        }
                     }
                     std::cout << "Adding vector \"" << vectorName << "\", split \""
                               << splitName << "\" of object " << objectName << " ..." << std::endl;
                  }
                  else {
                     std::cout << "Adding vector \"" << vectorName << "\" of object "
                               << objectName << " ..." << std::endl;
                  }

                  vectorToNameMap.insert(std::pair<unsigned int, const std::string>(
                     vectorID, std::string(vectorName)));
                  vectorToSplitMap.insert(std::pair<unsigned int, const std::string>(
                     vectorID, std::string(splitName)));
                  vectorToObjectMap.insert(std::pair<unsigned int, const std::string>(
                     vectorID, std::string(objectName)));
               }
            }
            else {
               std::cerr << "ERROR: Unexpected vector definition on input file "
                         << inputFile.getLine() << "!" << std::endl;
               exit(1);
            }
         }
      }


      // ====== Write output line ===========================================
      if(outBuffer[0] != 0x00) {
         if(!outputFile.write(outBuffer, strlen(outBuffer))) {
            exit(1);
         }
         outputLine++;
      }
   }

   // ====== Warn, if no vector had been extracted ==========================
   if( (foundVectors < vectorsToExtract.size()) &&
       (vectorsToExtract.size() != 0) ) {
      std::cerr << "WARNING: Found only " << foundVectors << " of "
                << vectorsToExtract.size() << " specified!" << std::endl;
   }

   return(outputLine);
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   unsigned int            compressionLevel = 9;
   bool                    quiet            = false;
   std::vector<VectorInfo> vectorsToExtract;


   if(argc < 4) {
      std::cerr << "Usage: " << argv[0]
                << " [Input File] [Output File] {-quiet} {-splitall} {-compress=}"
                   " {{-split}  {Vector Name Prefix} ...}" << std::endl;
      exit(1);
   }
   for(int i = 3;i < argc;i++) {
      if(argv[i][0] == '-') {
         if( (strcmp(argv[i], "-split") == 0) ||
             (strcmp(argv[i], "-splitall") == 0) ) {
            // To be processed later ...
         }
         else if(!(strcmp(argv[i], "-quiet"))) {
            quiet = true;
         }
         else if(!(strncmp(argv[i], "-compress=", 10))) {
            compressionLevel = atol((char*)&argv[i][10]);
            if(compressionLevel > 9) {
               compressionLevel = 9;
            }
         }
      }
   }


   if(!quiet) {
      std::cout << "ExtractVectors - Version 1.20" << std::endl
                << "=============================" << std::endl << std::endl;
   }


   // ====== Open files =====================================================
   InputFile         inputFile;
   const std::string inputFileName   = argv[1];
   InputFileFormat   inputFileFormat = IFF_Plain;
   if( (inputFileName.rfind(".bz2") == inputFileName.size() - 4) ||
       (inputFileName.rfind(".BZ2") == inputFileName.size() - 4) ) {
      inputFileFormat = IFF_BZip2;
   }
   if(inputFile.initialize(inputFileName.c_str(), inputFileFormat) == false) {
      exit(1);
   }

   OutputFile        outputFile;
   const std::string outputFileName   = argv[2];
   OutputFileFormat  outputFileFormat = OFF_Plain;
   if( (outputFileName.rfind(".bz2") == outputFileName.size() - 4) ||
       (outputFileName.rfind(".BZ2") == outputFileName.size() - 4) ) {
      outputFileFormat = OFF_BZip2;
   }
   if(outputFile.initialize(outputFileName.c_str(), outputFileFormat,
                            compressionLevel)== false) {
      exit(1);
   }


   // ====== Extract vectors ================================================
   bool splitMode = false;
   bool splitAll  = false;
   for(int i = 3;i < argc;i++) {
      if(argv[i][0] == '-') {
         if(strcmp(argv[i], "-split") == 0) {
            splitMode = true;
         }
         else if(strcmp(argv[i], "-splitall") == 0) {
            splitAll = true;
         }
         else {
            std::cerr << "ERROR: Bad parameter \"" << argv[i] << "\"!"
                      << std::endl;
            exit(1);
         }
      }
      else {
         VectorInfo vectorInfo(argv[i], splitMode);
         vectorsToExtract.push_back(vectorInfo);
         if(!splitAll) {
            splitMode = false;
         }
      }
   }
   const unsigned long long lines = extractVectors(inputFile, outputFile,
                                                   splitAll, vectorsToExtract);


   // ====== Close files ====================================================
   inputFile.finish();

   unsigned long long in, out;
   if(!outputFile.finish(true, &in, &out)) {
      exit(1);
   }
   if(!quiet) {
      std::cout << "Wrote " << lines << " lines";
      if(in > 0) {
         std::cout << " (" << in << " -> " << out << " - "
                     << ((double)out * 100.0 / in) << "%)";
      }
      std::cout << std::endl;
   }

   return(0);
}
