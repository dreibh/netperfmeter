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

#include "inputfile.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <utility>


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
   : Format(other.Format),
     Name(std::move(other.Name)),
     Line(other.Line),
     File(other.File),
     BZFile(other.BZFile),
     GZFile(other.GZFile),
     LZMAStream(other.LZMAStream),
     LZMAInitialized(other.LZMAInitialized),
     ZSTDDStream(other.ZSTDDStream),
     RawBufferPos(other.RawBufferPos),
     RawBufferSize(other.RawBufferSize),
     RawFileEof(other.RawFileEof),
     StreamEof(other.StreamEof),
     StoragePos(other.StoragePos)
{
   if(other.RawBufferSize > 0) {
      std::memcpy(RawBuffer, other.RawBuffer, other.RawBufferSize);
   }
   if(other.StoragePos > 0) {
      std::memcpy(Storage, other.Storage, other.StoragePos);
   }

   other.Format          = InputFileFormat::None;
   other.Line            = 0;
   other.File            = nullptr;
   other.BZFile          = nullptr;
   other.GZFile          = nullptr;
   std::memset(&other.LZMAStream, 0, sizeof(lzma_stream));
   other.LZMAInitialized = false;
   other.ZSTDDStream     = nullptr;
   other.RawBufferPos    = 0;
   other.RawBufferSize   = 0;
   other.RawFileEof      = false;
   other.StreamEof       = false;
   other.StoragePos      = 0;
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

      Format          = other.Format;
      Name            = std::move(other.Name);
      Line            = other.Line;
      File            = other.File;
      BZFile          = other.BZFile;
      GZFile          = other.GZFile;
      LZMAStream      = other.LZMAStream;
      LZMAInitialized = other.LZMAInitialized;
      ZSTDDStream     = other.ZSTDDStream;
      RawBufferPos    = other.RawBufferPos;
      RawBufferSize   = other.RawBufferSize;
      RawFileEof      = other.RawFileEof;
      StreamEof       = other.StreamEof;
      StoragePos      = other.StoragePos;

      if(other.RawBufferSize > 0) {
         std::memcpy(RawBuffer, other.RawBuffer, other.RawBufferSize);
      }
      if(other.StoragePos > 0) {
         std::memcpy(Storage, other.Storage, other.StoragePos);
      }

      other.Format          = InputFileFormat::None;
      other.Line            = 0;
      other.File            = nullptr;
      other.BZFile          = nullptr;
      other.GZFile          = nullptr;
      std::memset(&other.LZMAStream, 0, sizeof(lzma_stream));
      other.LZMAInitialized = false;
      other.ZSTDDStream     = nullptr;
      other.RawBufferPos    = 0;
      other.RawBufferSize   = 0;
      other.RawFileEof      = false;
      other.StreamEof       = false;
      other.StoragePos      = 0;
   }
   return *this;
}


// ###### Initialize input file (const char*) ###############################
void InputFile::initialize(const char* const     name,
                           const InputFileFormat format)
{
   initialize(name ? std::filesystem::path(name) : std::filesystem::path{}, format);
}


// ###### Initialize input file (std::filesystem::path) #####################
void InputFile::initialize(const std::filesystem::path& name,
                           const InputFileFormat        format)
{
   finish();

   StoragePos      = 0;
   Line            = 0;
   Format          = format;
   Name            = name.string();
   File            = nullptr;
   BZFile          = nullptr;
   GZFile          = nullptr;
   std::memset(&LZMAStream, 0, sizeof(lzma_stream));
   LZMAInitialized = false;
   ZSTDDStream     = nullptr;
   RawBufferPos    = 0;
   RawBufferSize   = 0;
   RawFileEof      = false;
   StreamEof       = false;

   if(format == InputFileFormat::None) {
      return;
   }
   else if(format == InputFileFormat::GZip) {
      if(!Name.empty()) {
         GZFile = gzopen(Name.c_str(), "rb");
      }
      if(GZFile == nullptr) {
         throw InputFileException("Unable to open GZip input file", Name, errno);
      }
   }
   else {
      File = !Name.empty() ? std::fopen(Name.c_str(), "rb") : std::tmpfile();
      if(File == nullptr) {
         throw InputFileException("Unable to open input file", Name, errno);
      }

      if(format == InputFileFormat::BZip2) {
         int bzerror = BZ_OK;
         BZFile = BZ2_bzReadOpen(&bzerror, File, 0, 0, nullptr, 0);
         if(bzerror != BZ_OK || BZFile == nullptr) {
            const int err = bzerror;
            const char* reason = BZ2_bzerror(BZFile, &bzerror);
            BZ2_bzReadClose(&bzerror, BZFile);
            BZFile = nullptr;
            std::fclose(File);
            File = nullptr;
            throw InputFileException("Unable to initialize BZip2 decompression",
                                     Name, err, reason ? reason : "Unknown BZip2 error");
         }
      }
      else if(format == InputFileFormat::XZ) {
         const lzma_ret ret = lzma_auto_decoder(&LZMAStream, UINT64_MAX, 0);
         if(ret != LZMA_OK) {
            std::fclose(File);
            File = nullptr;
            throw InputFileException("Unable to initialize XZ decompression",
                                     Name, static_cast<int>(ret), lzmaErrorToString(ret));
         }
         LZMAStream.next_in  = nullptr;
         LZMAStream.avail_in = 0;
         LZMAInitialized     = true;
      }
      else if(format == InputFileFormat::ZSTD) {
         ZSTDDStream = ZSTD_createDStream();
         if(ZSTDDStream == nullptr) {
            std::fclose(File);
            File = nullptr;
            throw InputFileException("Unable to create Zstandard decompression stream", Name);
         }
         const size_t ret = ZSTD_initDStream(ZSTDDStream);
         if(ZSTD_isError(ret)) {
            ZSTD_freeDStream(ZSTDDStream);
            ZSTDDStream = nullptr;
            std::fclose(File);
            File = nullptr;
            throw InputFileException("Unable to initialize Zstandard decompression",
                                     Name, static_cast<int>(ret), ZSTD_getErrorName(ret));
         }
      }
   }
}


// ###### Finish input file #################################################
void InputFile::finish(const bool closeFile)
{
   if(ZSTDDStream) {
      ZSTD_freeDStream(ZSTDDStream);
      ZSTDDStream = nullptr;
   }

   if(LZMAInitialized) {
      lzma_end(&LZMAStream);
      std::memset(&LZMAStream, 0, sizeof(lzma_stream));
      LZMAInitialized = false;
   }

   if(GZFile) {
      gzclose(GZFile);
      GZFile = nullptr;
   }

   if(BZFile) {
      int bzerror = BZ_OK;
      BZ2_bzReadClose(&bzerror, BZFile);
      BZFile = nullptr;
   }

   if(File) {
      if(closeFile) {
         if(std::fclose(File) != 0) {
            File = nullptr;
            throw InputFileException("Unable to close input file", Name, errno);
         }
         File = nullptr;
      }
      else {
         std::rewind(File);
      }
   }

   RawBufferPos  = 0;
   RawBufferSize = 0;
   RawFileEof    = false;
   StreamEof     = false;
}


// ###### Bulk read from file ###############################################
size_t InputFile::read(char* const buffer, const size_t bufferSize)
{
   if(!exists() || bufferSize == 0) {
      return 0;
   }

   size_t delivered = 0;

   // 1. Drain any leftover data in Storage
   if(StoragePos > 0) {
      const size_t toCopy = std::min(bufferSize, StoragePos);
      std::memcpy(buffer, Storage, toCopy);
      delivered += toCopy;
      if(toCopy < StoragePos) {
         std::memmove(Storage, Storage + toCopy, StoragePos - toCopy);
         StoragePos -= toCopy;
         return delivered;
      }
      StoragePos = 0;
      if(delivered == bufferSize) {
         return delivered;
      }
   }

   // 2. Read directly from the decompressor into destination buffer
   const size_t bytesToRead = bufferSize - delivered;
   char* const  dest        = buffer + delivered;

   // ====== Zstandard ======================================================
   if(Format == InputFileFormat::ZSTD) {
      ZSTD_outBuffer out = { dest, bytesToRead, 0 };
      while(out.pos < out.size) {
         if(RawBufferPos >= RawBufferSize && !RawFileEof) {
            const size_t n = std::fread(RawBuffer, 1, sizeof(RawBuffer), File);
            if(n == 0) {
               RawFileEof = true;
            }
            RawBufferPos  = 0;
            RawBufferSize = n;
         }
         if(RawBufferPos >= RawBufferSize && RawFileEof) {
            break;
         }

         ZSTD_inBuffer in = { RawBuffer, RawBufferSize, RawBufferPos };
         const size_t ret = ZSTD_decompressStream(ZSTDDStream, &out, &in);
         RawBufferPos = in.pos;

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
   else if(Format == InputFileFormat::XZ) {
      if(StreamEof) {
         return delivered;
      }
      LZMAStream.next_out  = reinterpret_cast<uint8_t*>(dest);
      LZMAStream.avail_out = bytesToRead;

      while(LZMAStream.avail_out > 0) {
         if(LZMAStream.avail_in == 0 && !RawFileEof) {
            const size_t n = std::fread(RawBuffer, 1, sizeof(RawBuffer), File);
            if(n == 0) {
               RawFileEof = true;
            }
            LZMAStream.next_in  = RawBuffer;
            LZMAStream.avail_in = n;
         }

         const lzma_action action = RawFileEof ? LZMA_FINISH : LZMA_RUN;
         const lzma_ret ret = lzma_code(&LZMAStream, action);

         if(ret == LZMA_STREAM_END) {
            StreamEof = true;
            break;
         }
         else if(ret != LZMA_OK) {
            throw InputFileException("Failed to decompress XZ data",
                                     Name, static_cast<int>(ret), lzmaErrorToString(ret));
         }
         else if(LZMAStream.avail_in == 0 && RawFileEof) {
            break;
         }
      }
      return delivered + (bytesToRead - LZMAStream.avail_out);
   }

   // ====== GZip ===========================================================
   else if(Format == InputFileFormat::GZip) {
      const int count = gzread(GZFile, dest, static_cast<unsigned int>(bytesToRead));
      if(count < 0) {
         int errnum = 0;
         const char* reason = gzerror(GZFile, &errnum);
         throw InputFileException("Failed to read from GZip file", Name, errnum, reason ? reason : "zlib error");
      }
      return delivered + static_cast<size_t>(count);
   }

   // ====== BZip2 ==========================================================
   else if(Format == InputFileFormat::BZip2) {
      if(StreamEof) {
         return delivered;
      }
      int bzerror = BZ_OK;
      const int count = BZ2_bzRead(&bzerror, BZFile, dest, static_cast<int>(bytesToRead));
      if(bzerror == BZ_STREAM_END) {
         StreamEof = true;
      }
      else if(bzerror != BZ_OK) {
         const char* reason = BZ2_bzerror(BZFile, &bzerror);
         throw InputFileException("Failed to read from BZip2 file",
                                  Name, bzerror, reason ? reason : "BZip2 error");
      }
      return delivered + (count > 0 ? static_cast<size_t>(count) : 0);
   }

   // ====== Plain text =====================================================
   else if(Format == InputFileFormat::Plain) {
      const size_t count = std::fread(dest, 1, bytesToRead, File);
      if(count == 0 && std::ferror(File)) {
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

   if(bufferSize < 1) {
      return -1;
   }
   bufferSize--; // Reserve space for terminating null character

   size_t currentLen = 0;
   for(;;) {
      // Check if accumulated buffer already contains a newline character
      for(size_t i = 0; i < currentLen; ++i) {
         if(buffer[i] == '\n') {
            const size_t remaining = currentLen - i - 1;
            if(remaining > sizeof(Storage)) {
               throw InputFileException("Buffer overflow while storing remaining line data", Name);
            }
            if(remaining > 0) {
               std::memcpy(Storage, buffer + i + 1, remaining);
            }
            StoragePos = remaining;
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

      const size_t bytesToRead = std::min(sizeof(Storage), bufferSize - currentLen);
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
