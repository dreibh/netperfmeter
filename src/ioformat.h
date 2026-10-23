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

#ifndef IOFORMAT_H
#define IOFORMAT_H

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <zstd.h>


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

// Aliases for unified input and output handling
using OutputFileFormat = FileFormat;
using InputFileFormat  = FileFormat;

inline constexpr FileFormat OFF_None  = FileFormat::None;
inline constexpr FileFormat OFF_Plain = FileFormat::Plain;
inline constexpr FileFormat OFF_BZip2 = FileFormat::BZip2;
inline constexpr FileFormat OFF_GZip  = FileFormat::GZip;
inline constexpr FileFormat OFF_XZ    = FileFormat::XZ;
inline constexpr FileFormat OFF_ZSTD  = FileFormat::ZSTD;

inline constexpr FileFormat IFF_None  = FileFormat::None;
inline constexpr FileFormat IFF_Plain = FileFormat::Plain;
inline constexpr FileFormat IFF_BZip2 = FileFormat::BZip2;
inline constexpr FileFormat IFF_GZip  = FileFormat::GZip;
inline constexpr FileFormat IFF_XZ    = FileFormat::XZ;
inline constexpr FileFormat IFF_ZSTD  = FileFormat::ZSTD;


// ###### Detect format from file path ======================================
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


// ###### Parse format name string ==========================================
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


// ###### Get default file extension for format =============================
[[nodiscard]] constexpr std::string_view getFormatExtension(const FileFormat format) noexcept
{
   switch(format) {
      case FileFormat::BZip2:
         return ".bz2";
      case FileFormat::GZip:
         return ".gz";
      case FileFormat::XZ:
         return ".xz";
      case FileFormat::ZSTD:
         return ".zst";
      case FileFormat::Plain:
      case FileFormat::None:
      default:
         return "";
   }
}


// ###### Get textual name of format ========================================
[[nodiscard]] constexpr std::string_view getFormatName(const FileFormat format) noexcept
{
   switch(format) {
      case FileFormat::BZip2:
         return "BZip2";
      case FileFormat::GZip:
         return "GZip";
      case FileFormat::XZ:
         return "XZ";
      case FileFormat::ZSTD:
         return "Zstandard";
      case FileFormat::Plain:
         return "Plain";
      case FileFormat::None:
      default:
         return "None";
   }
}


// ###### Get default compression level for format ==========================
[[nodiscard]] constexpr unsigned int getDefaultCompressionLevel(const FileFormat format) noexcept
{
   switch(format) {
      case FileFormat::BZip2:
         return 9;
      case FileFormat::GZip:
      case FileFormat::XZ:
         return 6;
      case FileFormat::ZSTD:
         return 3;
      case FileFormat::Plain:
      case FileFormat::None:
      default:
         return 0;
   }
}


// ###### Get minimum compression level for format ==========================
[[nodiscard]] constexpr unsigned int getMinCompressionLevel(const FileFormat format) noexcept
{
   switch(format) {
      case FileFormat::BZip2:
      case FileFormat::ZSTD:
         return 1;
      case FileFormat::GZip:
      case FileFormat::XZ:
      case FileFormat::Plain:
      case FileFormat::None:
      default:
         return 0;
   }
}


// ###### Get maximum compression level for format ==========================
[[nodiscard]] inline unsigned int getMaxCompressionLevel(const FileFormat format) noexcept
{
   switch(format) {
      case FileFormat::BZip2:
      case FileFormat::GZip:
      case FileFormat::XZ:
         return 9;
      case FileFormat::ZSTD:
#if defined(ZSTD_CLEVEL_DEFAULT)
         return static_cast<unsigned int>(ZSTD_maxCLevel());
#else
         return 22;
#endif
      case FileFormat::Plain:
      case FileFormat::None:
      default:
         return 0;
   }
}


// ###### Validate compression level for format =============================
[[nodiscard]] inline bool isValidCompressionLevel(const FileFormat format, const int level) noexcept
{
   if(level < 0) {
      return false;
   }
   const unsigned int uLevel = static_cast<unsigned int>(level);
   return uLevel >= getMinCompressionLevel(format) && uLevel <= getMaxCompressionLevel(format);
}

#endif // IOFORMAT_H
