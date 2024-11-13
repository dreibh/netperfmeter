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

#include <string.h>

#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>

#include "simpleredblacktree.h"
#include "inputfile.h"
#include "outputfile.h"


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


// ###### Remove scenario name  #############################################
static void removeScenarioName(char* str)
{
   char* s1 = index(str, '.');
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
   char*        segment = rindex(objectName, '.');
   if(segment == nullptr) {
      segment       = objectName;
      scalarName[0] = 0x00;
      aggNames[0]   = 0x00;
      aggValues[0]  = 0x00;
      levels        = 0;
   }
   else {
      segment[0] = 0x00;
      segment = (char*)&segment[1];
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
   for(i = length - 1;i >= 0;i--) {
      if(!isdigit(segment[i])) {
         break;
      }
   }
   char aggregate[i + 2];
   if(i >= 0) {
      strncpy((char*)&aggregate, segment, i + 1);
   }
   aggregate[i + 1] = 0x00;

   // ------ Extract aggregate and its value --------------
   if((size_t)i < length - 1) {
     if(aggNames[0] != 0x00) {
        safestrcat(aggNames, "\t", aggNamesSize);
     }
     if(scalarName[0] != 0x00) {
       safestrcat(aggNames, scalarName, aggNamesSize);
       safestrcat(aggNames, ".", aggNamesSize);
     }
     safestrcat(aggNames, aggregate, aggNamesSize);

     unsigned long value = atol((const char*)&segment[i + 1]);
     char valueString[16];
     snprintf((char*)&valueString, sizeof(valueString), "%lu", value);
     if(aggValues[0] != 0x00) {
        safestrcat(aggValues, "\t", aggValuesSize);
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
      char* identifier = index(statName, '#');
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
               safestrcat(aggNames, "\t", aggNamesSize);
            }
            safestrcat(aggNames, statName, aggNamesSize);

            if(aggValues[0] != 0x00) {
               safestrcat(aggValues, "\t", aggValuesSize);
            }
            safestrcat(aggValues, (const char*)&identifier[1], aggValuesSize);
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
          ((!quoted) && (str[n] != ' ') && (str[n] != '\t')) ) {
      if( (quoted == true) && (str[n] == 0x00) ) {
         return nullptr;
      }
      word[i++] = str[n++];
   }
   word[i] = 0x00;
   n++;

   return (char*)&str[n];
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
                         const bool         splitMode,
                         char*              objectName,
                         char*              statName,
                         const double       value)
{
/*
   std::cout << "Object=" << objectName << "\n";
   std::cout << "Statistic=" << statName << "\n";
   std::cout << "Value=" << value << "\n";
*/

   // ====== Try to split scalar name =======================================
   const char* splitName = "";
   if(splitMode) {
      for(int i = strlen(statName) - 1; i >= 0; i--) {
         if(statName[i] == ' ') {
            if(isdigit(statName[i + 1])) {
               splitName     = (const char*)&statName[i + 1];
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
                (char*)&scalarName, sizeof(scalarName),
                (char*)&aggNames, sizeof(aggNames),
                (char*)&aggValues, sizeof(aggValues));

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
                             const bool         splitAll)
{
   InputFile       inputFile;
   InputFileFormat inputFileFormat = IFF_Plain;

   // ====== Open input file ================================================
   if( (fileName.rfind(".bz2") == fileName.size() - 4) ||
       (fileName.rfind(".BZ2") == fileName.size() - 4) ) {
       inputFileFormat = IFF_BZip2;
   }
   if(inputFile.initialize(fileName.c_str(), inputFileFormat) == false) {
      return false;
   }


   // ====== Process input file =============================================
   double       value;
   bool         hasStatistic = false;
   char         buffer[4097];
   char         objectName[4096];
   char         statName[4096];
   char         fieldName[4096];
   char         statisticObjectName[4096];
   char         statisticBlockName[4096];
   unsigned int run      = 0;
   bool         success  = true;
   for(;;) {
      // ====== Read line from input file ===================================
      bool eof;
      const ssize_t bytesRead = inputFile.readLine((char*)&buffer, sizeof(buffer), eof);
      if((bytesRead < 0) || (eof)) {
         break;
      }

      // ====== Process line ================================================
      if( (!(strncmp(buffer, "scalar ",  7))) ||
          (!(strncmp(buffer, "scalar\t", 7))) ) {
         // ====== Parse scalar line ========================================
         char* s = getWord((char*)&buffer[7], (char*)&objectName);
         if(s != nullptr) {
            s = getWord(s, (char*)&statName);
            if(s != nullptr) {
               if(sscanf(s, "%lf", &value) != 1) {
                  std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
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
         removeScenarioName((char*)&objectName);
         handleScalar(varNames, varValues, run, interactiveMode, splitAll,
                      (char*)&objectName, (char*)&statName, value);
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
            char* s = getWord((char*)&buffer[6], (char*)&fieldName);
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
               snprintf((char*)&statName,   sizeof(statName),   "%s", newStatName.c_str());
               snprintf((char*)&objectName, sizeof(objectName), "%s", statisticObjectName);
               handleScalar(varNames, varValues, run, interactiveMode, splitAll,
                           (char*)&objectName, (char*)&statName, value);
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
         char* s = getWord((char*)&buffer[10], (char*)&statisticObjectName);
         if(s) {
            s = getWord(s, (char*)&statisticBlockName);
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
         removeScenarioName((char*)&statisticObjectName);
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
         if(outputFile.printf("RunNo\tValueNo\tSplit\t%s\t%s\t%s\n",
                              scalarNode->AggNames,
                              varNames.c_str(),
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
            if(outputFile.printf("%07llu\t", lineNumber) == false) {
               exit(1);
            }
         }
         if(outputFile.printf("%u\t%u\t\"%s\"\t%s\t%s\t%lf\n",
                              (unsigned int)scalarNode->Run,
                              (unsigned int)valueNumber,
                              scalarNode->SplitName,
                              scalarNode->AggValues,
                              scalarNode->VarValues,
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


// ###### Print usage and exit ##############################################
static void usage(const char* name)
{
   std::cerr << "Usage: " << name
             << " [Var Names] {-compress=0-9} {-interactive|-batch} {-splitall} {-line-numbers|-no-line-numbers}\n";
   exit(1);
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   unsigned int compressionLevel       = 9;
   bool         interactiveMode        = true;
   bool         addLineNumbers         = false;
   bool         splitAll               = false;
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

   simpleRedBlackTreeNew(&StatisticsStorage,
                         scalarNodePrintFunction,
                         scalarNodeComparisonFunction);


   // ====== Handle command-line arguments ==================================
   if(argc > 1) {
      varNames = argv[1];
      for(size_t i = 0; i < varNames.size(); i++) {
         if(varNames[i] == ' ') {
            varNames[i] = '\t';
         }
      }
      for(int i = 2;i < argc;i++) {
         if(!(strncmp(argv[i], "-compress=", 10))) {
            compressionLevel = atol((char*)&argv[i][10]);
            if(compressionLevel > 9) {
               compressionLevel = 9;
            }
         }
         else if(!(strcmp(argv[i], "-batch"))) {
            interactiveMode = false;
         }
         else if(!(strcmp(argv[i], "-interactive"))) {
            interactiveMode = true;
         }
         else if(!(strcmp(argv[i], "-line-numbers"))) {
            addLineNumbers = true;
         }
         else if(!(strcmp(argv[i], "-no-line-numbers"))) {
            addLineNumbers = false;
         }
         else if(!(strcmp(argv[i], "-splitall"))) {
            splitAll = true;
         }
         else if(!(strcmp(argv[i], "-quiet"))) {
            quietMode = true;
         }
         else if(!(strcmp(argv[i], "-ignore-scalar-file-errors"))) {
            ignoreScalarFileErrors = true;
         }
         else {
            usage(argv[0]);
         }
      }
   }
   else {
      usage(argv[0]);
   }


   if(!quietMode) {
      std::cout << "CreateSummary - Version 5.1.0\n"
           << "=============================\n\n"
           << "Compression Level: " << compressionLevel << "\n"
           << "Interactive Mode:  " << (interactiveMode ? "on" : "off") << "\n"
           << "Line Numbers:      " << (addLineNumbers  ? "on" : "off") << "\n"
           << "\n";
   }


   // ====== Handle interactiveMode commands ====================================
   if(interactiveMode) {
      std::cout << "Ready> ";
      std::cout.flush();
   }
   else {
      std::cout << "Processing input ...\n";
   }
   bool scalarFileError = false;
   while((command = fgets((char*)&buffer, sizeof(buffer), stdin))) {
      command[strlen(command) - 1] = 0x00;
      if(command[0] == 0x00) {
         std::cout << "*** End of File ***\n";
         break;
      }

      if(interactiveMode) {
         std::cout << command << "\n";
      }

      if(!(strncmp(command, "--varnames=", 11))) {
         varNames = (const char*)&command[11];
         if(varNames[0] == '\"') {
            varNames = varNames.substr(1, varNames.size() - 2);
         }
         for(size_t i = 0; i < varNames.size(); i++) {
            if(varNames[i] == ' ') {
               varNames[i] = '\t';
            }
         }
      }
      else if(!(strncmp(command, "--values=", 9))) {
         varValues = (const char*)&command[9];
         if(varValues[0] == '\"') {
            varValues = varValues.substr(1, varValues.size() - 2);
         }
         for(size_t i = 0; i < varValues.size(); i++) {
            if(varValues[i] == ' ') {
               varValues[i] = '\t';
            }
         }
      }
      else if(!(strncmp(command, "--logfile=", 10))) {
         logFileName = (const char*)&command[10];
      }
      else if(!(strncmp(command, "--statusfile=", 13))) {
         statusFileName = (const char*)&command[13];
      }
      else if(!(strncmp(command, "--input=", 8))) {
         if(varValues == "") {
            std::cerr << "ERROR: No values given (parameter --values=...)!\n";
            exit(1);
         }
         if(!handleScalarFile(varNames, varValues, (char*)&command[8], interactiveMode, splitAll)) {
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
      else if(!(strncmp(command, "--skip=", 7))) {
         SkipListNode* skipListNode = new SkipListNode;
         if(skipListNode == nullptr) {
            std::cerr << "ERROR: Out of memory!\n";
            exit(1);
         }
         skipListNode->Next = SkipList;
         snprintf((char*)&skipListNode->Prefix, sizeof(skipListNode->Prefix), "%s",
                  (const char*)&command[7]);
         SkipList = skipListNode;
      }
      else if(!(strncmp(command, "--simulationsdirectory=", 23))) {
         simulationsDirectory = (const char*)&command[23];
      }
      else if(!(strncmp(command, "--resultsdirectory=", 19))) {
         resultsDirectory = (const char*)&command[19];
      }
      else if(!(strcmp(command, "--splitall"))) {
         splitAll = true;
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
               compressionLevel, interactiveMode, addLineNumbers);


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
   return 0;
}
