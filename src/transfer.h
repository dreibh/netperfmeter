/*
 * ==========================================================================
 *                  NetPerfMeter -- Network Performance Meter                 
 *                 Copyright (C) 2009-2021 by Thomas Dreibholz
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
 * Contact:  dreibh@iem.uni-due.de
 * Homepage: https://www.uni-due.de/~be0001/netperfmeter/
 */

#ifndef TRANSFER_H
#define TRANSFER_H

#include "flow.h"

#include <sys/types.h>


ssize_t transmitFrame(Flow*                    flow,
                      const unsigned long long now);

ssize_t handleNetPerfMeterData(const bool               isActiveMode,
                               const unsigned long long now,
                               const int                protocol,
                               const int                sd);

#endif
