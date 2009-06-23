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

#include <iostream>
#include <vector>
#include <bzlib.h>
#include <string.h>
#include "simpleredblacktree.h"


using namespace std;


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
   char*                         AggNames;
   char*                         AggValues;
   char*                         VarValues;
   size_t                        Run;
   size_t                        Entries;
   vector<double>                ValueSet;
};


// ###### Constructor #######################################################
ScalarNode::ScalarNode()
{
   Run        = 0;
   Entries    = 0;
   ScalarName = NULL;
   VarValues  = NULL;
   AggNames   = NULL;
   AggValues  = NULL;
   simpleRedBlackTreeNodeNew(&Node);
}


// ###### Destructor ########################################################
ScalarNode::~ScalarNode()
{
   if(ScalarName) {
      free(ScalarName);
   }
   if(VarValues) {
      free(VarValues);
   }
   if(AggNames) {
      free(AggNames);
   }
   if(AggValues) {
      free(AggValues);
   }
}


// ###### Get ScalarNode object from storage node ###########################
static inline ScalarNode* getScalarNodeFromStorageNode(struct SimpleRedBlackTreeNode* node)
{
   const long offset = (long)(&((struct ScalarNode*)node)->Node) - (long)node;
   return( (struct ScalarNode*)( (long)node - offset ));
}


// ###### Print function ####################################################
static void scalarNodePrintFunction(const void* node, FILE* fd)
{
   ScalarNode* scalarNode = getScalarNodeFromStorageNode((struct SimpleRedBlackTreeNode*)node);
   fprintf(fd, "Run%u.%s[%s].%s = { ",
           (unsigned int)scalarNode->Run, scalarNode->ScalarName, scalarNode->AggValues, scalarNode->VarValues);

   vector<double>::iterator valueIterator = scalarNode->ValueSet.begin();
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
      return(result);
   }
   result = strcmp(scalarNode1->AggValues, scalarNode2->AggValues);
   if(result != 0) {
      return(result);
   }
   result = strcmp(scalarNode1->VarValues, scalarNode2->VarValues);
   if(result != 0) {
      return(result);
   }
   if(scalarNode1->Run < scalarNode2->Run) {
      return(-1);
   }
   if(scalarNode1->Run > scalarNode2->Run) {
      return(1);
   }
   return(0);
}



static struct SimpleRedBlackTree StatisticsStorage;
static ScalarNode*                   NextScalarNode = NULL;
static SkipListNode*                 SkipList       = NULL;



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
   if(s1 != NULL) {
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
   return(l1 + l2 < size);
}


// ###### Get aggregate data ################################################
static unsigned int getAggregate(char*        str,
                                 char*        objectName,
                                 const size_t objectNameSize,
                                 char*        aggNames,
                                 const size_t aggNamesSize,
                                 char*        aggValues,
                                 const size_t aggValuesSize)
{
   unsigned int levels;

   // ====== Get segment ====================================================
   char* segment = rindex(str, '.');
   if(segment == NULL) {
      segment       = str;
      objectName[0] = 0x00;
      aggNames[0]   = 0x00;
      aggValues[0]  = 0x00;
      levels        = 0;
   }
   else {
      segment[0] = 0x00;
      segment = (char*)&segment[1];
      // ====== Recursively process top segments ============================
      levels = getAggregate(str, objectName, objectNameSize,
                            aggNames, aggNamesSize, aggValues, aggValuesSize);
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
   strncpy((char*)&aggregate, segment, i + 1);
   aggregate[i + 1] = 0x00;

   // ------ Build object hierarchy (if necessary) --------
   if((size_t)i < length - 1) {
     if(aggNames[0] != 0x00) {
        safestrcat(aggNames, " ", aggNamesSize);
     }
     if(objectName[0] != 0x00) {
       safestrcat(aggNames, objectName, aggNamesSize);
       safestrcat(aggNames, ".", aggNamesSize);
     }
     safestrcat(aggNames, aggregate, aggNamesSize);

     unsigned long value = atol((const char*)&segment[i + 1]);
     char valueString[16];
     snprintf((char*)&valueString, sizeof(valueString), "%04lu", value);
     if(aggValues[0] != 0x00) {
        safestrcat(aggValues, " ", aggValuesSize);
     }
     safestrcat(aggValues, valueString, aggValuesSize);
   }

   // ------ Add object to object name --------------------
   if(objectName[0] != 0x00) {
      safestrcat(objectName, ".", objectNameSize);
   }
   safestrcat(objectName, aggregate, objectNameSize);
   return(levels + 1);
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
      return(NULL);
   }
   if(str[n] == '\"') {
      quoted = true;
      n++;
   }

   size_t i = 0;
   while( ((quoted) && (str[n] != '\"')) ||
          ((!quoted) && (str[n] != ' ') && (str[n] != '\t')) ) {
      if(str[n] == 0x00) {
         return(NULL);
      }
      word[i++] = str[n++];
   }
   word[i] = 0x00;
   n++;

   return((char*)&str[n]);
}


// ###### Add scalar to storage #############################################
static void addScalar(const char*  scalarName,
                      const char*  aggNames,
                      const char*  aggValues,
                      const char*  varNames,
                      const char*  varValues,
                      const size_t runNumber,
                      const double value)
{
   // printf("addScalar: statName=<%s> aggN=<%s> aggV=<%s> varN=<%s> varV=<%s> run=%d val=%f\n",
   //        scalarName, aggNames, aggValues, varNames, varValues, runNumber, value);

   if(NextScalarNode == NULL) {
      NextScalarNode = new ScalarNode;
      if(NextScalarNode == NULL) {
         cerr << "ERROR: Out of memory!" << endl;
         exit(1);
      }
   }

   NextScalarNode->Run = runNumber;
   NextScalarNode->ScalarName = strdup(scalarName);
   NextScalarNode->AggNames   = strdup(aggNames);
   NextScalarNode->AggValues  = strdup(aggValues);
   NextScalarNode->VarValues  = strdup(varValues);
   if( (NextScalarNode->ScalarName == NULL) ||
       (NextScalarNode->AggNames == NULL) ||
       (NextScalarNode->AggValues == NULL) ||
       (NextScalarNode->VarValues == NULL) ) {
      cerr << "ERROR: Out of memory!" << endl;
      exit(1);
   }

   ScalarNode* scalarNode = (ScalarNode*)
      simpleRedBlackTreeInsert(&StatisticsStorage, &NextScalarNode->Node);
   if(scalarNode == NextScalarNode) {
      NextScalarNode = NULL;
   }

   scalarNode->ValueSet.push_back(value);
   scalarNode->Entries++;
}


// ###### Read and process scalar file ######################################
static bool handleScalarFile(const char* varNames,
                             const char* varValues,
                             const char* fileName,
                             const bool  interactive)
{
   FILE* inFile = fopen(fileName, "r");
   if(inFile == NULL) {
      cerr << "ERROR: Unable to open scalar file \"" << fileName << "\"!" << endl;
      return(false);
   }

   size_t fileNameLength = strlen(fileName);
   int bzerror;
   BZFILE* inBZFile = NULL;
   if((fileNameLength > 4) &&
      (fileName[fileNameLength - 4] == '.') &&
      (toupper(fileName[fileNameLength - 3]) == 'B') &&
      (toupper(fileName[fileNameLength - 2]) == 'Z') &&
      (fileName[fileNameLength - 1] == '2')) {
      inBZFile = BZ2_bzReadOpen(&bzerror, inFile, 0, 0, NULL, 0);
      if(bzerror != BZ_OK) {
         cerr << "ERROR: Unable to initialize BZip2 decompression on file <" << fileName << ">!" << endl;
         fclose(inFile);
         return(false);
      }
   }

   unsigned int line;
   unsigned int run;
   double       value;
   size_t       i;
   size_t       bytesRead;
   size_t       storageSize = 0;
   char         storage[4096];
   char         buffer[4097];
   char         objectName[4096];
   char         statName[4096];

   run  = 0;
   line = 0;
   bool success = true;
   for(;;) {
      memcpy((char*)&buffer, storage, storageSize);
      if(inBZFile) {
         bytesRead = BZ2_bzRead(&bzerror, inBZFile, (char*)&buffer[storageSize], sizeof(buffer) - storageSize);
      }
      else {
         bytesRead = fread((char*)&buffer[storageSize], 1, sizeof(buffer) - storageSize, inFile);
      }
      if(bytesRead <= 0) {
         bytesRead = 0;
      }
      bytesRead += storageSize;
      buffer[bytesRead] = 0x00;

      if(bytesRead == 0) {
         break;
      }

      storageSize = 0;
      for(i = 0;i < bytesRead;i++) {
         if(buffer[i] == '\n') {
            storageSize = bytesRead - i - 1;
            memcpy((char*)&storage, &buffer[i + 1], storageSize);
            buffer[i] = 0x00;
            break;
         }
      }

      line++;

      if(buffer[0] == '#') {
      }
      else if(!(strncmp(buffer, "run ", 4))) {
         run++;
      }
      else if(!(strncmp(buffer, "attr ", 5))) {
         // Skip this item
      }
      else if(!(strncmp(buffer, "version ", 8))) {
         // Skip this item
      }
      else if(!(strncmp(buffer, "scalar ", 7))) {
         // ====== Parse scalar line ========================================
         char* s = getWord((char*)&buffer[7], (char*)&objectName);
         if(s) {
            s = getWord(s, (char*)&statName);
            if(s) {
               if(sscanf(s, "%lf", &value) != 1) {
                  cerr << "ERROR: File \"" << fileName << "\", line " << line << " - Value expected!" << endl;
                  success = false;
                  break;
               }
            }
            else {
               cerr << "ERROR: File \"" << fileName << "\", line " << line << " - Statistics name expected!" << endl;
               success = false;
               break;
            }
         }
         else {
            cerr << "ERROR: File \"" << fileName << "\", line " << line << " - Object name expected!" << endl;
            success = false;
            break;
         }
         removeScenarioName((char*)&objectName);

/*
         cout << "Object=" << objectName << endl;
         cout << "Statistic=" << statName << endl;
         cout << "Value=" << value << endl;
*/

         // ====== Get scalar name ==========================================
         removeSpaces((char*)&statName);
         replaceSlashes((char*)&statName);
         removeBrackets((char*)&objectName);

         char scalarName[4096];
         char aggNames[MAX_NAME_SIZE];
         char aggValues[MAX_VALUES_SIZE];
         getAggregate(objectName, (char*)&scalarName, sizeof(scalarName),
                                  (char*)&aggNames, sizeof(aggNames),
                                  (char*)&aggValues, sizeof(aggValues));
         safestrcat((char*)&scalarName, "-", sizeof(scalarName));
         safestrcat((char*)&scalarName, statName, sizeof(scalarName));

         // ====== Reconciliate with skip list ==============================
         SkipListNode* skipListNode = SkipList;
         while(skipListNode != NULL) {
            if(strncmp(scalarName, skipListNode->Prefix, strlen(skipListNode->Prefix)) == 0) {
               break;
            }
            skipListNode = skipListNode->Next;;
         }
         if(skipListNode == NULL) {
            addScalar(scalarName, aggNames, aggValues, varNames, varValues, run, value);
         }
         else {
            if(interactive) {
               cout << "Skipping entry " << scalarName << endl;
            }
         }
      }
      else if(buffer[0] == 0x00) {
      }
      else {
         cerr << "ERROR: File \"" << fileName << "\", line " << line << " - Expected values, got crap!" << endl;
         success = false;
         break;
      }
   }

   if(inBZFile) {
      BZ2_bzReadClose(&bzerror, inBZFile);
   }
   fclose(inFile);
   return(success);
}


// ###### Dump scalars to output files ######################################
static void dumpScalars(const char*        simulationsDirectory,
                        const char*        resultsDirectory,
                        const char*        varNames,
                        const unsigned int compressionLevel,
                        const bool         interactive)
{
   // simpleRedBlackTreePrint(&StatisticsStorage, stdout);

   FILE*              outFile   = NULL;
   BZFILE*            outBZFile = NULL;
   size_t             line      = 0;
   int                bzerror;
   char               fileName[4096];
   char               lastStatisticsName[MAX_NAME_SIZE];
   char               buffer[16384];
   unsigned int       inLow;
   unsigned int       inHigh;
   unsigned int       outLow;
   unsigned int       outHigh;
   unsigned long long in;
   unsigned long long out;
   unsigned long long totalIn;
   unsigned long long totalOut;
   unsigned long long totalLines;
   size_t             totalFiles;

   totalIn = totalOut = totalLines = totalFiles = 0;
   lastStatisticsName[0] = 0x00;
   SimpleRedBlackTreeNode* node = simpleRedBlackTreeGetFirst(&StatisticsStorage);
   while(node != NULL) {
      ScalarNode* scalarNode = getScalarNodeFromStorageNode(node);

      if(strcmp(lastStatisticsName, scalarNode->ScalarName) != 0) {
         // ====== Close output file ========================================
         if(outBZFile) {
            BZ2_bzWriteClose64(&bzerror, outBZFile, 0, &inLow, &inHigh, &outLow, &outHigh);
            if(bzerror == BZ_OK) {
               outBZFile = NULL;
               in  = ((unsigned long long)inHigh << 32) + inLow;
               out = ((unsigned long long)outHigh << 32) + outLow;
               totalIn += in;
               totalOut += out;
               totalLines += line;
               totalFiles++;
               if(interactive) {
                  cout << " (" << line << " lines, "
                       << in << " -> " << out << " - " << ((double)out * 100.0 / in) << "%)" << endl;
               }
            }
            else {
               cerr << endl
                    << "ERROR: libbz2 failed to close file <" << fileName << ">!" << endl;
               fclose(outFile);
               unlink(fileName);
               outFile = NULL;
            }
         }
         else if(outFile) {
            cout << " (" << line << " lines)" << endl;
         }
         if(outFile) {
            fclose(outFile);
            outFile = NULL;
         }


         // ====== Open output file =========================================
         if(compressionLevel > 0) {
            snprintf((char*)&fileName, sizeof(fileName), "%s/%s.data", resultsDirectory, scalarNode->ScalarName);
            unlink(fileName);
            snprintf((char*)&fileName, sizeof(fileName), "%s/%s.data.bz2", resultsDirectory, scalarNode->ScalarName);
         }
         else {
            snprintf((char*)&fileName, sizeof(fileName), "%s/%s.data.bz2", resultsDirectory, scalarNode->ScalarName);
            unlink(fileName);
            snprintf((char*)&fileName, sizeof(fileName), "%s/%s.data", resultsDirectory, scalarNode->ScalarName);
         }
         if(interactive) {
            cout << "Statistics \"" << scalarNode->ScalarName << "\" ...";
         }
         cout.flush();
         outFile = fopen(fileName, "w");
         if(outFile == NULL) {
            cerr << endl
                 << "ERROR: Unable to create file <" << fileName << ">!" << endl;
            exit(1);
         }
         if(compressionLevel > 0) {
            outBZFile = BZ2_bzWriteOpen(&bzerror, outFile, compressionLevel, 0, 30);
            if(bzerror != BZ_OK) {
               cerr << endl
                    << "ERROR: Unable to initialize BZip2 compression on file <" << fileName << ">!" << endl
                    << "Reason: " << BZ2_bzerror(outBZFile, &bzerror) << endl;
               BZ2_bzWriteClose(&bzerror, outBZFile, 0, NULL, NULL);
               fclose(outFile);
               unlink(fileName);
               outBZFile = NULL;
               outFile   = NULL;
            }
         }
         else {
            outBZFile = NULL;
         }
         line = 1;


         // ====== Write table header =======================================
         if((outBZFile) || (outFile)) {
            snprintf((char*)&buffer, sizeof(buffer),
                     "RunNo ValueNo\t%s\t%s\t%s\n",
                     scalarNode->AggNames,
                     varNames,
                     scalarNode->ScalarName);
            if(outBZFile) {
               BZ2_bzWrite(&bzerror, outBZFile, buffer, strlen(buffer));
               if(bzerror != BZ_OK) {
                  cerr << endl
                        << "ERROR: libbz2 failed to write into file <" << fileName << ">!" << endl
                        << "Reason: " << BZ2_bzerror(outBZFile, &bzerror) << endl;
                  BZ2_bzWriteClose(&bzerror, outBZFile, 0, NULL, NULL);
                  fclose(outFile);
                  unlink(fileName);
                  outBZFile = NULL;
                  outFile   = NULL;
               }
            }
            else if(outFile) {
               if(fputs(buffer, outFile) <= 0) {
                  cerr << "ERROR: Failed to write into file <" << fileName << ">!" << endl;
                  fclose(outFile);
                  unlink(fileName);
                  outFile = NULL;
               }
            }
         }
         strcpy((char*)&lastStatisticsName, scalarNode->ScalarName);
      }


      // ====== Write table rows =========================================
      size_t valueNumber = 1;
      vector<double>::iterator valueIterator = scalarNode->ValueSet.begin();
      while(valueIterator != scalarNode->ValueSet.end()) {
         snprintf((char*)&buffer, sizeof(buffer),
                  "%07u %04u %04u\t%s\t%s\t%1.12f\n",
                  (unsigned int)line,
                  (unsigned int)scalarNode->Run,
                  (unsigned int)valueNumber,
                  scalarNode->AggValues,
                  scalarNode->VarValues,
                  *valueIterator);
         if(outBZFile) {
            BZ2_bzWrite(&bzerror, outBZFile, buffer, strlen(buffer));
            if(bzerror != BZ_OK) {
               cerr << endl
                     << "ERROR: Writing to file <" << fileName << "> failed!" << endl
                     << "Reason: " << BZ2_bzerror(outBZFile, &bzerror) << endl;
               BZ2_bzWriteClose(&bzerror, outBZFile, 0, NULL, NULL);
               fclose(outFile);
               unlink(fileName);
               outBZFile = NULL;
               outFile   = NULL;
               break;
            }
         }
         else if(outFile) {
            if(fputs(buffer, outFile) <= 0) {
               cerr << endl
                     << "ERROR: Failed to write into file <" << fileName << ">!" << endl;
               fclose(outFile);
               unlink(fileName);
               outFile = NULL;
               break;
            }
         }

         valueNumber++;
         line++;
         valueIterator++;
      }

      node = simpleRedBlackTreeGetNext(&StatisticsStorage, node);
   }


   // ====== Close last output file =========================================
   if(outBZFile) {
      BZ2_bzWriteClose64(&bzerror, outBZFile, 0, &inLow, &inHigh, &outLow, &outHigh);
      if(bzerror == BZ_OK) {
         in  = ((unsigned long long)inHigh << 32) + inLow;
         out = ((unsigned long long)outHigh << 32) + outLow;
         totalIn += in;
         totalOut += out;
         totalLines += line;
         totalFiles++;
         if(interactive) {
            cout << " (" << line << " lines, "
                 << in << " -> " << out << " - " << ((double)out * 100.0 / in) << "%)" << endl;
         }
     }
     else {
         cerr << endl
              << "ERROR: libbz2 failed to close file <" << fileName << ">!" << endl
              << "Reason: " << BZ2_bzerror(outBZFile, &bzerror) << endl;
         fclose(outFile);
         unlink(fileName);
         outFile = NULL;
      }
   }
   else if(outFile) {
      cout << " (" << line << " lines)" << endl;
   }
   if(outFile) {
      fclose(outFile);
      outFile = NULL;
   }

   cout << "Wrote " << totalLines << " lines into " << totalFiles << " files, "
        << totalIn << " -> " << totalOut << " - " << ((double)totalOut * 100.0 / totalIn) << "%" << endl;
}


// ###### Print usage and exit ##############################################
static void usage(const char* name)
{
   cerr << "Usage: "
        << name << " [Var Names] {-compress=1-9} {-interactive|-batch}" << endl;
   exit(1);
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   char         varNamesBuffer[2048];
   const char*  varNames = "_NoVarNamesGiven_";
   char         varValues[4096];
   char         simulationsDirectory[4096];
   char         resultsDirectory[4096];
   char         logFileName[4096];
   char         statusFileName[4096];
   unsigned int compressionLevel = 9;
   bool         interactive      = true;
   char         buffer[4096];
   char*        command;

   simpleRedBlackTreeNew(&StatisticsStorage,
                             scalarNodePrintFunction,
                             scalarNodeComparisonFunction);


   if(argc > 1) {
      varNames = argv[1];
      for(int i = 2;i < argc;i++) {
         if(!(strncmp(argv[i], "-compress=", 10))) {
            compressionLevel = atol((char*)&argv[i][10]);
            if(compressionLevel > 9) {
               compressionLevel = 9;
            }
         }
         else if(!(strcmp(argv[i], "-batch"))) {
            interactive = false;
         }
         else if(!(strcmp(argv[i], "-interactive"))) {
            interactive = true;
         }
         else {
            usage(argv[0]);
         }
      }
   }
   else {
      usage(argv[0]);
   }


   cout << "CreateSummary - Version 3.00" << endl
        << "============================" << endl << endl
        << "Compression Level: " << compressionLevel << endl
        << "Interactive Mode:  " << (interactive ? "on" : "off") << endl
        << endl;


   varValues[0]      = 0x00;
   logFileName[0]    = 0x00;
   statusFileName[0] = 0x00;
   strcpy((char*)&simulationsDirectory, ".");
   strcpy((char*)&resultsDirectory, ".");
   if(interactive) {
      cout << "Ready> ";
      cout.flush();
   }
   else {
      cout << "Processing input ..." << endl;
   }
   bool scalarFileError = false;
   while((command = fgets((char*)&buffer, sizeof(buffer), stdin))) {
      command[strlen(command) - 1] = 0x00;
      if(command[0] == 0x00) {
         cout << "*** End of File ***" << endl;
         break;
      }

      if(interactive) {
         cout << command << endl;
      }

      if(!(strncmp(command, "--varnames=", 11))) {
         if(command[11] != '\"') {
            snprintf((char*)&varNamesBuffer, sizeof(varNamesBuffer), "%s", (char*)&command[11]);
         }
         else {
            snprintf((char*)&varNamesBuffer, sizeof(varNamesBuffer), "%s", (char*)&command[12]);
            size_t l = strlen(varNamesBuffer);
            if(l > 0) {
               varNamesBuffer[l - 1] = 0x00;
            }
         }
         varNames = (const char*)&varNamesBuffer;
      }
      else if(!(strncmp(command, "--values=", 9))) {
         if(command[9] != '\"') {
            snprintf((char*)&varValues, sizeof(varValues), "%s", (char*)&command[9]);
         }
         else {
            snprintf((char*)&varValues, sizeof(varValues), "%s", (char*)&command[10]);
            size_t l = strlen(varValues);
            if(l > 0) {
               varValues[l - 1] = 0x00;
            }
         }
      }
      else if(!(strncmp(command, "--logfile=", 10))) {
         snprintf((char*)&logFileName, sizeof(logFileName), "%s", (char*)&command[10]);
      }
      else if(!(strncmp(command, "--statusfile=", 13))) {
         snprintf((char*)&statusFileName, sizeof(statusFileName), "%s", (char*)&command[13]);
      }
      else if(!(strncmp(command, "--input=", 8))) {
         if(varValues[0] == 0x00) {
            cerr << "ERROR: No values given (parameter --values=...)!" << endl;
            exit(1);
         }
         if(!handleScalarFile(varNames, varValues, (char*)&command[8], interactive)) {
            scalarFileError = true;
            if(logFileName[0] != 0x00) {
               fprintf(stderr, " => see logfile %s\n", logFileName);
            }
            if(statusFileName[0] != 0x00) {
               fputs(" Removing status file; restart simulation to re-create this run!\n", stderr);
               unlink(statusFileName);
            }
         }
         varValues[0]      = 0x00;
         logFileName[0]    = 0x00;
         statusFileName[0] = 0x00;
      }
      else if(!(strncmp(command, "--skip=", 7))) {
         SkipListNode* skipListNode = new SkipListNode;
         if(skipListNode == NULL) {
            cerr << "ERROR: Out of memory!" << endl;
            exit(1);
         }
         skipListNode->Next = SkipList;
         snprintf((char*)&skipListNode->Prefix, sizeof(skipListNode->Prefix), "%s",
                  (const char*)&command[7]);
         SkipList = skipListNode;
      }
      else if(!(strncmp(command, "--simulationsdirectory=", 23))) {
         snprintf((char*)&simulationsDirectory, sizeof(simulationsDirectory), "%s", (char*)&command[23]);
      }
      else if(!(strncmp(command, "--resultsdirectory=", 19))) {
         snprintf((char*)&resultsDirectory, sizeof(resultsDirectory), "%s", (char*)&command[19]);
      }
      else if(!(strncmp(command, "--level=", 8))) {
         // Deprecated, ignore ...
      }
      else {
         cerr << "ERROR: Invalid command \"" << command << "\"!" << endl
              << "Examples:" << endl
              << "   --skip=calcAppServer" << endl
              << "   --simulationsdirectory=MySimulationsDirectory" << endl
              << "   --resultsdirectory=MySimulationsDirectory/Results" << endl
              << "   --skip=calcAppServer" << endl
              << "   --varnames=Alpha Beta Gamma" << endl
              << "   --values=alpha100 beta200 gamma300" << endl
              << "   --input=simulations/scalar-file.sca.bz2" << endl
              << "   (Ctrl-D, EOF)" << endl;
         if(!interactive) {
            exit(1);
         }
      }

      if(interactive) {
         cout << "Ready> ";
         cout.flush();
      }
   }


   if(interactive) {
      cout << endl << endl;
   }
   if(!scalarFileError) {
      cout << "Writing scalar files..." << endl;
      dumpScalars(simulationsDirectory, resultsDirectory, varNames, compressionLevel, interactive);
   }
   else {
      cerr << "Not all scalar files have been read -> aborting!" << endl;
      exit(1);
   }


   simpleRedBlackTreeDelete(&StatisticsStorage);
   return(0);
}
