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

#ifndef FLOW_H
#define FLOW_H

#include "cpustatus.h"
#include "defragmenter.h"
#include "flowbandwidthstats.h"
#include "flowmanager.h"
#include "flowtrafficspec.h"
#include "measurement.h"
#include "messagereader.h"
#include "outputfile.h"
#include "thread.h"
#include "tools.h"

// #include <poll.h>

#include <vector>
#include <map>


class Flow : public Thread
{
   public:
   friend class FlowManager;
   enum FlowStatus {
      WaitingForStartup = 1,
      On                = 2,
      Off               = 3
   };

   // ====== Methods ========================================================
   public:
   Flow(const uint64_t         measurementID,
        const uint32_t         flowID,
        const uint16_t         streamID,
        const FlowTrafficSpec& trafficSpec,
        const int              controlSocket = -1);
   virtual ~Flow();


   inline uint64_t getMeasurementID() const {
      return MeasurementID;
   }
   inline uint32_t getFlowID() const {
      return FlowID;
   }
   inline uint16_t getStreamID() const {
      return StreamID;
   }
   inline int getSocketDescriptor() const {
      return SocketDescriptor;
   }
   void setSocketDescriptor(const int  socketDescriptor,
                            const bool originalSocketDescriptor = true,
                            const bool deleteWhenFinished       = true);

   inline Defragmenter* getDefragmenter() {
      return &MyDefragmenter;
   }
   inline int getControlSocketDescriptor() const {
      return ControlSocketDescriptor;
   }
   inline const FlowTrafficSpec& getTrafficSpec() const {
      return TrafficSpec;
   }
   inline FlowStatus getOutputStatus() const {
      return OutputStatus;
   }
   inline FlowStatus getInputStatus() const {
      return InputStatus;
   }
   inline bool isAcceptedIncomingFlow() const {
      return ControlSocketDescriptor != -1;
   }

   inline void endOfInput() {
      InputStatus = Off;
   }

   inline const FlowBandwidthStats& getCurrentBandwidthStats() const {
      return CurrentBandwidthStats;
   }
   inline FlowBandwidthStats& getCurrentBandwidthStats() {
      return CurrentBandwidthStats;
   }
   inline unsigned long long getFirstTransmission() const {
      return FirstTransmission;
   }
   inline unsigned long long getLastTransmission() const {
      return LastTransmission;
   }
   inline unsigned long long getFirstReception() const {
      return FirstReception;
   }
   inline unsigned long long getLastReception() const {
      return LastReception;
   }

   inline uint32_t nextOutboundFrameID() {
      return ++LastOutboundFrameID;
   }
   inline uint64_t nextOutboundSeqNumber() {
      return ++LastOutboundSeqNumber;
   }

   inline bool isRemoteAddressValid() const {
      return RemoteAddressIsValid;
   }
   inline const sockaddr* getRemoteAddress() const {
      return &RemoteAddress.sa;
   }

   inline OutputFile& getVectorFile() {
      return VectorFile;
   }
   inline const OutputFile& getVectorFile() const {
      return VectorFile;
   }
   inline double getJitter() const {
      return Jitter;
   }
   inline void setJitter(const double jitter) {
      Jitter = jitter;
   }
   inline double getDelay() const {
      return Delay;
   }
   inline void setDelay(const double transitTime) {
      Delay = transitTime;
   }

   inline Measurement* getMeasurement() const {
      return MyMeasurement;
   }
   bool setMeasurement(Measurement* measurement);

   inline static std::string getNodeOutputName(const std::string& pattern,
                                               const char*        type,
                                               const std::string  extension = "") {
      std::string prefix;
      std::string suffix;
      dissectName(pattern, prefix, suffix);
      const std::string result = prefix + "-" + std::string(type) +
                                 std::string(extension) + suffix;
      return result;
   }

   bool initializeVectorFile(const char* name, const OutputFileFormat format);
   void updateTransmissionStatistics(const unsigned long long now,
                                     const size_t             addedFrames,
                                     const size_t             addedPackets,
                                     const size_t             addedBytes);
   void updateReceptionStatistics(const unsigned long long now,
                                  const size_t             addedFrames,
                                  const size_t             addedBytes,
                                  const size_t             lostFrames,
                                  const size_t             lostPackets,
                                  const size_t             lostBytes,
                                  const unsigned long long seqNumber,
                                  const double             delay,
                                  const double             delayDiff,
                                  const double             jitter);


   void print(std::ostream& os, const bool printStatistics = false);
   void resetStatistics();

   bool configureSocket(const int socketDescriptor);

   bool activate();
   void deactivate(const bool asyncStop = false);

   protected:
   virtual void run();


   // ====== Private Methods ================================================
   private:
   unsigned long long scheduleNextTransmissionEvent();
   unsigned long long scheduleNextStatusChangeEvent(const unsigned long long now);
   void handleStatusChangeEvent(const unsigned long long now);


   // ====== Flow Identification ============================================
   uint64_t           MeasurementID;
   uint32_t           FlowID;
   uint16_t           StreamID;

   // ====== Socket Management ==============================================
   int                SocketDescriptor;
   bool               OriginalSocketDescriptor;
   bool               DeleteWhenFinished;
   pollfd*            PollFDEntry;   // For internal usage by FlowManager

   int                ControlSocketDescriptor;
   sockaddr_union     RemoteAddress;
   bool               RemoteAddressIsValid;


   // ====== Timing =========================================================
   unsigned long long TimeBase;     // when has flow been started?
   unsigned long long TimeOffset;   // time offset to TimeBase for next event
   unsigned long long FirstTransmission;
   unsigned long long FirstReception;
   unsigned long long LastTransmission;
   unsigned long long LastReception;

   // ====== Traffic Specification ==========================================
   FlowTrafficSpec    TrafficSpec;
   FlowStatus         InputStatus;
   FlowStatus         OutputStatus;
   uint32_t           LastOutboundFrameID;     // ID of last outbound frame
   uint64_t           LastOutboundSeqNumber;   // ID of last outbound packet
   unsigned long long NextStatusChangeEvent;
   size_t             OnOffEventPointer;

   // ====== Statistics =====================================================
   Measurement*       MyMeasurement;
   OutputFile         VectorFile;
   FlowBandwidthStats CurrentBandwidthStats;
   FlowBandwidthStats LastBandwidthStats;
   double             Delay;    // Transit time of latest received packet
   double             Jitter;   // Current jitter value
   Defragmenter       MyDefragmenter;
};

#endif
