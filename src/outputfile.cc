/* $Id$
 *
 * Plain/BZip2 File Output
 * Copyright (C) 2009-2010 by Thomas Dreibholz
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
   Line       = 0;
}


// ###### Destructor ########################################################
OutputFile::~OutputFile()
{
   finish();
}


// ###### Initialize output file ############################################
bool OutputFile::initialize(const char*            name,
                            const OutputFileFormat format,
                            const unsigned int     compressionLevel)
{
   // ====== Initialize object ==============================================
   finish();

   Line   = 0;
   Format = format;
   if(name != NULL) {
      Name = std::string(name);
   }
   else {
      Name = std::string();
   }

   // ====== Initialize file ================================================
   File   = NULL;
   BZFile = NULL;
   if(format != OFF_None) {
      File = (name != NULL) ? fopen(name, "w+") : tmpfile();
      if(File == NULL) {
         std::cerr << "ERROR: Unable to create output file <"
                   << Name << ">!" << std::endl;
         WriteError = true;
         return(false);
      }

      // ====== Initialize BZip2 compressor ====================================
      if(format == OFF_BZip2) {
         int bzerror;
         BZFile = BZ2_bzWriteOpen(&bzerror, File, compressionLevel, 0, 30);
         if(bzerror != BZ_OK) {
            std::cerr << "ERROR: Unable to initialize BZip2 compression on file <"
                      << Name << ">!" << std::endl
                      << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
            BZ2_bzWriteClose(&bzerror, BZFile, 0, NULL, NULL);
            WriteError = true;
            finish();
            return(false);
         }
      }
      WriteError = false;
   }
   return(true);
}


// ###### Finish output file ################################################
bool OutputFile::finish(const bool          closeFile,
                        unsigned long long* bytesIn,
                        unsigned long long* bytesOut)
{
   // ====== Finish BZip2 compression =======================================
   if(BZFile) {
      int          bzerror;
      unsigned int inLow, inHigh, outLow, outHigh;
      BZ2_bzWriteClose64(&bzerror, BZFile, 0, &inLow, &inHigh, &outLow, &outHigh);
      if(bzerror == BZ_OK) {
         if(bytesIn) {
            *bytesIn = ((unsigned long long)inHigh << 32) + inLow;
         }
         if(bytesOut) {
            *bytesOut = ((unsigned long long)outHigh << 32) + outLow;
         }
      }
      else {
         std::cerr << "ERROR: Unable to finish BZip2 compression on file <"
                   << Name << ">!" << std::endl
                   << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
         WriteError = true;
      }
      BZFile = NULL;
   }
   else {
      if(bytesIn) {
         *bytesIn = 0;
      }
      if(bytesOut) {
         *bytesOut = 0;
      }
   }

   // ====== Close or rewind file ===========================================
   if(File) {
      if(closeFile) {
         if(fclose(File) != 0) {
            std::cerr << "ERROR: Unable to close output file <"
                      << Name << ">!" << std::endl;
            WriteError = true;
         }
         File = NULL;
      }
      else {
         rewind(File);
      }
   }

   // ====== Delete file in case of write error =============================
   if(WriteError) {
      unlink(Name.c_str());
   }
   return(!WriteError);
}


// ###### Write string into output file #####################################
bool OutputFile::printf(const char* str, ...)
{
   if(exists()) {
      char buffer[16384];

      va_list va;
      va_start(va, str);
      int bufferLength = vsnprintf(buffer, sizeof(buffer), str, va);
      buffer[sizeof(buffer) - 1] = 0x00;   // Just to be really sure ...
      va_end(va);

      return(write((const char*)&buffer, bufferLength));
   }
   return(true);
}


// ###### Write data into output file #######################################
bool OutputFile::write(const char* buffer, const size_t bufferLength)
{
   if(exists()) {
      // ====== Compress string and write data ==============================
      if(BZFile) {
         int bzerror;
         BZ2_bzWrite(&bzerror, BZFile, (void*)buffer, bufferLength);
         if(bzerror != BZ_OK) {
            std::cerr << std::endl
                      << "ERROR: libbz2 failed to write into file <"
                      << Name << ">!" << std::endl
                      << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
            return(false);
         }
      }

      // ====== Write string as plain text ==================================
      else if(File) {
         if(fwrite(buffer, bufferLength, 1, File) != 1) {
            std::cerr << "ERROR: Failed to write into file <"
                     << Name << ">!" << std::endl;
            return(false);
         }
      }
   }
   return(true);
}
