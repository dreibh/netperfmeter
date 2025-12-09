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

#include "outputfile.h"

#include <unistd.h>
#include <stdarg.h>


// ###### Constructor #######################################################
OutputFile::OutputFile()
{
   File       = nullptr;
   BZFile     = nullptr;
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
   if(name != nullptr) {
      Name = std::string(name);
   }
   else {
      Name = std::string();
   }

   // ====== Initialize file ================================================
   File   = nullptr;
   BZFile = nullptr;
   if(format != OFF_None) {
      File = (name != nullptr) ? fopen(name, "w+") : tmpfile();
      if(File == nullptr) {
         std::cerr << "ERROR: Unable to create output file <" << Name << ">!\n";
         WriteError = true;
         return false;
      }

      // ====== Initialize BZip2 compressor ====================================
      if(format == OFF_BZip2) {
         int bzerror;
         BZFile = BZ2_bzWriteOpen(&bzerror, File, compressionLevel, 0, 30);
         if(bzerror != BZ_OK) {
            std::cerr << "ERROR: Unable to initialize BZip2 compression on file <" << Name << ">!\n"
                      << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << "\n";
            BZ2_bzWriteClose(&bzerror, BZFile, 0, nullptr, nullptr);
            WriteError = true;
            finish();
            return false;
         }
      }
      WriteError = false;
   }
   return true;
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
         std::cerr << "ERROR: Unable to finish BZip2 compression on file <" << Name << ">!\n"
                   << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << "\n";
         WriteError = true;
      }
      BZFile = nullptr;
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
            std::cerr << "ERROR: Unable to close output file <" << Name << ">!\n";
            WriteError = true;
         }
         File = nullptr;
      }
      else {
         rewind(File);
      }
   }

   // ====== Delete file in case of write error =============================
   if(WriteError) {
      unlink(Name.c_str());
   }
   return !WriteError;
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

      return write((const char*)&buffer, bufferLength);
   }
   return true;
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
            std::cerr << "\nERROR: libbz2 failed to write into file <" << Name << ">!\n"
                      << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << "\n";
            return false;
         }
      }

      // ====== Write string as plain text ==================================
      else if(File) {
         if(fwrite(buffer, bufferLength, 1, File) != 1) {
            std::cerr << "ERROR: Failed to write into file <" << Name << ">!\n";
            return false;
         }
      }
   }
   return true;
}
