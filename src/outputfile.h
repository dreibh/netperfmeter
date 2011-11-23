/* $Id$
 *
 * Plain/BZip2 File Output
 * Copyright (C) 2009-2012 by Thomas Dreibholz
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

#ifndef OUTPUTFILE_H
#define OUTPUTFILE_H

#include <stdio.h>
#include <bzlib.h>
#include <string>
#include <iostream>


// Output File Formats
enum OutputFileFormat
{
   OFF_None  = 0,
   OFF_Plain = 1,
   OFF_BZip2 = 2
};


class OutputFile
{
   // ====== Methods ========================================================
   public:
   OutputFile();
   ~OutputFile();

   bool initialize(const char*            name,
                   const OutputFileFormat format,
                   const unsigned int     compressionLevel = 9);
   bool finish(const bool          closeFile    = true,
               unsigned long long* bytesIn      = NULL,
               unsigned long long* bytesOut     = NULL);
   bool printf(const char* str, ...);
   bool write(const char* buffer, const size_t bufferLength);

   inline bool exists() const {
      return(File || BZFile);
   }
   inline OutputFileFormat getFormat() const {
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
   inline unsigned long long nextLine() {
      return(++Line);
   }

   // ====== Private Data ===================================================
   private:
   OutputFileFormat   Format;
   std::string        Name;
   unsigned long long Line;
   FILE*              File;
   BZFILE*            BZFile;
   bool               WriteError;
};

#endif
