/* $Id$
 *
 * Network Performance Meter
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: dreibh@iem.uni-due.de
 */

#ifndef CONTROL_H
#define CONTROL_H

#include "flow.h"
#include "messagereader.h"

#include <ext_socket.h>


#define NPFOV_BANDWIDTH_INFO 1
#define NPFOV_STATUS         2
#define NPFOV_MEASUREMENTS   3
#define NPFOV_FLOWS          4
#define NPFOV_CONNECTIONS    5
#define NPFOV_REALLY_VERBOSE 6

extern unsigned int gOutputVerbosity;


// ##########################################################################
// #### Active Side Control                                              ####
// ##########################################################################

bool performNetPerfMeterAddFlow(MessageReader* messageReader,
                                int            controlSocket,
                                const Flow*    flow);
bool performNetPerfMeterIdentifyFlow(MessageReader* messageReader,
                                     int            controlSocket,
                                     const Flow*    flow);
bool performNetPerfMeterStart(MessageReader*         messageReader,
                              int                    controlSocket,
                              const uint64_t         measurementID,
                              const char*            activeNodeName,
                              const char*            passiveNodeName,
                              const char*            configName,
                              const char*            vectorNamePattern,
                              const OutputFileFormat vectorFileFormat,
                              const char*            scalarNamePattern,
                              const OutputFileFormat scalarFileFormat);
bool performNetPerfMeterStop(MessageReader* messageReader,
                             int            controlSocket,
                             const uint64_t measurementID);

bool awaitNetPerfMeterAcknowledge(MessageReader* messageReader,
                                  int            controlSocket,
                                  const uint64_t measurementID,
                                  const uint32_t flowID,
                                  const uint16_t streamID,
                                  const int      timeout = -1);


// ##########################################################################
// #### Passive Side Control                                             ####
// ##########################################################################

void handleNetPerfMeterIdentify(const NetPerfMeterIdentifyMessage* identifyMsg,
                                const int                          sd,
                                const sockaddr_union*              from);

bool handleNetPerfMeterControlMessage(MessageReader* messageReader,
                                      int            controlSocket);

bool sendNetPerfMeterAcknowledge(int            controlSocket,
                                 sctp_assoc_t   assocID,
                                 const uint64_t measurementID,
                                 const uint32_t flowID,
                                 const uint16_t streamID,
                                 const uint32_t status);

#endif
