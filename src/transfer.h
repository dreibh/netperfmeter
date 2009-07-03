/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009 by Thomas Dreibholz
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
 * Contact: dreibh@iem.uni-due.de
 */

#ifndef TRANSFER_H
#define TRANSFER_H

#include "flowspec.h"
#include "messagereader.h"
#include "statisticswriter.h"
#include "netperfmeterpackets.h"


ssize_t transmitFrame(// ??? StatisticsWriter*        statsWriter,
                      Flow*                    flow,
                      const unsigned long long now,
                      const size_t             maxMsgSize);
ssize_t handleDataMessage(const bool               isActiveMode,
//                           MessageReader*           messageReader,
//                           StatisticsWriter*        statsWriter,
//                           std::vector<FlowSpec*>&  flowSet,
                          const unsigned long long now,
                          const int                protocol,
                          const int                sd);

#endif
