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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

#if defined(HAVE_LIBIBERTY)
#include <libiberty.h>
extern "C" {
int getopt_long_only(int argc, char* const* argv, const char* optstring,
                     const struct option* longopts, int* longindex);
}
#endif

#include "inputfile.h"
#include "outputfile.h"
#include "package-version.h"


class VectorInfo
{
   public:
   VectorInfo(const std::string& vectorPrefix, const bool splitMode) :
   VectorPrefix(vectorPrefix), SplitMode(splitMode), Found(false) { }
   std::string VectorPrefix;
   bool        SplitMode;
   bool        Found;
};


// ###### Read and process data file ########################################
static unsigned long long extractVectors(InputFile&               inputFile,
                                         OutputFile&              outputFile,
                                         const bool               defaultVectorSplittingMode,
                                         std::vector<VectorInfo>& vectorsToExtract,
                                         const bool               addLineNumbers,
                                         const char*              separator)
{
   std::map<unsigned int, const std::string> vectorToNameMap;
   std::map<unsigned int, const std::string> vectorToSplitMap;
   std::map<unsigned int, const std::string> vectorToObjectMap;
   unsigned long long                        outputLine = 0;
   char                                      inBuffer[4096];
   char                                      outBuffer[sizeof(inBuffer) + 4096];
   bool                                      versionOkay  = false;

   for(;;) {
      // ====== Read line from input file ===================================
      bool          eof;
      const ssize_t bytesRead = inputFile.readLine(inBuffer, sizeof(inBuffer), eof);
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
            snprintf(outBuffer, sizeof(outBuffer),
                     "Time%sEvent%sObject%sVector%sSplit%sValue\n",
                     separator, separator, separator, separator, separator);
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
         int          n;
         if(sscanf(inBuffer, "%u %u %lf %lf%n",
                   &vectorID, &event, &simTime, &value, &n) == 4) {
            std::map<unsigned int, const std::string>::iterator found =
               vectorToNameMap.find(vectorID);
            if(found != vectorToNameMap.end()) {
               if(addLineNumbers) {
                  snprintf(outBuffer, sizeof(outBuffer),
                           "%u%s%lf%s%u%s\"%s\"%s\"%s\"%s\"%s\"%s%lf\n",
                           (unsigned int)outputLine,            separator,
                           simTime,                             separator,
                           event,                               separator,
                           vectorToObjectMap[vectorID].c_str(), separator,
                           found->second.c_str(),               separator,
                           vectorToSplitMap[vectorID].c_str(),  separator,
                           value);
               }
               else {
                  snprintf(outBuffer, sizeof(outBuffer),
                           "%lf%s%u%s\"%s\"%s\"%s\"%s\"%s\"%s%lf\n",
                           simTime,                             separator,
                           event,                               separator,
                           vectorToObjectMap[vectorID].c_str(), separator,
                           found->second.c_str(),               separator,
                           vectorToSplitMap[vectorID].c_str(),  separator,
                           value);
               }
            }
         }

         // ====== Handle vector definition =================================
         else if(strncmp(inBuffer, "vector ", 7) == 0) {
            char objectName[1024];
            char vectorName[1024];
            if( (sscanf(inBuffer, "vector %u %1022s \"%1022[^\\\"]\" ETV",
                        &vectorID, objectName, vectorName) == 3) ||
                (sscanf(inBuffer, "vector %u %1022s %1022[^\\\"] ETV",
                        &vectorID, objectName, vectorName) == 3) ) {
               bool includeVector = (vectorsToExtract.size() == 0);
               bool splitMode     = defaultVectorSplittingMode;
               if(!includeVector) {
                  for(std::vector<VectorInfo>::iterator iterator = vectorsToExtract.begin();
                      iterator != vectorsToExtract.end(); iterator++) {
                     VectorInfo& vectorInfo = *iterator;
                     if(strncmp(vectorInfo.VectorPrefix.c_str(),
                                vectorName, vectorInfo.VectorPrefix.size()) == 0) {
                        includeVector = true;
                        vectorInfo.Found = true;
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
                           splitName     = &vectorName[i + 1];
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
   unsigned int uniqueFoundPrefixes = 0;
   for(std::vector<VectorInfo>::iterator iterator = vectorsToExtract.begin();
       iterator != vectorsToExtract.end(); iterator++) {
      if(iterator->Found) {
         uniqueFoundPrefixes++;
      }
   }
   if( (uniqueFoundPrefixes < vectorsToExtract.size()) &&
       (vectorsToExtract.size() != 0) ) {
      std::cerr << "WARNING: Found only " << uniqueFoundPrefixes << " of "
                << vectorsToExtract.size() << " specified!\n";
   }

   return outputLine;
}



// ###### Version ###########################################################
[[ noreturn ]] static void version()
{
   std::cerr << "ExtractVectors" << " " << EXTRACTVECTORS_VERSION << "\n";
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
         "    [-s separator|--separator separator]\n"
         "    [-l|--line-numbers|-n|--no-line-numbers]\n"
         "    [-p|--split|-a|--no-split]\n"
         "    [-q|--quiet]\n"
         "* Version:\n  " << program << " [-v|--version]\n"
         "* Help:\n  "    << program << " [-h|--help]\n";
   exit(exitCode);
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   bool                    addLineNumbers      = false;
   unsigned int            compressionLevel    = 9;
   const char*             separator           = "\t";
   bool                    vectorSplittingMode = false;
   bool                    quietMode           = false;
   std::vector<VectorInfo> vectorsToExtract;


   // ====== Handle command-line arguments ==================================
   const static struct option long_options[] = {
      { "compress",        required_argument, 0, 'c' },
      { "separator",       required_argument, 0, 's' },
      { "line-numbers",    no_argument,       0, 'l' },
      { "no-line-numbers", no_argument,       0, 'n' },
      { "split",           no_argument,       0, 'p' },
      { "no-split",        no_argument,       0, 'a' },
      { "quiet",           no_argument,       0, 'q' },

      { "help",            no_argument,       0, 'h' },
      { "version",         no_argument,       0, 'v' },
      {  nullptr,          0,                 0, 0   }
   };

   int option;
   int longIndex;
   while( (option = getopt_long_only(argc, argv, "c:s:lnpaqhv", long_options, &longIndex)) != -1 ) {
      switch(option) {
         case 'c':
            compressionLevel = atol(optarg);
            if(compressionLevel < 1) {
               compressionLevel = 1;
            }
            else if(compressionLevel > 9) {
               compressionLevel = 9;
            }
          break;
         case 's':
            separator = optarg;
          break;
         case 'l':
            addLineNumbers = true;
          break;
         case 'n':
            addLineNumbers = false;
          break;
         case 'p':
            vectorSplittingMode = true;
          break;
         case 'a':
            vectorSplittingMode = false;
          break;
         case 'q':
            quietMode = true;
          break;
         case 'v':
            version();
          break;
         case 'h':
         case '?':
            // Exit with 0 on h/help, exit with 1 on '?' (unknown option):
            usage(argv[0], (option == 'h') ? 0 : 1);
          break;
         case '-':
          break;
         default:
            // This should not happen: wrong getopt parameters, or missing case?
            fprintf(stderr, "INTERNAL ERROR: Unhandled option c=%c code=%x!\n",
                    (isprint(option) ? (char)option : ' '), option);
            return 1;
          break;
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
   if( (inputFileName.size() >= 4) &&
       ( (inputFileName.substr(inputFileName.size() - 4) == ".bz2") ||
         (inputFileName.substr(inputFileName.size() - 4) == ".BZ2")) ) {
       inputFileFormat = IFF_BZip2;
   }
   if(inputFile.initialize(inputFileName.c_str(), inputFileFormat) == false) {
      exit(1);
   }

   OutputFile       outputFile;
   OutputFileFormat outputFileFormat = OFF_Plain;
   if( (outputFileName.size() >= 4) &&
       ( (outputFileName.substr(outputFileName.size() - 4) == ".bz2") ||
         (outputFileName.substr(outputFileName.size() - 4) == ".BZ2")) ) {
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
                     vectorSplittingMode, vectorsToExtract,
                     addLineNumbers, separator);


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
