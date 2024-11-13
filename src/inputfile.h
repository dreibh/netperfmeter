/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2025 by Thomas Dreibholz
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

#ifndef INPUTFILE_H
#define INPUTFILE_H

#include <stdio.h>
#include <bzlib.h>
#include <string>
#include <iostream>


// Input File Formats
enum InputFileFormat
{
   IFF_Plain = 1,
   IFF_BZip2 = 2
};


class InputFile
{
   // ====== Methods ========================================================
   public:
   InputFile();
   ~InputFile();

   bool initialize(const char*            name,
                   const InputFileFormat format);
   bool finish(const bool closeFile = true);

   inline bool exists() const {
      return File || BZFile;
   }
   inline InputFileFormat getFormat() const {
      return Format;
   }
   inline FILE* getFile() const {
      return File;
   }
   inline const std::string& getName() const {
      return Name;
   }
   inline unsigned long long getLine() const {
      return Line;
   }
   ssize_t readLine(char* buffer, size_t bufferSize, bool& eof);

   // ====== Private Data ===================================================
   private:
   InputFileFormat    Format;
   std::string        Name;
   unsigned long long Line;
   FILE*              File;
   BZFILE*            BZFile;
   bool               ReadError;
   size_t             StoragePos;
   char               Storage[16384];
};

#endif
