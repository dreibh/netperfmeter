/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009 by Thomas Dreibholz
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

#include "outputfile.h"

#include <stdarg.h>


// ###### Constructor #######################################################
OutputFile::OutputFile()
{
   File       = NULL;
   BZFile     = NULL;
   WriteError = false;
}


// ###### Destructor ########################################################
OutputFile::~OutputFile()
{
   finish();
}


// ###### Initialize output file ############################################
bool OutputFile::initialize(const char* name, const OutputFileFormat format)
{
   // ====== Initialize object ==============================================
   finish();

   Format = format;
   if(name != NULL) {
      Name = std::string(name);
   }
   else {
      Name = std::string();
   }
   Line = 0;

   // ====== Initialize file ================================================
   File = NULL;
   if(format != OFF_None) {
      File = (name != NULL) ? fopen(name, "w+") : tmpfile();
      if(File == NULL) {
         std::cerr << "ERROR: Unable to create output file " << name << "!" << std::endl;
         WriteError = true;
         return(false);
      }
   
      // ====== Initialize BZip2 compressor ====================================
      if(format == OFF_BZip2) {
         int bzerror;
         BZFile = BZ2_bzWriteOpen(&bzerror, File, 9, 0, 30);
         if(bzerror != BZ_OK) {
            std::cerr << "ERROR: Unable to initialize BZip2 compression on file <"
                     << name << ">!" << std::endl
                     << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
            BZ2_bzWriteClose(&bzerror, BZFile, 0, NULL, NULL);
            fclose(File);
            File = NULL;
            unlink(name);
            WriteError = true;
            return(false);
         }
      }
      WriteError = false;
   }
   return(true);
}


// ###### Finish output file ################################################
bool OutputFile::finish(const bool closeFile)
{
   // ====== Finish BZip2 compression =======================================
   if(BZFile) {
      int bzerror;
      BZ2_bzWriteClose(&bzerror, BZFile, 0, NULL, NULL);
      if(bzerror != BZ_OK) {
         std::cerr << "ERROR: Unable to finish BZip2 compression on file <"
                   << Name << ">!" << std::endl
                   << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
         WriteError = true;
      }
      BZFile = NULL;
   }
   // ====== Close or rewind file ===========================================
   if(File) {
      if(closeFile) {
         if(fclose(File) != 0) {
            WriteError = true;
         }
         File = NULL;
      }
      else {
         rewind(File);
      }
   }
   return(!WriteError);
}


// ###### Write string into output file #####################################
bool OutputFile::printf(const char* str, ...)
{
   char buffer[16384];

   va_list va;
   va_start(va, str);
   int bufferLength = vsnprintf(buffer, sizeof(buffer), str, va);
   buffer[sizeof(buffer) - 1] = 0x00;   // Just to be really sure ...
   va_end(va);
   
   if(exists()) {
      // ====== Compress string and write data =================================
      if(BZFile) {
         int bzerror;
         BZ2_bzWrite(&bzerror, BZFile, (void*)&buffer, bufferLength);
         if(bzerror != BZ_OK) {
            std::cerr << std::endl
                     << "ERROR: libbz2 failed to write into file <"
                     << Name << ">!" << std::endl
                     << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
            return(false);
         }
      }

      // ====== Write string as plain text =====================================
      else if(File) {
         if(fputs(buffer, File) < 0) {
            std::cerr << "ERROR: Failed to write into file <"
                     << Name << ">!" << std::endl;
            return(false);
         }
      }
   }
   return(true);
}
