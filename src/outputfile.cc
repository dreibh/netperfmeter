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
 *
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

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>


// ###### Destructor ########################################################
OutputFile::~OutputFile()
{
   try {
      finish();
   }
   catch(...) {
      // Destructors must remain noexcept
   }
}


// ###### Move Constructor ##################################################
OutputFile::OutputFile(OutputFile&& other) noexcept
   : Format(other.Format),
     Name(std::move(other.Name)),
     Line(other.Line),
     File(other.File),
     BZFile(other.BZFile),
     WriteError(other.WriteError)
{
   other.Format     = OutputFileFormat::None;
   other.Line       = 0;
   other.File       = nullptr;
   other.BZFile     = nullptr;
   other.WriteError = false;
}


// ###### Move Assignment Operator ##########################################
OutputFile& OutputFile::operator=(OutputFile&& other) noexcept
{
   if(this != &other) {
      try {
         finish();
      }
      catch(...) {
      }

      Format     = other.Format;
      Name       = std::move(other.Name);
      Line       = other.Line;
      File       = other.File;
      BZFile     = other.BZFile;
      WriteError = other.WriteError;

      other.Format     = OutputFileFormat::None;
      other.Line       = 0;
      other.File       = nullptr;
      other.BZFile     = nullptr;
      other.WriteError = false;
   }
   return *this;
}


// ###### Initialize output file (const char*) ##############################
void OutputFile::initialize(const char*            name,
                            const OutputFileFormat format,
                            const unsigned int     compressionLevel)
{
   if(name != nullptr) {
      initialize(std::filesystem::path(name), format, compressionLevel);
   }
   else {
      initialize(std::filesystem::path{}, format, compressionLevel);
   }
}


// ###### Initialize output file (std::filesystem::path) ####################
void OutputFile::initialize(const std::filesystem::path& name,
                            const OutputFileFormat       format,
                            const unsigned int           compressionLevel)
{
   finish();

   Line   = 0;
   Format = format;
   Name   = name.string();

   File   = nullptr;
   BZFile = nullptr;

   if(format != OutputFileFormat::None) {
      if(!Name.empty()) {
         File = std::fopen(Name.c_str(), "w+");
      }
      else {
         File = std::tmpfile();
      }

      if(File == nullptr) {
         WriteError = true;
         throw OutputFileException("Unable to create output file", Name, errno);
      }

      // ====== Initialize BZip2 compressor =================================
      if(format == OutputFileFormat::BZip2) {
         int bzerror = BZ_OK;
         BZFile = BZ2_bzWriteOpen(&bzerror, File, static_cast<int>(compressionLevel), 0, 30);
         if(bzerror != BZ_OK || BZFile == nullptr) {
            const int err = bzerror;
            const char* reason = BZ2_bzerror(BZFile, &bzerror);
            BZ2_bzWriteClose(&bzerror, BZFile, 0, nullptr, nullptr);
            BZFile = nullptr;
            std::fclose(File);
            File = nullptr;
            WriteError = true;
            if(!Name.empty()) {
               std::error_code ec;
               std::filesystem::remove(Name, ec);
            }
            throw OutputFileException("Unable to initialize BZip2 compression on file",
                                      Name, err, reason ? reason : "Unknown BZip2 error");
         }
      }
      WriteError = false;
   }
}


// ###### Finish output file ################################################
void OutputFile::finish(const bool          closeFile,
                        unsigned long long* bytesIn,
                        unsigned long long* bytesOut)
{
   std::exception_ptr errorToThrow = nullptr;

   // ====== Finish BZip2 compression =======================================
   if(BZFile) {
      int          bzerror = BZ_OK;
      unsigned int inLow = 0;
      unsigned int inHigh = 0;
      unsigned int outLow = 0;
      unsigned int outHigh = 0;
      BZ2_bzWriteClose64(&bzerror, BZFile, 0, &inLow, &inHigh, &outLow, &outHigh);
      if(bzerror == BZ_OK) {
         if(bytesIn) {
            *bytesIn = (static_cast<unsigned long long>(inHigh) << 32) + inLow;
         }
         if(bytesOut) {
            *bytesOut = (static_cast<unsigned long long>(outHigh) << 32) + outLow;
         }
      }
      else {
         const char* reason = BZ2_bzerror(BZFile, &bzerror);
         WriteError = true;
         errorToThrow = std::make_exception_ptr(
            OutputFileException("Unable to finish BZip2 compression on file",
                                Name, bzerror, reason ? reason : "Unknown BZip2 error")
         );
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
         if(std::fclose(File) != 0) {
            WriteError = true;
            if(!errorToThrow) {
               errorToThrow = std::make_exception_ptr(
                  OutputFileException("Unable to close output file", Name, errno)
               );
            }
         }
         File = nullptr;
      }
      else {
         std::rewind(File);
      }
   }

   // ====== Delete file in case of write error =============================
   if(WriteError && !Name.empty()) {
      std::error_code ec;
      std::filesystem::remove(Name, ec);
   }

   if(errorToThrow) {
      std::rethrow_exception(errorToThrow);
   }
}


// ###### Write string into output file (printf-style) ######################
void OutputFile::printf(const char* str, ...)
{
   if(!exists()) {
      return;
   }

   char stackBuffer[16384];
   va_list va;
   va_start(va, str);
   va_list vaCopy;
   va_copy(vaCopy, va);

   const int bufferLength = std::vsnprintf(stackBuffer, sizeof(stackBuffer), str, va);
   va_end(va);

   if(bufferLength < 0) {
      va_end(vaCopy);
      throw OutputFileException("String formatting error in printf()", Name);
   }

   if(static_cast<size_t>(bufferLength) < sizeof(stackBuffer)) {
      va_end(vaCopy);
      write(stackBuffer, static_cast<size_t>(bufferLength));
      return;
   }

   // Buffer exceeded stack allocation; dynamically resize
   std::vector<char> dynamicBuffer(static_cast<size_t>(bufferLength) + 1);
   std::vsnprintf(dynamicBuffer.data(), dynamicBuffer.size(), str, vaCopy);
   va_end(vaCopy);

   write(dynamicBuffer.data(), static_cast<size_t>(bufferLength));
}


// ###### Write string_view into output file ################################
void OutputFile::write(std::string_view data)
{
   write(data.data(), data.size());
}


// ###### Write raw buffer into output file #################################
void OutputFile::write(const char* buffer, const size_t bufferLength)
{
   if(exists() && bufferLength > 0) {
      // ====== Compress data and write to file =============================
      if(BZFile) {
         int bzerror = BZ_OK;
         BZ2_bzWrite(&bzerror, BZFile, const_cast<char*>(buffer), static_cast<int>(bufferLength));
         if(bzerror != BZ_OK) {
            WriteError = true;
            const char* reason = BZ2_bzerror(BZFile, &bzerror);
            throw OutputFileException("libbz2 failed to write into file",
                                      Name, bzerror, reason ? reason : "Unknown BZip2 error");
         }
      }

      // ====== Write data as plain text ====================================
      else if(File) {
         if(std::fwrite(buffer, bufferLength, 1, File) != 1) {
            WriteError = true;
            throw OutputFileException("Unable to write into file", Name, errno);
         }
      }
   }
}
