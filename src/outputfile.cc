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

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <unistd.h>
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
     GZFile(other.GZFile),
     LZMAStream(other.LZMAStream),
     LZMAInitialized(other.LZMAInitialized),
     ZSTDCStream(other.ZSTDCStream),
     PlainBytesIn(other.PlainBytesIn),
     CompressedBytesOut(other.CompressedBytesOut),
     WriteError(other.WriteError)
{
   other.Format             = OutputFileFormat::None;
   other.Line               = 0;
   other.File               = nullptr;
   other.BZFile             = nullptr;
   other.GZFile             = nullptr;
   std::memset(&other.LZMAStream, 0, sizeof(lzma_stream));
   other.LZMAInitialized    = false;
   other.ZSTDCStream        = nullptr;
   other.PlainBytesIn       = 0;
   other.CompressedBytesOut = 0;
   other.WriteError         = false;
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

      Format             = other.Format;
      Name               = std::move(other.Name);
      Line               = other.Line;
      File               = other.File;
      BZFile             = other.BZFile;
      GZFile             = other.GZFile;
      LZMAStream         = other.LZMAStream;
      LZMAInitialized    = other.LZMAInitialized;
      ZSTDCStream        = other.ZSTDCStream;
      PlainBytesIn       = other.PlainBytesIn;
      CompressedBytesOut = other.CompressedBytesOut;
      WriteError         = other.WriteError;

      other.Format             = OutputFileFormat::None;
      other.Line               = 0;
      other.File               = nullptr;
      other.BZFile             = nullptr;
      other.GZFile             = nullptr;
      std::memset(&other.LZMAStream, 0, sizeof(lzma_stream));
      other.LZMAInitialized    = false;
      other.ZSTDCStream        = nullptr;
      other.PlainBytesIn       = 0;
      other.CompressedBytesOut = 0;
      other.WriteError         = false;
   }
   return *this;
}


// ###### Initialize output file (const char*) ##############################
void OutputFile::initialize(const char* const     name,
                            const OutputFileFormat format,
                            const unsigned int     compressionLevel)
{
   initialize(name ? std::filesystem::path(name) : std::filesystem::path{}, format, compressionLevel);
}


// ###### Initialize output file (std::filesystem::path) ####################
void OutputFile::initialize(const std::filesystem::path& name,
                            const OutputFileFormat       format,
                            const unsigned int           compressionLevel)
{
   finish();

   Line               = 0;
   Format             = format;
   Name               = name.string();
   PlainBytesIn       = 0;
   CompressedBytesOut = 0;
   File               = nullptr;
   BZFile             = nullptr;
   GZFile             = nullptr;
   std::memset(&LZMAStream, 0, sizeof(lzma_stream));
   LZMAInitialized    = false;
   ZSTDCStream        = nullptr;

   if(format == OutputFileFormat::None) {
      return;
   }

   // ====== Initialize GZip compressor =====================================
   if(format == OutputFileFormat::GZip) {
      const unsigned int level = (compressionLevel == 0) ? 6u : std::clamp(compressionLevel, 1u, 9u);
      const std::string mode = "wb" + std::to_string(level);

      if(!Name.empty()) {
         errno = 0;
         GZFile = gzopen(Name.c_str(), mode.c_str());
         if(GZFile == nullptr) {
            WriteError = true;
            throw OutputFileException("Unable to create GZip output file", Name, errno);
         }
      }
      else {
         // Passive instance: buffer output in a temporary file for subsequent upload
         errno = 0;
         File = std::tmpfile();
         if(File == nullptr) {
            WriteError = true;
            throw OutputFileException("Unable to create temporary output file", Name, errno);
         }

         const int fd = dup(fileno(File));
         if(fd < 0) {
            std::fclose(File);
            File = nullptr;
            WriteError = true;
            throw OutputFileException("Unable to duplicate file descriptor for GZip", Name, errno);
         }

         errno = 0;
         GZFile = gzdopen(fd, mode.c_str());
         if(GZFile == nullptr) {
            close(fd);
            std::fclose(File);
            File = nullptr;
            WriteError = true;
            throw OutputFileException("Unable to create GZip output file", Name, errno);
         }
      }
   }

   // ====== Initialize Other Formats (Plain, BZ2, XZ, ZSTD) =================
   else {
      errno = 0;
      File = !Name.empty() ? std::fopen(Name.c_str(), "w+b") : std::tmpfile();
      if(File == nullptr) {
         WriteError = true;
         throw OutputFileException("Unable to create output file", Name, errno);
      }

      // ====== Initialize BZip2 compressor =================================
      if(format == OutputFileFormat::BZip2) {
         int bzerror = BZ_OK;
         const unsigned int level = (compressionLevel == 0) ? 9u : std::clamp(compressionLevel, 1u, 9u);
         BZFile = BZ2_bzWriteOpen(&bzerror, File, static_cast<int>(level), 0, 30);
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

      // ====== Initialize XZ compressor ====================================
      else if(format == OutputFileFormat::XZ) {
         const unsigned int level = (compressionLevel == 0) ? 6u : std::min(compressionLevel, 9u);
         const lzma_ret ret = lzma_easy_encoder(&LZMAStream, level, LZMA_CHECK_CRC64);
         if(ret != LZMA_OK) {
            std::fclose(File);
            File = nullptr;
            WriteError = true;
            if(!Name.empty()) {
               std::error_code ec;
               std::filesystem::remove(Name, ec);
            }
            throw OutputFileException("Unable to initialize XZ compression on file",
                                      Name, static_cast<int>(ret), lzmaErrorToString(ret));
         }
         LZMAInitialized = true;
      }

      // ====== Initialize Zstandard compressor ==============================
      else if(format == OutputFileFormat::ZSTD) {
         ZSTDCStream = ZSTD_createCStream();
         if(ZSTDCStream == nullptr) {
            std::fclose(File);
            File = nullptr;
            WriteError = true;
            if(!Name.empty()) {
               std::error_code ec;
               std::filesystem::remove(Name, ec);
            }
            throw OutputFileException("Unable to create Zstandard compression stream", Name);
         }

         const int zLevel = (compressionLevel == 0) ? ZSTD_defaultCLevel() : static_cast<int>(compressionLevel);
         const size_t ret = ZSTD_initCStream(ZSTDCStream, zLevel);
         if(ZSTD_isError(ret)) {
            ZSTD_freeCStream(ZSTDCStream);
            ZSTDCStream = nullptr;
            std::fclose(File);
            File = nullptr;
            WriteError = true;
            if(!Name.empty()) {
               std::error_code ec;
               std::filesystem::remove(Name, ec);
            }
            throw OutputFileException("Unable to initialize Zstandard compression",
                                      Name, static_cast<int>(ret), ZSTD_getErrorName(ret));
         }
      }
   }

   WriteError = false;
}


// ###### Finish output file ================================================
void OutputFile::finish(const bool          closeFile,
                        unsigned long long* bytesIn,
                        unsigned long long* bytesOut)
{
   std::exception_ptr errorToThrow = nullptr;

   // ====== Finish Zstandard compression ===================================
   if(ZSTDCStream) {
      ZSTD_inBuffer in = { nullptr, 0, 0 };
      uint8_t outputBuffer[16384];
      size_t  remaining = 1;
      while(remaining > 0) {
         ZSTD_outBuffer out = { outputBuffer, sizeof(outputBuffer), 0 };
         remaining = ZSTD_compressStream2(ZSTDCStream, &out, &in, ZSTD_e_end);
         if(ZSTD_isError(remaining)) {
            WriteError = true;
            errorToThrow = std::make_exception_ptr(
               OutputFileException("Failed to finish Zstandard compression",
                                   Name, static_cast<int>(remaining), ZSTD_getErrorName(remaining)));
            break;
         }
         if(out.pos > 0 && File != nullptr) {
            if(std::fwrite(outputBuffer, 1, out.pos, File) != out.pos) {
               WriteError = true;
               errorToThrow = std::make_exception_ptr(
                  OutputFileException("Failed to write Zstandard compressed data", Name, errno));
               break;
            }
            CompressedBytesOut += out.pos;
         }
      }
      ZSTD_freeCStream(ZSTDCStream);
      ZSTDCStream = nullptr;
      if(bytesIn)  { *bytesIn  = PlainBytesIn; }
      if(bytesOut) { *bytesOut = CompressedBytesOut; }
   }

   // ====== Finish XZ compression ==========================================
   else if(LZMAInitialized) {
      if(bytesIn)  { *bytesIn  = LZMAStream.total_in; }
      lzma_ret ret = LZMA_OK;
      uint8_t outputBuffer[16384];
      do {
         LZMAStream.next_out  = outputBuffer;
         LZMAStream.avail_out = sizeof(outputBuffer);
         ret = lzma_code(&LZMAStream, LZMA_FINISH);
         if(ret != LZMA_OK && ret != LZMA_STREAM_END) {
            WriteError = true;
            errorToThrow = std::make_exception_ptr(
               OutputFileException("Failed to finish XZ compression",
                                   Name, static_cast<int>(ret), lzmaErrorToString(ret)));
            break;
         }
         const size_t writeBytes = sizeof(outputBuffer) - LZMAStream.avail_out;
         if(writeBytes > 0 && File != nullptr) {
            if(std::fwrite(outputBuffer, 1, writeBytes, File) != writeBytes) {
               WriteError = true;
               errorToThrow = std::make_exception_ptr(
                  OutputFileException("Failed to write XZ compressed data", Name, errno));
               break;
            }
            CompressedBytesOut += writeBytes;
         }
      } while(ret != LZMA_STREAM_END);

      if(bytesOut) { *bytesOut = LZMAStream.total_out; }
      lzma_end(&LZMAStream);
      std::memset(&LZMAStream, 0, sizeof(lzma_stream));
      LZMAInitialized = false;
   }

   // ====== Finish GZip compression ========================================
   else if(GZFile) {
      if(bytesIn) { *bytesIn = PlainBytesIn; }
      const int gzerr = gzclose(GZFile);
      GZFile = nullptr;
      if(gzerr != Z_OK) {
         WriteError = true;
         errorToThrow = std::make_exception_ptr(
            OutputFileException("Failed to close GZip output file", Name, gzerr));
      }
      if(bytesOut) {
         if(File != nullptr) {
            std::fflush(File);
            std::fseek(File, 0, SEEK_END);
            const long pos = std::ftell(File);
            *bytesOut = (pos >= 0) ? static_cast<unsigned long long>(pos) : PlainBytesIn;
         }
         else if(!Name.empty()) {
            std::error_code ec;
            const auto sz = std::filesystem::file_size(Name, ec);
            *bytesOut = !ec ? static_cast<unsigned long long>(sz) : PlainBytesIn;
         }
         else {
            *bytesOut = PlainBytesIn;
         }
      }
   }

   // ====== Finish BZip2 compression =======================================
   else if(BZFile) {
      int          bzerror = BZ_OK;
      unsigned int inLow = 0, inHigh = 0, outLow = 0, outHigh = 0;
      BZ2_bzWriteClose64(&bzerror, BZFile, 0, &inLow, &inHigh, &outLow, &outHigh);
      if(bzerror == BZ_OK) {
         if(bytesIn)  { *bytesIn  = (static_cast<unsigned long long>(inHigh) << 32) + inLow; }
         if(bytesOut) { *bytesOut = (static_cast<unsigned long long>(outHigh) << 32) + outLow; }
      }
      else {
         const char* reason = BZ2_bzerror(BZFile, &bzerror);
         WriteError = true;
         errorToThrow = std::make_exception_ptr(
            OutputFileException("Failed to finish BZip2 compression",
                                Name, bzerror, reason ? reason : "Unknown BZip2 error"));
      }
      BZFile = nullptr;
   }
   else {
      if(bytesIn)  { *bytesIn  = PlainBytesIn; }
      if(bytesOut) { *bytesOut = PlainBytesIn; }
   }

   // ====== Close underlying FILE* (for Plain, BZ2, GZ temp, XZ, ZSTD) ======
   if(File) {
      if(closeFile) {
         if(std::fclose(File) != 0) {
            WriteError = true;
            if(!errorToThrow) {
               errorToThrow = std::make_exception_ptr(
                  OutputFileException("Unable to close output file", Name, errno));
            }
         }
         File = nullptr;
      }
      else {
         std::rewind(File);
      }
   }

   // ====== Remove file on error ===========================================
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


// ###### Write line into output file #######################################
void OutputFile::writeLine(std::string_view line)
{
   write(line);
   write("\n");
   Line++;
}


// ###### Write raw buffer into output file #################################
void OutputFile::write(const char* buffer, const size_t bufferLength)
{
   if(!exists() || bufferLength == 0) {
      return;
   }

   PlainBytesIn += bufferLength;

   // ====== Zstandard ======================================================
   if(ZSTDCStream) {
      ZSTD_inBuffer in = { buffer, bufferLength, 0 };
      uint8_t outputBuffer[16384];
      while(in.pos < in.size) {
         ZSTD_outBuffer out = { outputBuffer, sizeof(outputBuffer), 0 };
         const size_t ret = ZSTD_compressStream2(ZSTDCStream, &out, &in, ZSTD_e_continue);
         if(ZSTD_isError(ret)) {
            WriteError = true;
            throw OutputFileException("libzstd failed to compress data",
                                      Name, static_cast<int>(ret), ZSTD_getErrorName(ret));
         }
         if(out.pos > 0) {
            if(std::fwrite(outputBuffer, 1, out.pos, File) != out.pos) {
               WriteError = true;
               throw OutputFileException("Failed to write Zstandard data", Name, errno);
            }
            CompressedBytesOut += out.pos;
         }
      }
   }

   // ====== XZ =============================================================
   else if(LZMAInitialized) {
      LZMAStream.next_in  = reinterpret_cast<const uint8_t*>(buffer);
      LZMAStream.avail_in = bufferLength;

      uint8_t outputBuffer[16384];
      while(LZMAStream.avail_in > 0) {
         LZMAStream.next_out  = outputBuffer;
         LZMAStream.avail_out = sizeof(outputBuffer);
         const lzma_ret ret = lzma_code(&LZMAStream, LZMA_RUN);
         if(ret != LZMA_OK) {
            WriteError = true;
            throw OutputFileException("liblzma failed to compress data",
                                      Name, static_cast<int>(ret), lzmaErrorToString(ret));
         }
         const size_t writeBytes = sizeof(outputBuffer) - LZMAStream.avail_out;
         if(writeBytes > 0) {
            if(std::fwrite(outputBuffer, 1, writeBytes, File) != writeBytes) {
               WriteError = true;
               throw OutputFileException("Failed to write XZ data into file", Name, errno);
            }
            CompressedBytesOut += writeBytes;
         }
      }
   }

   // ====== GZip ===========================================================
   else if(GZFile) {
      const int bytesWritten = gzwrite(GZFile, buffer, static_cast<unsigned int>(bufferLength));
      if(bytesWritten <= 0) {
         WriteError = true;
         int errnum = 0;
         const char* reason = gzerror(GZFile, &errnum);
         throw OutputFileException("zlib failed to write data", Name, errnum, reason ? reason : "GZip error");
      }
   }

   // ====== BZip2 ==========================================================
   else if(BZFile) {
      int bzerror = BZ_OK;
      BZ2_bzWrite(&bzerror, BZFile, const_cast<char*>(buffer), static_cast<int>(bufferLength));
      if(bzerror != BZ_OK) {
         WriteError = true;
         const char* reason = BZ2_bzerror(BZFile, &bzerror);
         throw OutputFileException("libbz2 failed to write data", Name, bzerror, reason ? reason : "BZip2 error");
      }
   }

   // ====== Plain text =====================================================
   else if(File) {
      if(std::fwrite(buffer, bufferLength, 1, File) != 1) {
         WriteError = true;
         throw OutputFileException("Failed to write to file", Name, errno);
      }
   }
}
