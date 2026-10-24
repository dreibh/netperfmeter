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

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#ifndef INPUTFILE_H
#define INPUTFILE_H

#include "fileio.h"

#include <bzlib.h>
#include <lzma.h>
#include <zlib.h>
#include <zstd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>


// ###### Input File Exception ##############################################
class InputFileException : public std::runtime_error
{
   public:
   InputFileException(std::string_view message,
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


// ###### Input File Class ##################################################
class InputFile
{
   public:
   InputFile() = default;
   ~InputFile();

   InputFile(const InputFile&) = delete;
   InputFile& operator=(const InputFile&) = delete;

   InputFile(InputFile&& other) noexcept;
   InputFile& operator=(InputFile&& other) noexcept;

   void initialize(const std::filesystem::path& name,
                   InputFileFormat              format = InputFileFormat::Plain);

   void initialize(const char*     name,
                   InputFileFormat format = InputFileFormat::Plain);

   void finish(bool closeFile = true);

   size_t read(char* buffer, size_t bufferSize);
   inline size_t read(void* buffer, size_t bufferSize) {
      return read(static_cast<char*>(buffer), bufferSize);
   }

   ssize_t readLine(char* buffer, size_t bufferSize, bool& eof);
   bool readLine(std::string& line);

   [[nodiscard]] inline bool exists() const noexcept {
      return File != nullptr || BZFile != nullptr || GZFile != nullptr ||
             LZMAInitialized || ZSTDDStream != nullptr;
   }

   [[nodiscard]] inline InputFileFormat getFormat() const noexcept  { return Format; }
   [[nodiscard]] inline FILE* getFile() const noexcept              { return File;   }
   [[nodiscard]] inline const std::string& getName() const noexcept { return Name;   }
   [[nodiscard]] inline unsigned long long getLine() const noexcept { return Line;   }

   private:
   InputFileFormat    Format          { InputFileFormat::None };
   std::string        Name;
   unsigned long long Line            { 0 };
   FILE*              File            { nullptr };
   BZFILE*            BZFile          { nullptr };
   gzFile             GZFile          { nullptr };
   lzma_stream        LZMAStream      {};
   bool               LZMAInitialized { false };
   ZSTD_DStream*      ZSTDDStream     { nullptr };

   uint8_t            RawBuffer[16384];
   size_t             RawBufferPos    { 0 };
   size_t             RawBufferSize   { 0 };
   bool               RawFileEof      { false };
   bool               StreamEof       { false };

   // Stored leftover uncompressed text from line reading
   size_t             StoragePos      { 0 };
   char               Storage[16384];
};

#endif // INPUTFILE_H
