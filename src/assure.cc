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

#include "assure.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>


// ###### Print error message and abort (C++20 source_location) #############
[[noreturn]] void __assure_fail(std::string_view           expression,
                                const std::source_location location)
{
   std::cerr << "assure(" << expression << ") failed in "
             << location.function_name() << " ("
             << location.file_name() << ":" << location.line() << ")!\n";
   std::abort();
}


// ###### Print error message and abort (Classic signature) #################
[[noreturn]] void __assure_fail(const char*  expression,
                                const char*  file,
                                unsigned int line,
                                const char*  function)
{
   std::cerr << "assure(" << expression << ") failed in "
             << function << " (" << file << ":" << line << ")!\n";
   std::abort();
}


// ###### Print error message with errno and abort (source_location) ########
[[noreturn]] void __assure_fail_perror(std::string_view           expression,
                                       int                        errorCode,
                                       const std::source_location location)
{
   std::cerr << "assure(" << expression << ") failed in "
             << location.function_name() << " ("
             << location.file_name() << ":" << location.line() << "): "
             << std::strerror(errorCode) << '\n';
   std::abort();
}


[[noreturn]] void __assure_fail_perror(std::string_view           expression,
                                       const std::source_location location)
{
   __assure_fail_perror(expression, errno, location);
}


// ###### Print error message with errno and abort (Classic signature) ######
[[noreturn]] void __assure_fail_perror(const char*  expression,
                                       const char*  file,
                                       unsigned int line,
                                       const char*  function)
{
   std::cerr << "assure(" << expression << ") failed in "
             << function << " (" << file << ":" << line << "): "
             << std::strerror(errno) << '\n';
   std::abort();
}
