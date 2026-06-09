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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <getopt.h>
#include <iostream>
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

#include "simpleredblacktree.h"
#include "inputfile.h"
#include "outputfile.h"
#include "package-version.h"


#define MAX_NAME_SIZE    256
#define MAX_VALUES_SIZE 8192


struct SkipListNode
{
   struct SkipListNode* Next;
   char                 Prefix[128];
};


class ScalarNode
{
   public:
   ScalarNode();
   ~ScalarNode();

   struct SimpleRedBlackTreeNode Node;
   char*                         ScalarName;
   char*                         SplitName;
   char*                         AggNames;
   char*                         AggValues;
   char*                         VarValues;
   size_t                        Run;
   size_t                        Entries;
   std::vector<double>           ValueSet;
};


// ###### Constructor #######################################################
ScalarNode::ScalarNode()
{
   Run        = 0;
   Entries    = 0;
   ScalarName = nullptr;
   SplitName  = nullptr;
   VarValues  = nullptr;
   AggNames   = nullptr;
   AggValues  = nullptr;
   simpleRedBlackTreeNodeNew(&Node);
}


// ###### Destructor ########################################################
ScalarNode::~ScalarNode()
{
   if(ScalarName) {
      free(ScalarName);
      ScalarName = nullptr;
   }
   if(SplitName) {
      free(SplitName);
      SplitName = nullptr;
   }
   if(VarValues) {
      free(VarValues);
      VarValues = nullptr;
   }
   if(AggNames) {
      free(AggNames);
      AggNames = nullptr;
   }
   if(AggValues) {
      free(AggValues);
      AggValues = nullptr;
   }
}


// ###### Get ScalarNode object from storage node ###########################
static inline ScalarNode* getScalarNodeFromStorageNode(struct SimpleRedBlackTreeNode* node)
{
   const long offset = (long)(&((class ScalarNode*)node)->Node) - (long)node;
   return  (class ScalarNode*)( (long)node - offset );
}


// ###### Print function ####################################################
static void scalarNodePrintFunction(const void* node, FILE* fd)
{
   ScalarNode* scalarNode = getScalarNodeFromStorageNode((struct SimpleRedBlackTreeNode*)node);
   fprintf(fd, "Run%u.%s|%s[%s].%s = { ",
           (unsigned int)scalarNode->Run, scalarNode->ScalarName, scalarNode->SplitName,
           scalarNode->AggValues, scalarNode->VarValues);

   std::vector<double>::iterator valueIterator = scalarNode->ValueSet.begin();
   while(valueIterator != scalarNode->ValueSet.end()) {
      fprintf(fd, "%f ", *valueIterator);
      valueIterator++;
   }

   fputs("}\n", fd);
}


// ###### Comparison function ###############################################
static int scalarNodeComparisonFunction(const void* node1, const void* node2)
{
   const ScalarNode* scalarNode1 = getScalarNodeFromStorageNode((struct SimpleRedBlackTreeNode*)node1);
   const ScalarNode* scalarNode2 = getScalarNodeFromStorageNode((struct SimpleRedBlackTreeNode*)node2);
   int result = strcmp(scalarNode1->ScalarName, scalarNode2->ScalarName);
   if(result != 0) {
      return result;
   }
   result = strcmp(scalarNode1->SplitName, scalarNode2->SplitName);
   if(result != 0) {
      return result;
   }
   result = strcmp(scalarNode1->AggValues, scalarNode2->AggValues);
   if(result != 0) {
      return result;
   }
   result = strcmp(scalarNode1->VarValues, scalarNode2->VarValues);
   if(result != 0) {
      return result;
   }
   if(scalarNode1->Run < scalarNode2->Run) {
      return -1;
   }
   if(scalarNode1->Run > scalarNode2->Run) {
      return 1;
   }
   return 0;
}



static struct SimpleRedBlackTree StatisticsStorage;
static ScalarNode*               NextScalarNode = nullptr;
static SkipListNode*             SkipList       = nullptr;



// ###### Remove slashes ####################################################
static void replaceSlashes(char* str)
{
   const size_t length = strlen(str);
   for(size_t i = 0;i < length;i++) {
      if(str[i] == '/') {
         str[i] = ':';
      }
   }
}


// ###### Remove spaces #####################################################
static void removeSpaces(char* str)
{
   const size_t length = strlen(str);
   size_t j = 0;
   for(size_t i = 0;i < length;i++) {
      if(str[i] != ' ') {
         str[j++] = str[i];
      }
   }
   str[j] = 0x00;
}


// ###### Remove square brackets ############################################
static void removeBrackets(char* str)
{
   const size_t length = strlen(str);
   size_t j = 0;
   for(size_t i = 0;i < length;i++) {
      if((str[i] != '[') && (str[i] != ']')) {
         str[j++] = str[i];
      }
   }
   str[j] = 0x00;
}


// ###### Check columns for proper quotation ################################
static bool checkColumns(const std::string& values)
{
   bool         inQuotation    = false;
   unsigned int quotationMarks = 0;
   for(size_t i = 0; i < values.size(); i++) {
      switch(values[i]) {
         case '\\':
            i++;
          break;
         case '"':
            inQuotation = !inQuotation;
            quotationMarks++;
          break;
      }
   }
   return ((quotationMarks & 1) == 0);
}


// ###### Remove scenario name  #############################################
static void removeScenarioName(char* str)
{
   char* s1 = strchr(str, '.');
   if(s1 != nullptr) {
      s1++;
      while(*s1 != 0x00) {
         *str++ = *s1++;
      }
      *str = 0x00;
   }
}


// ###### Length-checking strcat() ##########################################
static int safestrcat(char* dest, const char* src, const size_t size)
{
   const size_t l1 = strlen(dest);
   const size_t l2 = strlen(src);
   if (l1 >= size - 1) {
      return 0;
   }
   strncat(dest, src, size - l1 - 1);
   dest[size - 1] = 0x00;
   return l1 + l2 < size;
}


// ###### Get aggregate data ################################################
static unsigned int getAggregate(char*        objectName,
                                 char*        statName,
                                 char*        scalarName,
                                 const size_t scalarNameSize,
                                 char*        aggNames,
                                 const size_t aggNamesSize,
                                 char*        aggValues,
                                 const size_t aggValuesSize,
                                 const bool   topLevel = true)
{
   /*
      Example:
         objectName="configurableScenario.tsb[0].nameServerArray[0].poolElement"
         statName="Value#1"
      Hierarchy:
         => tsb -> 0
         => nameServerArray -> 0
         => poolElement
         => Value -> 1
      Results:
         scalarName="tsb.nameServerArray.poolElement-Value"
         aggNames="tsb tsb.nameServerArray Value"
         aggValues="0 0 1"
   */

   // ====== Get segment ====================================================
   unsigned int levels;
   char*        segment = strrchr(objectName, '.');
   if(segment == nullptr) {
      segment       = objectName;
      scalarName[0] = 0x00;
      aggNames[0]   = 0x00;
      aggValues[0]  = 0x00;
      levels        = 0;
   }
   else {
      segment[0] = 0x00;
      segment = &segment[1];
      // ====== Recursively process top segments ============================
      levels = getAggregate(objectName, statName, scalarName, scalarNameSize,
                            aggNames, aggNamesSize, aggValues, aggValuesSize,
                            false);
   }

   // ====== Process segment ================================================
   // printf("Segment=<%s>\n", segment);
   const size_t length = strlen(segment);

   // ------ Get object number (if it has one) ------------
   ssize_t i;
   for(i = (ssize_t)length - 1;i >= 0;i--) {
      if(!isdigit(segment[i])) {
         break;
      }
   }
   char aggregate[4096];
   assert((size_t)i + 1 < sizeof(aggregate));
   if(i >= 0) {
      strncpy(aggregate, segment, (size_t)i + 1);
   }
   aggregate[i + 1] = 0x00;

   // ------ Extract aggregate and its value --------------
   if (i < (ssize_t)length - 1) {
     if(aggNames[0] != 0x00) {
        safestrcat(aggNames, " ", aggNamesSize);
     }
     if(scalarName[0] != 0x00) {
       safestrcat(aggNames, scalarName, aggNamesSize);
       safestrcat(aggNames, ".", aggNamesSize);
     }
     safestrcat(aggNames, aggregate, aggNamesSize);

     unsigned long value = (unsigned long)atol(&segment[i + 1]);
     char valueString[32];
     snprintf(valueString, sizeof(valueString), "%lu", value);
     if(aggValues[0] != 0x00) {
        safestrcat(aggValues, " ", aggValuesSize);
     }
     safestrcat(aggValues, valueString, aggValuesSize);
   }

   // ------ Add aggregate (or object) to scalarName ------
   if(scalarName[0] != 0x00) {
      safestrcat(scalarName, ".", scalarNameSize);
   }
   safestrcat(scalarName, aggregate, scalarNameSize);

   // ------ Add scalar name to scalarName-----------------
   if(topLevel) {
      char* identifier = strchr(statName, '#');
      if(identifier) {
         const size_t identifierLength = strlen(identifier);
         for(size_t i = 1;i < identifierLength;i++) {
            if(!isdigit(identifier[i])) {
               // Identifier is not a number -> skip it!
               identifier = nullptr;
               break;
            }
         }
         if(identifier) {
            identifier[0] = 0x00;   // Cut number off from statName.

            if(aggNames[0] != 0x00) {
               safestrcat(aggNames, " ", aggNamesSize);
            }
            safestrcat(aggNames, statName, aggNamesSize);

            if(aggValues[0] != 0x00) {
               safestrcat(aggValues, " ", aggValuesSize);
            }
            safestrcat(aggValues, &identifier[1], aggValuesSize);
         }
      }

      safestrcat(scalarName, "-", scalarNameSize);
      safestrcat(scalarName, statName, scalarNameSize);
   }

   // printf("Level %u: scalarName=<%s> statName=<%s> aggNames=<%s> aggValues=<%s>\n",
   //        levels, scalarName, statName, aggNames, aggValues);

   return levels + 1;
}


// ###### Get text word #####################################################
static char* getWord(char* str, char* word)
{
   size_t n      = 0;
   bool   quoted = false;

   while( (str[n] == ' ') || (str[n] == '\t') ) {
      n++;
   }
   if(str[n] == 0x00) {
      return nullptr;
   }
   if(str[n] == '\"') {
      quoted = true;
      n++;
   }

   size_t i = 0;
   while( ((quoted) && (str[n] != '\"')) ||
          ((!quoted) && (str[n] != ' ') && (str[n] != '\t') && (str[n] != 0x00)) ) {
      if( (quoted == true) && (str[n] == 0x00) ) {
         return nullptr;
      }
      word[i++] = str[n++];
   }
   word[i] = 0x00;
   if (str[n] != 0x00) {
      n++;
   }
   return &str[n];
}


// ###### Add scalar to storage #############################################
static void addScalar(const char*  scalarName,
                      const char*  splitName,
                      const char*  aggNames,
                      const char*  aggValues,
                      const char*  varNames,
                      const char*  varValues,
                      const size_t runNumber,
                      const double value)
{
   // printf("addScalar: scalarName=<%s> splitName=<%s> aggN=<%s> aggV=<%s> varN=<%s> varV=<%s> run=%lu val=%f\n",
   //        scalarName, splitName, aggNames, aggValues, varNames, varValues, runNumber, value);

   if(NextScalarNode == nullptr) {
      NextScalarNode = new ScalarNode;
      if(NextScalarNode == nullptr) {
         std::cerr << "ERROR: Out of memory!\n";
         exit(1);
      }
   }

   NextScalarNode->Run        = runNumber;
   NextScalarNode->ScalarName = (char*)scalarName;   // WARNING: String MUST be duplicated when node is added!
   NextScalarNode->SplitName  = (char*)splitName;    // WARNING: String MUST be duplicated when node is added!
   NextScalarNode->AggNames   = (char*)aggNames;     // WARNING: String MUST be duplicated when node is added!
   NextScalarNode->AggValues  = (char*)aggValues;    // WARNING: String MUST be duplicated when node is added!
   NextScalarNode->VarValues  = (char*)varValues;    // WARNING: String MUST be duplicated when node is added!

   ScalarNode* scalarNode = (ScalarNode*)
      simpleRedBlackTreeInsert(&StatisticsStorage, &NextScalarNode->Node);
   if(scalarNode == NextScalarNode) {
      // ====== Duplicate string, so that no temporary copies are linked ====
      NextScalarNode->ScalarName = strdup(scalarName);
      NextScalarNode->SplitName  = strdup(splitName);
      NextScalarNode->AggNames   = strdup(aggNames);
      NextScalarNode->AggValues  = strdup(aggValues);
      NextScalarNode->VarValues  = strdup(varValues);
      if( (NextScalarNode->ScalarName == nullptr) ||
          (NextScalarNode->SplitName  == nullptr) ||
          (NextScalarNode->AggNames   == nullptr) ||
          (NextScalarNode->AggValues  == nullptr) ||
          (NextScalarNode->VarValues  == nullptr) ) {
         std::cerr << "ERROR: Out of memory!\n";
         exit(1);
      }
      NextScalarNode = nullptr;
   }
   else {
      NextScalarNode->ScalarName = nullptr;
      NextScalarNode->SplitName  = nullptr;
      NextScalarNode->AggNames   = nullptr;
      NextScalarNode->AggValues  = nullptr;
      NextScalarNode->VarValues  = nullptr;
   }

   scalarNode->ValueSet.push_back(value);
   scalarNode->Entries++;
}


// ###### Handle scalar #####################################################
static void handleScalar(const std::string& varNames,
                         const std::string& varValues,
                         const unsigned int run,
                         const bool         interactiveMode,
                         const bool         scalarSplittingMode,
                         char*              objectName,
                         char*              statName,
                         const double       value)
{
/*
   std::cout << "Object="    << objectName << "\n";
   std::cout << "Statistic=" << statName   << "\n";
   std::cout << "Value="     << value      << "\n";
*/

   // ====== Try to split scalar name =======================================
   const char* splitName = "";
   if(scalarSplittingMode) {
      for(int i = strlen(statName) - 1; i >= 0; i--) {
         if(statName[i] == ' ') {
            if(isdigit(statName[i + 1])) {
               splitName   = &statName[i + 1];
               statName[i] = 0x00;
            }
            break;
         }
      }
   }

   // ====== Get scalar name ================================================
   removeSpaces(statName);
   replaceSlashes(statName);
   removeBrackets(objectName);

   char scalarName[4096];
   char aggNames[MAX_NAME_SIZE];
   char aggValues[MAX_VALUES_SIZE];
   getAggregate(objectName, statName,
                scalarName, sizeof(scalarName),
                aggNames, sizeof(aggNames),
                aggValues, sizeof(aggValues));

   // ====== Reconciliate with skip list ====================================
   SkipListNode* skipListNode = SkipList;
   while(skipListNode != nullptr) {
      if(strncmp(scalarName, skipListNode->Prefix,
                 strlen(skipListNode->Prefix)) == 0) {
         break;
      }
      skipListNode = skipListNode->Next;;
   }
   if(skipListNode == nullptr) {
      addScalar(scalarName, splitName, aggNames, aggValues,
                varNames.c_str(), varValues.c_str(), run, value);
   }
   else {
      if(interactiveMode) {
         std::cout << "Skipping entry " << scalarName << "\n";
      }
   }
}


// ###### Read and process scalar file ######################################
static bool handleScalarFile(const std::string& varNames,
                             const std::string& varValues,
                             const std::string& fileName,
                             const bool         interactiveMode,
                             const bool         scalarSplitting)
{
   InputFile       inputFile;
   InputFileFormat inputFileFormat = IFF_Plain;

   // ====== Open input file ================================================
   if( (fileName.size() >= 4) &&
       ( (fileName.substr(fileName.size() - 4) == ".bz2") ||
         (fileName.substr(fileName.size() - 4) == ".BZ2")) ) {
       inputFileFormat = IFF_BZip2;
   }
   if(inputFile.initialize(fileName.c_str(), inputFileFormat) == false) {
      return false;
   }


   // ====== Process input file =============================================
   double       value;
   char         buffer[4096];
   char         objectName[4096];
   char         statName[4096];
   char         fieldName[4096];
   char         statisticObjectName[4096];
   char         statisticBlockName[4096];
   bool         hasStatistic = false;
   unsigned int run          = 0;
   bool         success      = true;
   memset(statName, 0, sizeof(statName));
   for(;;) {
      // ====== Read line from input file ===================================
      bool eof;
      const ssize_t bytesRead = inputFile.readLine(buffer, sizeof(buffer), eof);
      if((bytesRead < 0) || (eof)) {
         break;
      }

      // ====== Process line ================================================
      if( (!(strncmp(buffer, "scalar ",  7))) ||
          (!(strncmp(buffer, "scalar\t", 7))) ) {
         // ====== Parse scalar line ========================================
         char* s = getWord(&buffer[7], objectName);
         if(s != nullptr) {
            s = getWord(s, statName);
            if(s != nullptr) {
               if(sscanf(s, "%lf", &value) != 1) {
                  std::cerr << "ERROR: File \"" << fileName << "\", line "
                            << inputFile.getLine()
                            << " - Value expected!\n";
                  success = false;
                  break;
               }
            }
            else {
               std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                         << " - Statistics name expected for \"scalar\"!\n";
               success = false;
               break;
            }
         }
         else {
            std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                      << " - Object name expected for \"scalar\"!\n";
            success = false;
            break;
         }
         removeScenarioName(objectName);
         handleScalar(varNames, varValues, run, interactiveMode, scalarSplitting,
                      objectName, statName, value);
      }
      else if(buffer[0] == '#') {
      }
      else if( (!(strncmp(buffer, "bin", 3))) ||
               (!(strncmp(buffer, "par", 3))) ||
               (!(strncmp(buffer, "attr", 4))) ||
               (!(strncmp(buffer, "config", 6))) ) {
         // Skip this item
      }
      else if(!(strncmp(buffer, "run", 3))) {
         run++;
      }
      else if(!(strncmp(buffer, "field ", 6))) {
         if(hasStatistic) {
            char* s = getWord(&buffer[6], fieldName);
            if(s) {
               if(sscanf(s, "%lf", &value) != 1) {
                  std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                            << " - Value expected!\n";
                  success = false;
                  break;
               }
               std::string newStatName = fieldName;
               if(newStatName == "stddev") {
                  newStatName = "StdDev";
               }
               else if(newStatName == "sqrsum") {
                  newStatName = "SqrSum";
               }
               else {
                  newStatName[0] = toupper(newStatName[0]);
               }
               newStatName = newStatName + statisticBlockName;
               // handleScalar() will overwrite the fields object/stat => make copies first!
               snprintf(statName,   sizeof(statName),   "%s", newStatName.c_str());
               snprintf(objectName, sizeof(objectName), "%s", statisticObjectName);
               handleScalar(varNames, varValues, run, interactiveMode, scalarSplitting,
                           objectName, statName, value);
            }
         }
         else {
            std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                      << " - \"field\" without \"statistic\"!\n";
            success = false;
            break;
         }
      }
      else if(!(strncmp(buffer, "statistic ", 10))) {
         // ====== Parse scalar line ========================================
         char* s = getWord(&buffer[10], statisticObjectName);
         if(s) {
            s = getWord(s, statisticBlockName);
            if(s == nullptr) {
               std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                         << " - Statistics name expected for \"statistic\"!\n";
               success = false;
               break;
            }
         }
         else {
            std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                      << " - Object name expected for \"statistic\"!\n";
            success = false;
            break;
         }
         removeScenarioName(statisticObjectName);
         hasStatistic = true;
      }
      else if(buffer[0] == 0x00) {
         // Empty line
      }
      else if(!(strncmp(buffer, "version", 7))) {
         // Skip this item
      }
      else {
         std::cerr << "NOTE: " << fileName << ":" << inputFile.getLine()
                   << " - Ignoring line \"" << buffer << "\"\n";
         // Skip this item
      }
   }


   // ====== Close input file ===============================================
   inputFile.finish();
   return success;
}


// ###### Close output file #################################################
static void closeOutputFile(OutputFile&              outputFile,
                            const std::string&       outputFileName,
                            const unsigned long long lineNumber,
                            unsigned long long&      totalIn,
                            unsigned long long&      totalOut,
                            unsigned long long&      totalLines,
                            unsigned int&            totalFiles,
                            const bool               interactiveMode)
{
   if(outputFile.exists()) {
      unsigned long long in, out;
      if(!outputFile.finish(true, &in, &out)) {
         exit(1);
      }
      totalIn    += in;
      totalOut   += out;
      totalLines += lineNumber;
      totalFiles++;
      if(interactiveMode) {
         std::cout << " (" << lineNumber << " lines";
         if(in > 0) {
            std::cout << ", " << in << " -> " << out << " - "
                      << ((double)out * 100.0 / in) << "%";
         }
         std::cout << ")\n";
      }
   }
}


// ###### Dump scalars to output files ######################################
static void dumpScalars(const std::string& simulationsDirectory,
                        const std::string& resultsDirectory,
                        const std::string& varNames,
                        const unsigned int compressionLevel,
                        const bool         interactiveMode,
                        const char*        separator,
                        const bool         addLineNumbers)
{
   std::string        fileName           = "";
   std::string        lastStatisticsName = "";
   unsigned long long totalIn            = 0;
   unsigned long long totalOut           = 0;
   unsigned long long totalLines         = 0;
   unsigned int       totalFiles         = 0;
   unsigned long long lineNumber         = 0;
   OutputFile         outputFile;

   // simpleRedBlackTreePrint(&StatisticsStorage, stdout);

   SimpleRedBlackTreeNode* node = simpleRedBlackTreeGetFirst(&StatisticsStorage);
   while(node != nullptr) {
      ScalarNode* scalarNode = getScalarNodeFromStorageNode(node);

      if(strcmp(lastStatisticsName.c_str(), scalarNode->ScalarName) != 0) {
         // ====== Close output file ========================================
         closeOutputFile(outputFile, fileName, lineNumber,
                         totalIn, totalOut, totalLines, totalFiles,
                         interactiveMode);

         // ====== Open output file =========================================
         if(compressionLevel > 0) {
            fileName = resultsDirectory + "/" + scalarNode->ScalarName + ".data";
            unlink(fileName.c_str());
            fileName += ".bz2";
         }
         else {
            fileName = resultsDirectory + "/" + scalarNode->ScalarName + ".data.bz2";
            unlink(fileName.c_str());
            fileName = resultsDirectory + "/" + scalarNode->ScalarName + ".data";
         }
         if(interactiveMode) {
            std::cout << "Statistics \"" << scalarNode->ScalarName << "\" ...";
         }
         std::cout.flush();
         if(outputFile.initialize(fileName.c_str(),
                                  (compressionLevel > 0) ? OFF_BZip2 : OFF_Plain,
                                  compressionLevel) == false) {
            exit(1);
         }
         lineNumber = 1;

         // ====== Write table header =======================================
         if(outputFile.printf("RunNo%sValueNo%sSplit%s%s%s%s%s%s\n",
                              separator, separator, separator,
                              scalarNode->AggNames, separator,
                              varNames.c_str(),     separator,
                              scalarNode->ScalarName) == false) {
            exit(1);
         }
         lastStatisticsName = scalarNode->ScalarName;
      }


      // ====== Write table rows =========================================
      size_t valueNumber = 1;
      std::vector<double>::iterator valueIterator = scalarNode->ValueSet.begin();
      while(valueIterator != scalarNode->ValueSet.end()) {
         if(addLineNumbers) {
            if(outputFile.printf("%llu%s", lineNumber, separator) == false) {
               exit(1);
            }
         }
         if(outputFile.printf("%u%s%u%s\"%s\"%s%s%s%s%s%lf\n",
                              (unsigned int)scalarNode->Run, separator,
                              (unsigned int)valueNumber,     separator,
                              scalarNode->SplitName,         separator,
                              scalarNode->AggValues,         separator,
                              scalarNode->VarValues,         separator,
                              *valueIterator) == false) {
            exit(1);
         }
         valueNumber++;
         lineNumber++;
         valueIterator++;
      }

      node = simpleRedBlackTreeGetNext(&StatisticsStorage, node);
   }

   // ====== Close last output file =========================================
   closeOutputFile(outputFile, fileName, lineNumber,
                     totalIn, totalOut, totalLines, totalFiles,
                     interactiveMode);

   // ====== Display some compression information ===========================
   std::cout << "Wrote " << totalLines << " lines into "
        << totalFiles << " files";
   if(totalIn > 0) {
      std::cout << ", "
                << totalIn << " -> " << totalOut << " - "
                << ((double)totalOut * 100.0 / totalIn) << "%";
   }
   std::cout << "\n";
}


// ###### Version ###########################################################
[[ noreturn ]] static void version()
{
   std::cerr << "CreateSummary" << " " << CREATESUMMARY_VERSION << "\n";
   exit(0);
}


// ###### Usage #############################################################
[[ noreturn ]] static void usage(const char* program, const int exitCode)
{
   std::cerr << "Usage:\n"
      << "* Run:\n  "
      << program << "\n"
         "    [variable_names]\n"
         "    [-b|--batch|-i|--interactive]\n"
         "    [-c level|--compress level]\n"
         "    [-s separator|--separator separator]\n"
         "    [-l|--line-numbers|-n|--no-line-numbers]\n"
         "    [-p|--split|-a|--no-split]\n"
         "    [-r|--ignore-scalar-file-errors]\n"
         "    [-q|--quiet]\n"
         "* Version:\n  " << program << " [-v|--version]\n"
         "* Help:\n  "    << program << " [-h|--help]\n";
   exit(exitCode);
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   unsigned int compressionLevel       = 9;
   const char*  separator              = "\t";
   bool         interactiveMode        = true;
   bool         addLineNumbers         = false;
   bool         scalarSplittingMode    = false;
   bool         quietMode              = false;
   bool         ignoreScalarFileErrors = false;
   std::string  varNames               = "_NoVarNamesGiven_";
   std::string  varValues              = "";
   std::string  simulationsDirectory   = ".";
   std::string  resultsDirectory       = ".";
   std::string  logFileName            = "";
   std::string  statusFileName         = "";
   char         buffer[4096];
   char*        command;

   // ====== Handle command-line arguments ==================================
   const static struct option long_options[] = {
      { "interactive",               no_argument,       0, 'i' },
      { "batch",                     no_argument,       0, 'b' },

      { "compress",                  required_argument, 0, 'c' },
      { "separator",                 required_argument, 0, 's' },
      { "line-numbers",              no_argument,       0, 'l' },
      { "no-line-numbers",           no_argument,       0, 'n' },
      { "split",                     no_argument,       0, 'p' },
      { "no-split",                  no_argument,       0, 'a' },
      { "ignore-scalar-file-errors", no_argument,       0, 'r' },
      { "quiet",                     no_argument,       0, 'q' },

      { "help",                      no_argument,       0, 'h' },
      { "version",                   no_argument,       0, 'v' },
      {  nullptr,                    0,                 0, 0   }
   };

   int option;
   int longIndex;
   while( (option = getopt_long_only(argc, argv, "ibcs:lnpt:rqhv", long_options, &longIndex)) != -1 ) {
      switch(option) {
         case 'i':
            interactiveMode = true;
          break;
         case 'b':
            interactiveMode = false;
          break;
         case 'c':
            {
               const int parsedCompressionLevel = atoi(optarg);
               if(parsedCompressionLevel < 0) {
                  compressionLevel = 0;
               }
               else if(parsedCompressionLevel > 9) {
                  compressionLevel = 9;
               }
               else {
                  compressionLevel = (unsigned int)parsedCompressionLevel;
               }
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
            scalarSplittingMode = true;
          break;
         case 'a':
            scalarSplittingMode = false;
          break;
         case 'r':
            ignoreScalarFileErrors = true;
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

   if(optind < argc) {
      varNames = argv[optind++];
      if(!checkColumns(varNames)) {
         std::cerr << "ERROR: Invalid quotation in variable names string!\n";
         exit(1);
      }
   }
   if(optind < argc) {
      std::cerr << "ERROR: Invalid option " << argv[optind] << "!\n";
      usage(argv[0], 1);
   }

   // ====== Print information ==============================================
   if(!quietMode) {
      std::cout << "CreateSummary " << CREATESUMMARY_VERSION << "\n"
                << "* Interactive Mode:  " << (interactiveMode      ? "on" : "off") << "\n"
                << "* Separator:         \"" << separator << "\"\n"
                << "* Compression Level: " << compressionLevel << "\n"
                << "* Line Numbers:      " << (addLineNumbers       ? "on" : "off") << "\n"
                << "* Scalar Splitting:  " << (scalarSplittingMode  ? "on" : "off") << "\n"
                << "\n";
   }


   // ====== Handle interactiveMode commands ====================================
   simpleRedBlackTreeNew(&StatisticsStorage,
                         scalarNodePrintFunction,
                         scalarNodeComparisonFunction);

   if(interactiveMode) {
      std::cout << "Ready> ";
      std::cout.flush();
   }
   else {
      std::cout << "Processing input ...\n";
   }
   bool scalarFileError = false;
   while((command = fgets(buffer, sizeof(buffer), stdin))) {
      size_t length = strlen(command);
      if( (length > 0) && (command[length - 1] == '\n') ) {
         command[length - 1] = 0x00;
         length--;
      }

      if(length == 0) {
         if(interactiveMode) {
            std::cout << "Ready> ";
            std::cout.flush();
         }
         continue;
      }

      if(!(strncmp(command, "--values=", 9))) {
         varValues = &command[9];
         if( (varValues.size() >= 2)    &&
             (varValues.front() == '"') &&
             (varValues.back() == '"') ) {
            varValues = varValues.substr(1, varValues.size() - 2);
         }
         if(!checkColumns(varValues)) {
            std::cerr << "ERROR: Invalid quotation in variable values string!\n";
            exit(1);
         }
      }
      else if(!(strncmp(command, "--input=", 8))) {
         if(varValues == "") {
            std::cerr << "ERROR: No values given (parameter --values=...)!\n";
            exit(1);
         }
         if(!handleScalarFile(varNames, varValues,
                              simulationsDirectory + "/" + &command[8],
                              interactiveMode, scalarSplittingMode)) {
            scalarFileError = true;
            if(logFileName != "") {
               std::cerr << " => see logfile " << logFileName << "\n";
            }
            if(statusFileName != "") {
               std::cerr << " Removing status file; restart simulation to re-create this run!\n";
               unlink(statusFileName.c_str());
            }
         }
         varValues      = "";
         logFileName    = "";
         statusFileName = "";
      }
      else if(!(strncmp(command, "--varnames=", 11))) {
         varNames = &command[11];
         if( (varNames.size() >= 2)    &&
             (varNames.front() == '"') &&
             (varNames.back() == '"') ) {
            varNames = varNames.substr(1, varNames.size() - 2);
         }
         if(!checkColumns(varNames)) {
            std::cerr << "ERROR: Invalid quotation in variable names string!\n";
            exit(1);
         }
      }
      else if(!(strncmp(command, "--logfile=", 10))) {
         logFileName = &command[10];
      }
      else if(!(strncmp(command, "--statusfile=", 13))) {
         statusFileName = &command[13];
      }
      else if(!(strncmp(command, "--skip=", 7))) {
         SkipListNode* skipListNode = new SkipListNode;
         if(skipListNode == nullptr) {
            std::cerr << "ERROR: Out of memory!\n";
            exit(1);
         }
         skipListNode->Next = SkipList;
         snprintf(skipListNode->Prefix, sizeof(skipListNode->Prefix), "%s",
                  &command[7]);
         SkipList = skipListNode;
      }
      else if(!(strncmp(command, "--simulationsdirectory=", 23))) {
         simulationsDirectory = &command[23];
      }
      else if(!(strncmp(command, "--resultsdirectory=", 19))) {
         resultsDirectory = &command[19];
      }
      else if(!(strcmp(command, "--splitall"))) {
         scalarSplittingMode = true;
      }
      else if(!(strcmp(command, "--ignore-scalar-file-errors"))) {
         ignoreScalarFileErrors = true;
      }
      else if(!(strncmp(command, "--level=", 8))) {
         // Deprecated, ignore ...
      }
      else {
         std::cerr << "ERROR: Invalid command \"" << command << "\"!\n"
              << "Examples:\n"
              << "   --skip=calcAppServer\n"
              << "   --simulationsdirectory=MySimulationsDirectory\n"
              << "   --resultsdirectory=MySimulationsDirectory/Results\n"
              << "   --skip=calcAppServer\n"
              << "   --varnames=Alpha Beta Gamma\n"
              << "   --values=alpha100 beta200 gamma300\n"
              << "   --input=simulations/scalar-file.sca.bz2\n"
              << "   (Ctrl-D, EOF)\n";
         if(!interactiveMode) {
            exit(1);
         }
      }

      if(interactiveMode) {
         std::cout << "Ready> ";
         std::cout.flush();
      }
   }


   // ====== Write results ==================================================
   if(interactiveMode) {
      std::cout << "\n" << "\n";
   }
   if(scalarFileError) {
      if(ignoreScalarFileErrors == false) {
         std::cerr << "ERROR: Not all scalar files have been read -> aborting!\n";
         exit(1);
      }
      else {
         std::cerr << "WARNING: Not all scalar files have been read -> continuing!\n";
      }
   }
   std::cout << "Writing scalar files...\n";
   dumpScalars(simulationsDirectory, resultsDirectory, varNames,
               compressionLevel, interactiveMode, separator, addLineNumbers);


   // ====== Clean up =======================================================
   if(NextScalarNode) {
      delete NextScalarNode;
      NextScalarNode = nullptr;
   }
   ScalarNode* scalarNode = (ScalarNode*)simpleRedBlackTreeGetFirst(&StatisticsStorage);
   while(scalarNode) {
      simpleRedBlackTreeRemove(&StatisticsStorage, &scalarNode->Node);
      delete scalarNode;
      scalarNode = (ScalarNode*)simpleRedBlackTreeGetFirst(&StatisticsStorage);
   }
   simpleRedBlackTreeDelete(&StatisticsStorage);

   SkipListNode* currentSkipNode = SkipList;
   while(currentSkipNode != nullptr) {
      SkipListNode* nextSkipNode = currentSkipNode->Next;
      delete currentSkipNode;
      currentSkipNode = nextSkipNode;
   }
   SkipList = nullptr;

   return 0;
}
