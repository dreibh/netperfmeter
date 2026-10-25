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

#include "inputfile.h"

#include <algorithm>
#include <bzlib.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <lzma.h>
#include <utility>
#include <zlib.h>
#include <zstd.h>


// ###### Private Implementation Structure ##################################
struct InputFile::Impl
{
   FILE*         File            { nullptr };
   BZFILE*       BZFile          { nullptr };
   gzFile        GZFile          { nullptr };
   lzma_stream   LZMAStream      {};
   bool          LZMAInitialized { false };
   ZSTD_DStream* ZSTDDStream     { nullptr };

   uint8_t       RawBuffer[16384];
   size_t        RawBufferPos    { 0 };
   size_t        RawBufferSize   { 0 };
   bool          RawFileEof      { false };
   bool          StreamEof       { false };   // Track logical end-of-stream

   // Stored leftover uncompressed text from line reading
   size_t        StoragePos      { 0 };
   char          Storage[16384];

   [[nodiscard]] bool exists() const noexcept {
      return File != nullptr || BZFile != nullptr || GZFile != nullptr ||
             LZMAInitialized || ZSTDDStream != nullptr;
   }
};


// ###### Constructor #######################################################
InputFile::InputFile()
   : Pimpl(std::make_unique<Impl>())
{
}


// ###### Destructor ########################################################
InputFile::~InputFile()
{
   try {
      finish();
   }
   catch(...) {
      // Destructors must remain noexcept
   }
}


// ###### Move Constructor ##################################################
InputFile::InputFile(InputFile&& other) noexcept
   : Pimpl(std::move(other.Pimpl)),
     Format(other.Format),
     Name(std::move(other.Name)),
     Line(other.Line)
{
   other.Format = FileFormat::None;
   other.Line   = 0;
   if(!other.Pimpl) {
      other.Pimpl = std::make_unique<Impl>();
   }
}


// ###### Move Assignment Operator ##########################################
InputFile& InputFile::operator=(InputFile&& other) noexcept
{
   if(this != &other) {
      try {
         finish();
      }
      catch(...) {
      }

      Pimpl  = std::move(other.Pimpl);
      Format = other.Format;
      Name   = std::move(other.Name);
      Line   = other.Line;

      other.Format = FileFormat::None;
      other.Line   = 0;
      if(!other.Pimpl) {
         other.Pimpl = std::make_unique<Impl>();
      }
   }
   return *this;
}


// ###### Check whether file exists/is open #################################
bool InputFile::exists() const noexcept
{
   return Pimpl && Pimpl->exists();
}


// ###### Get underlying FILE* ##############################################
FILE* InputFile::getFile() const noexcept
{
   return Pimpl ? Pimpl->File : nullptr;
}


// ###### Initialize input file (const char*) ###############################
void InputFile::initialize(const char* const name,
                           const FileFormat  format)
{
   initialize(name ? std::filesystem::path(name) : std::filesystem::path{}, format);
}


// ###### Initialize input file (std::filesystem::path) #####################
void InputFile::initialize(const std::filesystem::path& name,
                           const FileFormat             format)
{
   finish();

   if(!Pimpl) {
      Pimpl = std::make_unique<Impl>();
   }

   Pimpl->StoragePos      = 0;
   Line                   = 0;
   Format                 = format;
   Name                   = name.string();
   Pimpl->File            = nullptr;
   Pimpl->BZFile          = nullptr;
   Pimpl->GZFile          = nullptr;
   std::memset(&Pimpl->LZMAStream, 0, sizeof(lzma_stream));
   Pimpl->LZMAInitialized = false;
   Pimpl->ZSTDDStream     = nullptr;
   Pimpl->RawBufferPos    = 0;
   Pimpl->RawBufferSize   = 0;
   Pimpl->RawFileEof      = false;
   Pimpl->StreamEof       = false;

   if(format == FileFormat::None) {
      return;
   }
   else if(format == FileFormat::GZip) {
      if(!Name.empty()) {
         Pimpl->GZFile = gzopen(Name.c_str(), "rb");
      }
      if(Pimpl->GZFile == nullptr) {
         throw InputFileException("Unable to open GZip input file", Name, errno);
      }
   }
   else {
      Pimpl->File = !Name.empty() ? std::fopen(Name.c_str(), "rb") : std::tmpfile();
      if(Pimpl->File == nullptr) {
         throw InputFileException("Unable to open input file", Name, errno);
      }

      if(format == FileFormat::BZip2) {
         int bzerror = BZ_OK;
         Pimpl->BZFile = BZ2_bzReadOpen(&bzerror, Pimpl->File, 0, 0, nullptr, 0);
         if(bzerror != BZ_OK || Pimpl->BZFile == nullptr) {
            const int err = bzerror;
            const char* reason = BZ2_bzerror(Pimpl->BZFile, &bzerror);
            BZ2_bzReadClose(&bzerror, Pimpl->BZFile);
            Pimpl->BZFile = nullptr;
            std::fclose(Pimpl->File);
            Pimpl->File = nullptr;
            throw InputFileException("Unable to initialize BZip2 decompression",
                                     Name, err, reason ? reason : "Unknown BZip2 error");
         }
      }
      else if(format == FileFormat::XZ) {
         const lzma_ret ret = lzma_auto_decoder(&Pimpl->LZMAStream, UINT64_MAX, 0);
         if(ret != LZMA_OK) {
            std::fclose(Pimpl->File);
            Pimpl->File = nullptr;
            throw InputFileException("Unable to initialize XZ decompression",
                                     Name, static_cast<int>(ret), lzmaErrorToString(static_cast<int>(ret)));
         }
         Pimpl->LZMAStream.next_in  = nullptr;
         Pimpl->LZMAStream.avail_in = 0;
         Pimpl->LZMAInitialized     = true;
      }
      else if(format == FileFormat::ZSTD) {
         Pimpl->ZSTDDStream = ZSTD_createDStream();
         if(Pimpl->ZSTDDStream == nullptr) {
            std::fclose(Pimpl->File);
            Pimpl->File = nullptr;
            throw InputFileException("Unable to create Zstandard decompression stream", Name);
         }
         const size_t ret = ZSTD_initDStream(Pimpl->ZSTDDStream);
         if(ZSTD_isError(ret)) {
            ZSTD_freeDStream(Pimpl->ZSTDDStream);
            Pimpl->ZSTDDStream = nullptr;
            std::fclose(Pimpl->File);
            Pimpl->File = nullptr;
            throw InputFileException("Unable to initialize Zstandard decompression",
                                     Name, static_cast<int>(ret), ZSTD_getErrorName(ret));
         }
      }
   }
}


// ###### Finish input file #################################################
void InputFile::finish(const bool closeFile)
{
   if(!Pimpl) {
      return;
   }

   if(Pimpl->ZSTDDStream) {
      ZSTD_freeDStream(Pimpl->ZSTDDStream);
      Pimpl->ZSTDDStream = nullptr;
   }

   if(Pimpl->LZMAInitialized) {
      lzma_end(&Pimpl->LZMAStream);
      std::memset(&Pimpl->LZMAStream, 0, sizeof(lzma_stream));
      Pimpl->LZMAInitialized = false;
   }

   if(Pimpl->GZFile) {
      gzclose(Pimpl->GZFile);
      Pimpl->GZFile = nullptr;
   }

   if(Pimpl->BZFile) {
      int bzerror = BZ_OK;
      BZ2_bzReadClose(&bzerror, Pimpl->BZFile);
      Pimpl->BZFile = nullptr;
   }

   if(Pimpl->File) {
      if(closeFile) {
         if(std::fclose(Pimpl->File) != 0) {
            Pimpl->File = nullptr;
            throw InputFileException("Unable to close input file", Name, errno);
         }
         Pimpl->File = nullptr;
      }
      else {
         std::rewind(Pimpl->File);
      }
   }

   Pimpl->RawBufferPos  = 0;
   Pimpl->RawBufferSize = 0;
   Pimpl->RawFileEof    = false;
   Pimpl->StreamEof     = false;
}


// ###### Bulk read from file ###############################################
size_t InputFile::read(char* const buffer, const size_t bufferSize)
{
   if(!exists() || bufferSize == 0 || !Pimpl) {
      return 0;
   }

   size_t delivered = 0;

   // 1. Drain any leftover data in Storage
   if(Pimpl->StoragePos > 0) {
      const size_t toCopy = std::min(bufferSize, Pimpl->StoragePos);
      std::memcpy(buffer, Pimpl->Storage, toCopy);
      delivered += toCopy;
      if(toCopy < Pimpl->StoragePos) {
         std::memmove(Pimpl->Storage, Pimpl->Storage + toCopy, Pimpl->StoragePos - toCopy);
         Pimpl->StoragePos -= toCopy;
         return delivered;
      }
      Pimpl->StoragePos = 0;
      if(delivered == bufferSize) {
         return delivered;
      }
   }

   // 2. Read directly from the decompressor into destination buffer
   const size_t bytesToRead = bufferSize - delivered;
   char* const  dest        = buffer + delivered;

   // ====== Zstandard ======================================================
   if(Format == FileFormat::ZSTD) {
      ZSTD_outBuffer out = { dest, bytesToRead, 0 };
      while(out.pos < out.size) {
         if(Pimpl->RawBufferPos >= Pimpl->RawBufferSize && !Pimpl->RawFileEof) {
            const size_t n = std::fread(Pimpl->RawBuffer, 1, sizeof(Pimpl->RawBuffer), Pimpl->File);
            if(n == 0) {
               Pimpl->RawFileEof = true;
            }
            Pimpl->RawBufferPos  = 0;
            Pimpl->RawBufferSize = n;
         }
         if(Pimpl->RawBufferPos >= Pimpl->RawBufferSize && Pimpl->RawFileEof) {
            break;
         }

         ZSTD_inBuffer in = { Pimpl->RawBuffer, Pimpl->RawBufferSize, Pimpl->RawBufferPos };
         const size_t ret = ZSTD_decompressStream(Pimpl->ZSTDDStream, &out, &in);
         Pimpl->RawBufferPos = in.pos;

         if(ZSTD_isError(ret)) {
            throw InputFileException("Failed to decompress Zstandard data",
                                     Name, static_cast<int>(ret), ZSTD_getErrorName(ret));
         }
         if(ret == 0) {
            break;
         }
      }
      return delivered + out.pos;
   }

   // ====== XZ =============================================================
   else if(Format == FileFormat::XZ) {
      if(Pimpl->StreamEof) {
         return delivered;
      }
      Pimpl->LZMAStream.next_out  = reinterpret_cast<uint8_t*>(dest);
      Pimpl->LZMAStream.avail_out = bytesToRead;

      while(Pimpl->LZMAStream.avail_out > 0) {
         if(Pimpl->LZMAStream.avail_in == 0 && !Pimpl->RawFileEof) {
            const size_t n = std::fread(Pimpl->RawBuffer, 1, sizeof(Pimpl->RawBuffer), Pimpl->File);
            if(n == 0) {
               Pimpl->RawFileEof = true;
            }
            Pimpl->LZMAStream.next_in  = Pimpl->RawBuffer;
            Pimpl->LZMAStream.avail_in = n;
         }

         const lzma_action action = Pimpl->RawFileEof ? LZMA_FINISH : LZMA_RUN;
         const lzma_ret ret = lzma_code(&Pimpl->LZMAStream, action);

         if(ret == LZMA_STREAM_END) {
            Pimpl->StreamEof = true;
            break;
         }
         else if(ret != LZMA_OK) {
            throw InputFileException("Failed to decompress XZ data",
                                     Name, static_cast<int>(ret), lzmaErrorToString(static_cast<int>(ret)));
         }
         else if(Pimpl->LZMAStream.avail_in == 0 && Pimpl->RawFileEof) {
            break;
         }
      }
      return delivered + (bytesToRead - Pimpl->LZMAStream.avail_out);
   }

   // ====== GZip ===========================================================
   else if(Format == FileFormat::GZip) {
      const int count = gzread(Pimpl->GZFile, dest, static_cast<unsigned int>(bytesToRead));
      if(count < 0) {
         int errnum = 0;
         const char* reason = gzerror(Pimpl->GZFile, &errnum);
         throw InputFileException("Failed to read from GZip file", Name, errnum, reason ? reason : "zlib error");
      }
      return delivered + static_cast<size_t>(count);
   }

   // ====== BZip2 ==========================================================
   else if(Format == FileFormat::BZip2) {
      if(Pimpl->StreamEof) {
         return delivered;
      }
      int bzerror = BZ_OK;
      const int count = BZ2_bzRead(&bzerror, Pimpl->BZFile, dest, static_cast<int>(bytesToRead));
      if(bzerror == BZ_STREAM_END) {
         Pimpl->StreamEof = true;
      }
      else if(bzerror != BZ_OK) {
         const char* reason = BZ2_bzerror(Pimpl->BZFile, &bzerror);
         throw InputFileException("Failed to read from BZip2 file",
                                  Name, bzerror, reason ? reason : "BZip2 error");
      }
      return delivered + (count > 0 ? static_cast<size_t>(count) : 0);
   }

   // ====== Plain text =====================================================
   else if(Format == FileFormat::Plain) {
      const size_t count = std::fread(dest, 1, bytesToRead, Pimpl->File);
      if(count == 0 && std::ferror(Pimpl->File)) {
         throw InputFileException("Failed to read from file", Name, errno);
      }
      return delivered + count;
   }

   return delivered;
}


// ###### Read line from file (fixed buffer) ################################
ssize_t InputFile::readLine(char* const buffer, size_t bufferSize, bool& eof)
{
   eof = false;

   if(bufferSize < 1 || !Pimpl) {
      return -1;
   }
   bufferSize--; // Reserve space for terminating null character

   size_t currentLen = 0;
   for(;;) {
      // Check if accumulated buffer already contains a newline character
      for(size_t i = 0; i < currentLen; ++i) {
         if(buffer[i] == '\n') {
            const size_t remaining = currentLen - i - 1;
            if(remaining > sizeof(Pimpl->Storage)) {
               throw InputFileException("Buffer overflow while storing remaining line data", Name);
            }
            if(remaining > 0) {
               std::memcpy(Pimpl->Storage, buffer + i + 1, remaining);
            }
            Pimpl->StoragePos = remaining;
            buffer[i]  = 0x00;
            Line++;
            return static_cast<ssize_t>(i);
         }
      }

      if(currentLen >= bufferSize) {
         throw InputFileException(
            "Line " + std::to_string(Line + 1) + " is too long to fit into buffer",
            Name);
      }

      const size_t bytesToRead = std::min(sizeof(Pimpl->Storage), bufferSize - currentLen);
      const size_t n = read(buffer + currentLen, bytesToRead);
      if(n == 0) {
         if(currentLen == 0) {
            eof = true;
            buffer[0] = 0x00;
            return 0;
         }

         // File ended without trailing newline
         buffer[currentLen] = 0x00;
         Line++;
         return static_cast<ssize_t>(currentLen);
      }

      currentLen += n;
   }
}


// ###### Read line from file (std::string) #################################
bool InputFile::readLine(std::string& line)
{
   line.clear();
   char chunk[4096];
   bool eof = false;

   const ssize_t bytesRead = readLine(chunk, sizeof(chunk), eof);
   if(bytesRead <= 0) {
      return false;
   }

   line.assign(chunk, static_cast<size_t>(bytesRead));
   return true;
}
