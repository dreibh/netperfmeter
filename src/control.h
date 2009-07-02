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

#ifndef CONTROL_H
#define CONTROL_H

#include "flowspec.h"
#include "netperfmeterpackets.h"
#include "messagereader.h"
#include "tools.h"

#include <ext_socket.h>
#include <vector>



bool performNetPerfMeterAddFlow(int controlSocket, const Flow* flow);
bool performNetPerfMeterIdentifyFlow(int controlSocket, const Flow* flow);
bool performNetPerfMeterStart(int            controlSocket,
                              const uint64_t measurementID);
bool performNetPerfMeterStop(int            controlSocket,
                             const uint64_t measurementID);

bool sendNetPerfMeterAcknowledge(int            controlSocket,
                                 sctp_assoc_t   assocID,   // ????? notwendig ???
                                 const uint64_t measurementID,
                                 const uint32_t flowID,
                                 const uint16_t streamID,
                                 const uint32_t status);
bool awaitNetPerfMeterAcknowledge(int            controlSocket,
                                  const uint64_t measurementID,
                                  const uint32_t flowID,
                                  const uint16_t streamID,
                                  const int      timeout = -1);


bool handleControlMessage(MessageReader*           messageReader,
                          std::vector<FlowSpec*>& flowSet,
                          int                      controlSocket);

// bool addFlowToRemoteNode(int controlSocket, const FlowSpec* flowSpec);
FlowSpec* createRemoteFlow(const NetPerfMeterAddFlowMessage* addFlowMsg,
                           const sctp_assoc_t            controlAssocID,
                           const char*                   description);
bool removeFlowFromRemoteNode(int controlSocket, FlowSpec* flowSpec);
void remoteAllFlowsOwnedBy(std::vector<FlowSpec*>& flowSet, const sctp_assoc_t assocID);
bool sendAcknowledgeToRemoteNode(int            controlSocket,
                                 sctp_assoc_t   assocID,
                                 const uint64_t measurementID,
                                 const uint32_t flowID,
                                 const uint16_t streamID,
                                 const uint32_t status);
void handleIdentifyMessage(std::vector<FlowSpec*>&        flowSet,
                           const NetPerfMeterIdentifyMessage* identifyMsg,
                           const int                      sd,
                           const sctp_assoc_t             assocID,
                           const sockaddr_union*          from,
                           const int                      controlSocket);
/*bool startMeasurement(int            controlSocket,
                      const uint64_t measurementID);*/
// bool stopMeasurement(int            controlSocket,
//                      const uint64_t measurementID);

#endif
