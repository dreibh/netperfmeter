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

#include "fileio.h"
#include "inputfile.h"
#include "outputfile.h"

#include <cstring>
#include <fstream>
#include <unistd.h>

const char* lzmaErrorToString(const lzma_ret ret) noexcept
{
   switch(ret) {
      case LZMA_OK:             return "Operation completed successfully";
      case LZMA_STREAM_END:     return "End of stream reached";
      case LZMA_MEM_ERROR:      return "Cannot allocate memory";
      case LZMA_MEMLIMIT_ERROR: return "Memory limit reached";
      case LZMA_FORMAT_ERROR:   return "Format not recognized";
      case LZMA_OPTIONS_ERROR:  return "Invalid compression options";
      case LZMA_DATA_ERROR:     return "Data is corrupt";
      case LZMA_BUF_ERROR:      return "Buffer error";
      case LZMA_PROG_ERROR:     return "Internal programming error";
      default:                  return "Unknown liblzma error";
   }
}

FileFormat detectFormatFromHeader(const void* data, const size_t size) noexcept
{
   if(size >= 3 && std::memcmp(data, "BZh", 3) == 0) {
      return FileFormat::BZip2;
   }
   if(size >= 2 && static_cast<const uint8_t*>(data)[0] == 0x1f &&
                   static_cast<const uint8_t*>(data)[1] == 0x8b) {
      return FileFormat::GZip;
   }
   if(size >= 6 && std::memcmp(data, "\xfd\x37\x7a\x58\x5a\x00", 6) == 0) {
      return FileFormat::XZ;
   }
   if(size >= 4 && static_cast<const uint8_t*>(data)[0] == 0x28 &&
                   static_cast<const uint8_t*>(data)[1] == 0xb5 &&
                   static_cast<const uint8_t*>(data)[2] == 0x2f &&
                   static_cast<const uint8_t*>(data)[3] == 0xfd) {
      return FileFormat::ZSTD;
   }
   return FileFormat::Plain;
}

FileFormat detectFormatFromContent(const std::filesystem::path& path)
{
   std::ifstream f(path, std::ios::binary);
   if(!f) {
      return FileFormat::Plain;
   }
   char header[6];
   f.read(header, sizeof(header));
   const size_t bytesRead = static_cast<size_t>(f.gcount());
   return detectFormatFromHeader(header, bytesRead);
}

void transformFile(const std::filesystem::path& sourcePath,
                   const FileFormat             sourceFormat,
                   const std::filesystem::path& destinationPath,
                   const FileFormat             destinationFormat,
                   const unsigned int           compressionLevel)
{
   if(sourceFormat == FileFormat::None || destinationFormat == FileFormat::None) {
      return;
   }
   if(sourceFormat == destinationFormat && sourcePath == destinationPath) {
      return;
   }

   const bool inPlace = (sourcePath == destinationPath);
   std::filesystem::path tempPath;
   if(inPlace) {
      tempPath = destinationPath;
      tempPath += ".tmp." + std::to_string(getpid());
   }
   const std::filesystem::path& writePath = inPlace ? tempPath : destinationPath;

   try {
      InputFile input;
      input.initialize(sourcePath, sourceFormat);

      OutputFile output;
      output.initialize(writePath, destinationFormat, compressionLevel);

      char buffer[65536];
      size_t bytesRead = 0;
      while((bytesRead = input.read(buffer, sizeof(buffer))) > 0) {
         output.write(buffer, bytesRead);
      }

      input.finish(true);
      output.finish(true);

      if(inPlace) {
         std::error_code ec;
         std::filesystem::rename(tempPath, destinationPath, ec);
         if(ec) {
            std::filesystem::remove(tempPath, ec);
            throw OutputFileException("Failed to rename transformed file", destinationPath.string(), ec.value());
         }
      }
   }
   catch(...) {
      if(inPlace) {
         std::error_code ec;
         std::filesystem::remove(tempPath, ec);
      }
      throw;
   }
}

void transformFile(const std::filesystem::path& sourcePath,
                   const std::filesystem::path& destinationPath,
                   const FileFormat             destinationFormat,
                   const unsigned int           compressionLevel)
{
   const FileFormat sourceFormat = detectFormatFromContent(sourcePath);
   transformFile(sourcePath, sourceFormat, destinationPath, destinationFormat, compressionLevel);
}

