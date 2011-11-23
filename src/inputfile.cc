/* $Id$
 *
 * Plain/BZip2 File Input
 * Copyright (C) 2009-2012 by Thomas Dreibholz
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
 * You should have relReceived a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: dreibh@iem.uni-due.de
 */

#include "inputfile.h"

#include <string.h>


// ###### Constructor #######################################################
InputFile::InputFile()
{
   File      = NULL;
   BZFile    = NULL;
   ReadError = false;
   Line      = 0;
}


// ###### Destructor ########################################################
InputFile::~InputFile()
{
   finish();
}


// ###### Initialize output file ############################################
bool InputFile::initialize(const char*           name,
                           const InputFileFormat format)
{
   // ====== Initialize object ==============================================
   finish();

   StoragePos = 0;
   Line       = 0;
   Format     = format;
   if(name != NULL) {
      Name = std::string(name);
   }
   else {
      Name = std::string();
   }

   // ====== Initialize file ================================================
   BZFile = NULL;
   File = (name != NULL) ? fopen(name, "r") : tmpfile();
   if(File == NULL) {
      std::cerr << "ERROR: Unable to open input file <"
                << Name << ">!" << std::endl;
      ReadError = true;
      return(false);
   }

   // ====== Initialize BZip2 compressor ====================================
   if(format == IFF_BZip2) {
      int bzerror;
      BZFile = BZ2_bzReadOpen(&bzerror, File, 0, 0, NULL, 0);
      if(bzerror != BZ_OK) {
         std::cerr << "ERROR: Unable to initialize BZip2 compression on file <"
                   << Name << ">!" << std::endl
                   << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
         BZ2_bzWriteClose(&bzerror, BZFile, 0, NULL, NULL);
         ReadError = true;
         finish();
         return(false);
      }
   }

   ReadError = false;
   return(true);
}


// ###### Finish output file ################################################
bool InputFile::finish(const bool closeFile)
{
   // ====== Finish BZip2 compression =======================================
   if(BZFile) {
      int bzerror;
      BZ2_bzReadClose(&bzerror, BZFile);
      BZFile = NULL;
   }

   // ====== Close or rewind file ===========================================
   if(File) {
      if(closeFile) {
         fclose(File);
         File = NULL;
      }
      else {
         rewind(File);
      }
   }
   return(!ReadError);
}


// ###### Read line from file ###############################################
ssize_t InputFile::readLine(char* buffer, size_t bufferSize, bool& eof)
{
   eof = false;

   if(bufferSize < 1) {
      return(-1);
   }
   bufferSize--;  // Leave one byte for terminating 0x00 byte.

   ssize_t bytesRead;
   for(;;) {
      if(StoragePos >= bufferSize) {
         std::cerr << "ERROR: Line " << Line << " of file <"
                   << Name << "> is too long to fit into buffer!" << std::endl;
         return(-1);
      }
      memcpy(buffer, (const char*)&Storage, StoragePos);
      if(Format == IFF_BZip2) {
         int bzerror;
         bytesRead = BZ2_bzRead(&bzerror, BZFile, (char*)&buffer[StoragePos],
                                std::min(sizeof(Storage), bufferSize) - StoragePos);
      }
      else if(Format == IFF_Plain) {
         bytesRead = fread((char*)&buffer[StoragePos], 1,
                           std::min(sizeof(Storage), bufferSize) - StoragePos, File);
      }
      else {
         bytesRead = -1;
      }

      if(bytesRead < 0) {
         return(bytesRead);   // Error.
      }
      else {
         bytesRead += StoragePos;
         buffer[bytesRead] = 0x00;
         if(bytesRead == 0) {
            eof = true;
            return(0);   // End of file.
         }

         StoragePos = 0;
         for(ssize_t i = 0;i < bytesRead;i++) {
            if(buffer[i] == '\n') {
               StoragePos = bytesRead - i - 1;
               memcpy((char*)&Storage, &buffer[i + 1], StoragePos);
               buffer[i] = 0x00;
               Line++;
               return(i);   // A line has been read.
            }
         }
      }
   }
}
