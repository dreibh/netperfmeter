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

#ifndef FILEIO_H
#define FILEIO_H

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>


// ====== I/O File Formats ==================================================
enum class FileFormat : uint8_t
{
   None  = 0,
   Plain = 1,
   BZip2 = 2,
   GZip  = 3,
   XZ    = 4,
   ZSTD  = 5
};


// ====== Shared Error String Helpers =======================================
[[nodiscard]] const char* lzmaErrorToString(int returnCode) noexcept;


// ====== Format Detection ==================================================
[[nodiscard]] inline FileFormat detectFormat(const std::filesystem::path& path)
{
   std::string ext = path.extension().string();
   for(char& c : ext) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
   }

   if(ext == ".bz2") {
      return FileFormat::BZip2;
   }
   else if(ext == ".gz") {
      return FileFormat::GZip;
   }
   else if(ext == ".xz" || ext == ".lzma") {
      return FileFormat::XZ;
   }
   else if(ext == ".zst" || ext == ".zstd") {
      return FileFormat::ZSTD;
   }
   else {
      return FileFormat::Plain;
   }
}

[[nodiscard]] FileFormat detectFormatFromHeader(const void* data, size_t size) noexcept;
[[nodiscard]] FileFormat detectFormatFromContent(const std::filesystem::path& path);

[[nodiscard]] inline FileFormat parseFormat(std::string_view str)
{
   std::string lower(str);
   for(char& c : lower) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
   }

   if(lower == "plain" || lower == "none") {
      return FileFormat::Plain;
   }
   else if(lower == "bz2" || lower == "bzip2") {
      return FileFormat::BZip2;
   }
   else if(lower == "gz" || lower == "gzip") {
      return FileFormat::GZip;
   }
   else if(lower == "xz" || lower == "lzma") {
      return FileFormat::XZ;
   }
   else if(lower == "zst" || lower == "zstd" || lower == "zstandard") {
      return FileFormat::ZSTD;
   }
   else {
      return FileFormat::None;
   }
}

[[nodiscard]] constexpr std::string_view getFormatExtension(const FileFormat format) noexcept
{
   switch(format) {
      case FileFormat::BZip2: return ".bz2";
      case FileFormat::GZip:  return ".gz";
      case FileFormat::XZ:    return ".xz";
      case FileFormat::ZSTD:  return ".zst";
      case FileFormat::Plain:
      case FileFormat::None:
      default:                return "";
   }
}

[[nodiscard]] constexpr std::string_view getFormatName(const FileFormat format) noexcept
{
   switch(format) {
      case FileFormat::BZip2: return "BZip2";
      case FileFormat::GZip:  return "GZip";
      case FileFormat::XZ:    return "XZ";
      case FileFormat::ZSTD:  return "Zstandard";
      case FileFormat::Plain: return "Plain";
      case FileFormat::None:
      default:                return "None";
   }
}

[[nodiscard]] constexpr unsigned int getDefaultCompressionLevel(const FileFormat format) noexcept
{
   switch(format) {
      case FileFormat::BZip2: return 9;
      case FileFormat::GZip:
      case FileFormat::XZ:    return 6;
      case FileFormat::ZSTD:  return 3;
      case FileFormat::Plain:
      case FileFormat::None:
      default:                return 0;
   }
}

[[nodiscard]] constexpr unsigned int getMinCompressionLevel(const FileFormat format) noexcept
{
   switch(format) {
      case FileFormat::BZip2:
      case FileFormat::ZSTD:  return 1;
      case FileFormat::GZip:
      case FileFormat::XZ:
      case FileFormat::Plain:
      case FileFormat::None:
      default:                return 0;
   }
}

[[nodiscard]] unsigned int getMaxCompressionLevel(FileFormat format) noexcept;
[[nodiscard]] bool isValidCompressionLevel(FileFormat format, int level) noexcept;


// ====== File Transformation ===============================================
void transformFile(const std::filesystem::path& sourcePath,
                   FileFormat                   sourceFormat,
                   const std::filesystem::path& destinationPath,
                   FileFormat                   destinationFormat,
                   unsigned int                 compressionLevel = 0);

void transformFile(const std::filesystem::path& sourcePath,
                   const std::filesystem::path& destinationPath,
                   FileFormat                   destinationFormat,
                   unsigned int                 compressionLevel = 0);

#endif // FILEIO_H
