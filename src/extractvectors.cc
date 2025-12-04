/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2026 by Thomas Dreibholz
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <iostream>
#include <vector>
#include <map>
#include <string>

#include "inputfile.h"
#include "outputfile.h"
#include "package-version.h"


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
                                         const bool               defaultVectorSplittingMode,
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
                         << version << " (expected 2)\n";
               exit(1);
            }
            versionOkay = true;
            snprintf((char*)&outBuffer, sizeof(outBuffer),
                     "Time Event Object Vector Split Value\n");
         }
         else {
            std::cerr << "ERROR: Missing \"version\" entry in input file!\n";
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
               bool splitMode     = defaultVectorSplittingMode;
               if(!includeVector) {
                  for(std::vector<VectorInfo>::iterator iterator = vectorsToExtract.begin();
                      iterator != vectorsToExtract.end(); iterator++) {
                     const VectorInfo& vectorInfo = *iterator;
                     if(strncmp(vectorInfo.VectorPrefix.c_str(),
                                vectorName, vectorInfo.VectorPrefix.size()) == 0) {
                        includeVector = true;
                        foundVectors++;
                        if(vectorInfo.SplitMode) {
                           splitMode = true;
                        }
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
                               << splitName << "\" of object " << objectName << " ...\n";
                  }
                  else {
                     std::cout << "Adding vector \"" << vectorName << "\" of object "
                               << objectName << " ...\n";
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
                         << inputFile.getLine() << "!\n";
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
                << vectorsToExtract.size() << " specified!\n";
   }

   return outputLine;
}



// ###### Version ###########################################################
[[ noreturn ]] static void version()
{
   std::cerr << "CombineSummaries" << " " << COMBINESUMMARIES_VERSION << "\n";
   exit(0);
}


// ###### Usage #############################################################
[[ noreturn ]] static void usage(const char* program, const int exitCode)
{
   std::cerr << "Usage:\n"
      << "* Run:\n  "
      << program << "\n"
         "    input_file\n"
         "    output_file\n"
         "    [!]vector_name_prefix ...\n"
         "    [-c level|--compress level]\n"
         "    [-q|--quiet]\n"
         "* Version:\n  " << program << " [-v|--version]\n"
         "* Help:\n  "    << program << " [-h|--help]\n";
   exit(exitCode);
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   unsigned int            compressionLevel    = 9;
   bool                    vectorSplittingMode = false;
   bool                    quietMode           = false;
   std::vector<VectorInfo> vectorsToExtract;


   // ====== Handle command-line arguments ==================================
   const static struct option long_options[] = {
      { "split",    no_argument,       0, 's' },
      { "no-split", no_argument,       0, 'a' },
      { "compress", required_argument, 0, 'c' },
      { "quiet",    no_argument,       0, 'q' },

      { "help",     no_argument,       0, 'h' },
      { "version",  no_argument,       0, 'v' },
      {  nullptr,   0,                 0, 0   }
   };

   int option;
   int longIndex;
   while( (option = getopt_long_only(argc, argv, "sac:qhv", long_options, &longIndex)) != -1 ) {
      switch(option) {
         case 's':
            vectorSplittingMode = true;
          break;
         case 'a':
            vectorSplittingMode = false;
          break;
         case 'c':
            compressionLevel = atol(optarg);
            if(compressionLevel < 1) {
               compressionLevel = 1;
            }
            else if(compressionLevel > 9) {
               compressionLevel = 9;
            }
          break;
         case 'q':
            quietMode = true;
          break;
         case 'v':
            version();
          break;
         default:
            usage(argv[0], 1);
          // break;
      }
   }
   if(optind + 1 >= argc) {
      usage(argv[0], 1);
   }
   const std::string inputFileName(argv[optind++]);
   const std::string outputFileName(argv[optind++]);


   // ====== Print information ==============================================
   if(!quietMode) {
      std::cout << "ExtractVectors " << EXTRACTVECTORS_VERSION << "\n"
                << "* Vector Splitting:  " << (vectorSplittingMode  ? "on" : "off") << "\n"
                << "* Compression Level: " << compressionLevel << "\n"
                << "\n";
   }


   // ====== Open files =====================================================
   InputFile        inputFile;
   InputFileFormat  inputFileFormat = IFF_Plain;
   if( (inputFileName.rfind(".bz2") == inputFileName.size() - 4) ||
       (inputFileName.rfind(".BZ2") == inputFileName.size() - 4) ) {
      inputFileFormat = IFF_BZip2;
   }
   if(inputFile.initialize(inputFileName.c_str(), inputFileFormat) == false) {
      exit(1);
   }

   OutputFile       outputFile;
   OutputFileFormat outputFileFormat = OFF_Plain;
   if( (outputFileName.rfind(".bz2") == outputFileName.size() - 4) ||
       (outputFileName.rfind(".BZ2") == outputFileName.size() - 4) ) {
      outputFileFormat = OFF_BZip2;
   }
   if(outputFile.initialize(outputFileName.c_str(), outputFileFormat,
                            compressionLevel)== false) {
      exit(1);
   }


   // ====== Extract vectors ================================================
   for(int i = optind; i < argc; i++) {
      if(argv[i][0] == '!') {
         VectorInfo vectorInfo(&argv[i][1], true);
         vectorsToExtract.push_back(vectorInfo);
      }
      else {
         VectorInfo vectorInfo(argv[i], vectorSplittingMode);
         vectorsToExtract.push_back(vectorInfo);
      }
   }
   const unsigned long long lines =
      extractVectors(inputFile, outputFile,
                     vectorSplittingMode, vectorsToExtract);


   // ====== Close files ====================================================
   inputFile.finish();

   unsigned long long in, out;
   if(!outputFile.finish(true, &in, &out)) {
      exit(1);
   }
   if(!quietMode) {
      std::cout << "Wrote " << lines << " lines";
      if(in > 0) {
         std::cout << " (" << in << " -> " << out << " - "
                     << ((double)out * 100.0 / in) << "%)";
      }
      std::cout << "\n";
   }

   return 0;
}
