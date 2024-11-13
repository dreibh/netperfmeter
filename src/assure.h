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

#ifndef ASSURE_H
#define ASSURE_H

// Assertion like assert(), but which will be checked in all build types,
// i.e. not only for Debug builds but also in Release builds
#define assure(expression) \
   if(__builtin_expect(!(expression), 0)) { \
      __assure_fail(#expression, __FILE__, __LINE__, __func__); \
   }

#define assure_perror(expression) \
   if(__builtin_expect(!(expression), 0)) { \
      __assure_fail_perror(#expression, __FILE__, __LINE__, __func__); \
   }

void __assure_fail(const char*  expression,
                   const char*  file,
                   unsigned int line,
                   const char*  function) __attribute__ ((__noreturn__));
void __assure_fail_perror(const char*  expression,
                          const char*  file,
                          unsigned int line,
                          const char*  function) __attribute__ ((__noreturn__));

#endif
