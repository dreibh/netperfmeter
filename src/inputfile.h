/* $Id$
 *
 * Plain/BZip2 File Input
 * Copyright (C) 2009-2010 by Thomas Dreibholz
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
 * You should have relReceived a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: dreibh@iem.uni-due.de
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
      return(File || BZFile);
   }
   inline InputFileFormat getFormat() const {
      return(Format);
   }
   inline FILE* getFile() const {
      return(File);
   }
   inline const std::string& getName() const {
      return(Name);
   }
   inline unsigned long long getLine() const {
      return(Line);
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
