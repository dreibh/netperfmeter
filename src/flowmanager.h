/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2024 by Thomas Dreibholz
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

#ifndef FLOWMANAGER_H
#define FLOWMANAGER_H

#include "thread.h"
#include "messagereader.h"
#include "outputfile.h"
#include "flowbandwidthstats.h"
#include "flowtrafficspec.h"
#include "defragmenter.h"
#include "measurement.h"
#include "cpustatus.h"
#include "tools.h"

#include <poll.h>

#include <vector>
#include <map>


class Flow;

class FlowManager : public Thread
{
   friend class Flow;

   // ====== Methods ========================================================
   protected:
   FlowManager();
   virtual ~FlowManager();

   public:
   inline static FlowManager* getFlowManager() {
      return &FlowManagerSingleton;
   }
   inline MessageReader* getMessageReader() {
      return &Reader;
   }
   inline std::vector<Flow*>& getFlowSet() {   // Internal usage only!
      return FlowSet;
   }

   inline void configureDisplay(const bool on) {
      lock();
      DisplayEnabled = on;
      unlock();
   }

   void addUnidentifiedSocket(const int protocol, const int socketDescriptor);
   void removeUnidentifiedSocket(const int  socketDescriptor,
                                 const bool closeSocket = true);
   Flow* identifySocket(const uint64_t         measurementID,
                        const uint32_t         flowID,
                        const uint16_t         streamID,
                        const int              socketDescriptor,
                        const sockaddr_union*  from,
                        int&                   controlSocket);

   void addFlow(Flow* flow);
   void removeFlow(Flow* flow);
   void printFlows(std::ostream& os,
                   const bool    printStatistics);

   bool startMeasurement(const int                controlSocket,
                         const uint64_t           measurementID,
                         const unsigned long long now,
                         const char*              vectorNamePattern,
                         const OutputFileFormat   vectorFileFormat,
                         const char*              scalarNamePattern,
                         const OutputFileFormat   scalarFileFormat);
   void stopMeasurement(const int                 controlSocket,
                        const uint64_t            measurementID,
                        const unsigned long long  now = getMicroTime());

   void writeScalarStatistics(const uint64_t           measurementID,
                              const unsigned long long now,
                              OutputFile&              scalarFile,
                              const unsigned long long firstStatisticsEvent);
   void writeVectorStatistics(const uint64_t           measurementID,
                              const unsigned long long now,
                              OutputFile&              vectorFile,
                              FlowBandwidthStats&      globalStats,
                              FlowBandwidthStats&      relGlobalStats,
                              const unsigned long long firstStatisticsEvent,
                              const unsigned long long lastStatisticsEvent);

   Flow* findFlow(const uint64_t measurementID,
                  const uint32_t flowID,
                  const uint16_t streamID);
   Flow* findFlow(const int socketDescriptor,
                  uint16_t  streamID);
   Flow* findFlow(const struct sockaddr* from);


   bool addMeasurement(const int controlSocket, Measurement* measurement);
   Measurement* findMeasurement(const int controlSocket, const uint64_t measurementID);
   void removeMeasurement(const int controlSocket, Measurement* measurement);
   void removeAllMeasurements(const int controlSocket);
   void printMeasurements(std::ostream& os);


   // ====== Protected Methods ==============================================
   protected:
   void run();


   // ====== Private Methods ================================================
   unsigned long long getNextEvent();
   void handleEvents(const unsigned long long now);


   // ====== Private Data ===================================================
   private:
   static FlowManager                 FlowManagerSingleton;

   // ------ Flow Management ------------------------------------------------
   MessageReader                      Reader;
   std::vector<Flow*>                 FlowSet;
   FlowBandwidthStats                 CurrentGlobalStats;
   FlowBandwidthStats                 LastGlobalStats;
   bool                               DisplayEnabled;

   // ------ Measurement Management -----------------------------------------
   std::map<std::pair<int, uint64_t>,
            Measurement*>             MeasurementSet;
   unsigned long long                 DisplayInterval;
   unsigned long long                 FirstDisplayEvent;
   unsigned long long                 LastDisplayEvent;
   unsigned long long                 NextDisplayEvent;
   CPUStatus                          CPULoadStats;
   FlowBandwidthStats                 GlobalStats;      // For displaying only
   FlowBandwidthStats                 RelGlobalStats;   // For displaying only
   CPUStatus                          CPUDisplayStats;  // For displaying only

   struct UnidentifiedSocket {
      int     SocketDescriptor;
      int     Protocol;
      pollfd* PollFDEntry;
      bool    ToBeRemoved;
      bool    ToBeClosed;
   };
   std::map<int, UnidentifiedSocket*> UnidentifiedSockets;
};


int createAndBindSocket(const int             family,
                        const int             type,
                        const int             protocol,
                        const uint16_t        localPort,
                        const unsigned int    localAddresses,
                        const sockaddr_union* localAddressArray,
                        const bool            listenMode,
                        const bool            bindV6Only);
bool setBufferSizes(int sd, const int sndBufSize, const int rcvBufSize);

#endif
