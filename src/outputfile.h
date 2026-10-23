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

#ifndef OUTPUTFILE_H
#define OUTPUTFILE_H

#include "ioformat.h"

#include <bzlib.h>
#include <lzma.h>
#include <zlib.h>
#include <zstd.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(__GNUC__) || defined(__clang__)
#define NPM_PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define NPM_PRINTF_FORMAT(fmt, args)
#endif


// ###### Output File Exception #############################################
class OutputFileException : public std::runtime_error
{
   public:
   OutputFileException(std::string_view message,
                       std::string_view filename  = {},
                       int              errorCode = 0,
                       std::string_view reason    = {})
      : std::runtime_error(formatErrorMessage(message, filename, errorCode, reason)),
        Filename(filename),
        Reason(!reason.empty() ? std::string(reason) : (errorCode != 0 ? std::string(std::strerror(errorCode)) : std::string())),
        ErrorCode(errorCode)
   {
   }

   [[nodiscard]] const std::string& getFileName() const noexcept { return Filename; }
   [[nodiscard]] const std::string& getReason() const noexcept   { return Reason; }
   [[nodiscard]] int getErrorCode() const noexcept               { return ErrorCode; }

   private:
   static std::string formatErrorMessage(std::string_view message,
                                         std::string_view filename,
                                         int              errorCode,
                                         std::string_view reason)
   {
      std::string result(message);
      if(!filename.empty()) {
         result += " <";
         result += filename;
         result += ">";
      }
      if(!reason.empty()) {
         result += ": ";
         result += reason;
      }
      else if(errorCode != 0) {
         result += ": ";
         result += std::strerror(errorCode);
      }
      return result;
   }

   std::string Filename;
   std::string Reason;
   int         ErrorCode { 0 };
};


// ###### Output File Class #################################################
class OutputFile
{
   public:
   OutputFile() = default;
   ~OutputFile();

   OutputFile(const OutputFile&) = delete;
   OutputFile& operator=(const OutputFile&) = delete;

   OutputFile(OutputFile&& other) noexcept;
   OutputFile& operator=(OutputFile&& other) noexcept;

   void initialize(const std::filesystem::path& name,
                   OutputFileFormat             format           = OutputFileFormat::Plain,
                   unsigned int                 compressionLevel = 3);

   void initialize(const char*      name,
                   OutputFileFormat format           = OutputFileFormat::Plain,
                   unsigned int     compressionLevel = 3);

   void finish(bool                closeFile = true,
               unsigned long long* bytesIn   = nullptr,
               unsigned long long* bytesOut  = nullptr);

   void printf(const char* str, ...) NPM_PRINTF_FORMAT(2, 3);
   void write(const char* buffer, size_t bufferLength);
   void write(std::string_view data);

   [[nodiscard]] inline bool exists() const noexcept {
      return File != nullptr || BZFile != nullptr || GZFile != nullptr ||
             LZMAInitialized || ZSTDCStream != nullptr;
   }

   [[nodiscard]] inline OutputFileFormat getFormat() const noexcept { return Format; }
   [[nodiscard]] inline FILE* getFile() const noexcept              { return File;   }
   [[nodiscard]] inline const std::string& getName() const noexcept { return Name;   }
   [[nodiscard]] inline unsigned long long getLine() const noexcept { return Line;   }
   inline unsigned long long nextLine() noexcept                    { return Line++; }

   private:
   OutputFileFormat   Format             { OutputFileFormat::None };
   std::string        Name;
   unsigned long long Line               { 0 };
   FILE*              File               { nullptr };
   BZFILE*            BZFile             { nullptr };
   gzFile             GZFile             { nullptr };
   lzma_stream        LZMAStream         {};
   bool               LZMAInitialized    { false };
   ZSTD_CStream*      ZSTDCStream        { nullptr };
   unsigned long long PlainBytesIn       { 0 };
   unsigned long long CompressedBytesOut { 0 };
   bool               WriteError         { false };
};

#endif // OUTPUTFILE_H
