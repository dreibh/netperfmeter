#ifndef T3_H
#define T3_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <bzlib.h>
#include <ext_socket.h>

#include <poll.h>
#include <assert.h>
#include <stdarg.h>

#include "messagereader.h"
#include "thread.h"
#include "tools.h"


class OutputFile
{
   // ====== Methods ========================================================
   public:
   OutputFile();
   ~OutputFile();
   
   bool initialize(const char* name, const bool compressFile);
   bool finish(const bool closeFile = true);
   bool printf(const char* str, ...);
   
   inline FILE* getFile() const {
      return(File);
   }
   inline const std::string& getName() const {
      return(Name);
   }
   inline size_t nextLine() {
      return(++Line);
   }

   // ====== Private Data ===================================================
   private:
   std::string Name;
   size_t      Line;
   FILE*       File;
   BZFILE*     BZFile;
};




class TrafficSpec
{
   // ====== Methods ========================================================
   public:
   TrafficSpec();
   ~TrafficSpec();

   void print(std::ostream& os) const;
   void reset();
   

   // ====== Public Data ====================================================
   public:
   static void showEntry(std::ostream& os, const double value, const uint8_t rng);

   std::string            Description;
   uint8_t                Protocol;
   std::set<unsigned int> OnOffEvents;

   double   ReliableMode;
   double   OrderedMode;

   double   OutboundFrameRate;
   double   OutboundFrameSize;
   double   InboundFrameRate;
   double   InboundFrameSize;
   uint8_t  OutboundFrameRateRng;
   uint8_t  OutboundFrameSizeRng;
   uint8_t  InboundFrameRateRng;
   uint8_t  InboundFrameSizeRng;
};



class FlowBandwidthStats
{
   // ====== Methods ========================================================
   public:
   FlowBandwidthStats();
   ~FlowBandwidthStats();

   void print(std::ostream& os,
              const double  transmissionDuration,
              const double  receptionDuration) const;
   void reset();
   

   // ====== Public Data ====================================================
   public:
   unsigned long long     TransmittedBytes;
   unsigned long long     TransmittedPackets;
   unsigned long long     TransmittedFrames;

   unsigned long long     ReceivedBytes;
   unsigned long long     ReceivedPackets;
   unsigned long long     ReceivedFrames;
   
   unsigned long long     LostBytes;
   unsigned long long     LostPackets;
   unsigned long long     LostFrames;
};





class Measurement;

class MeasurementManager : public Mutex
{
   // ====== Protected Methods ==============================================
   protected:
   
   // ====== Public Methods =================================================
   public:
   inline static MeasurementManager* getMeasurementManager() {
      return(&MeasurementManagerSingleton);
   }

   bool addMeasurement(Measurement* measurement);
   void printMeasurements(std::ostream& os);
   void removeMeasurement(Measurement* measurement);

   unsigned long long getNextEvent();
   void handleEvents(const unsigned long long now);

   // ====== Private Data ===================================================
   private:
   Measurement* findMeasurement(const uint64_t measurementID);

   static MeasurementManager        MeasurementManagerSingleton;
   std::map<uint64_t, Measurement*> MeasurementSet;
};


class Measurement : public Mutex
{
   friend class MeasurementManager;

   // ====== Public Methods =================================================
   public:
   Measurement();
   ~Measurement();

   bool initialize(const unsigned long long now,
                   const uint64_t           measurementID,
                   const char*              vectorNamePattern,
                   const char*              scalarNamePattern);
   void finish();
   void writeVectorStatistics(const unsigned long long now);

   // ====== Private Data ===================================================
   private:
   uint64_t           MeasurementID;
   unsigned long long StatisticsInterval;
   unsigned long long FirstStatisticsEvent;
   unsigned long long LastStatisticsEvent;
   unsigned long long NextStatisticsEvent;

   std::string        VectorNamePattern;
   std::string        ScalarNamePattern;
   OutputFile         VectorFile;
   OutputFile         ScalarFile;
//    OutputFile         ConfigFile;

   FlowBandwidthStats CurrentBandwidthStats;
   FlowBandwidthStats LastBandwidthStats;
/*
   unsigned long long VectorLine;
   std::string        VectorName;
   std::string        VectorPrefix;
   std::string        VectorSuffix;
   FILE*              VectorFile;
   BZFILE*            VectorBZFile;

   std::string        ScalarName;
   std::string        ScalarPrefix;
   std::string        ScalarSuffix;
   FILE*              ScalarFile;
   BZFILE*            ScalarBZFile;

   unsigned long long TotalTransmittedBytes;
   unsigned long long TotalTransmittedPackets;
   unsigned long long TotalTransmittedFrames;
   unsigned long long TotalReceivedBytes;
   unsigned long long TotalReceivedPackets;
   unsigned long long TotalReceivedFrames;
   unsigned long long TotalLostBytes;
   unsigned long long TotalLostPackets;
   unsigned long long TotalLostFrames;
   unsigned long long LastTotalTransmittedBytes;
   unsigned long long LastTotalTransmittedPackets;
   unsigned long long LastTotalTransmittedFrames;
   unsigned long long LastTotalReceivedBytes;
   unsigned long long LastTotalReceivedPackets;
   unsigned long long LastTotalReceivedFrames;
   unsigned long long LastTotalLostBytes;
   unsigned long long LastTotalLostPackets;
   unsigned long long LastTotalLostFrames;
*/
};





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
      return(&FlowManagerSingleton);
   }
   inline MessageReader* getMessageReader() {
      return(&Reader);
   }

   void addSocket(const int protocol, const int socketDescriptor);
   bool identifySocket(const uint64_t        measurementID,
                       const uint32_t        flowID,
                       const uint16_t        streamID,
                       const int             socketDescriptor,
                       const sockaddr_union* from,
                       const bool            compressVectorFile,
                       int&                  controlSocketDescriptor,
                       sctp_assoc_t&         controlAssocID);
   void removeSocket(const int  socketDescriptor,
                     const bool closeSocket = true);

   void addFlow(Flow* flow);
   void removeFlow(Flow* flow);

   void startMeasurement(const uint64_t           measurementID,
                         const bool               printFlows = false,
                         const unsigned long long now        = getMicroTime());
   void stopMeasurement(const uint64_t            measurementID,
                        const bool                printFlows = false,
                        const unsigned long long  now        = getMicroTime());
                         

   void print(std::ostream& os,
              const bool    printStatistics = false);

   Flow* findFlow(const uint64_t measurementID,
                  const uint32_t flowID,
                  const uint16_t streamID);
   Flow* findFlow(const int socketDescriptor,
                  uint16_t  streamID);
   Flow* findFlow(const struct sockaddr* from);
      
      
   // ====== Protected Methods ==============================================
   protected:
   void run();


   // ====== Private Data ===================================================
   private:
   static FlowManager FlowManagerSingleton;

   MessageReader      Reader;
   std::vector<Flow*> FlowSet;
   std::map<int, int> UnidentifiedSockets;
   bool               UpdatedUnidentifiedSockets;
};


class Flow : public Thread
{
   friend class FlowManager;   
   enum FlowStatus {
      WaitingForStartup = 1,
      On                = 2,
      Off               = 3
   };

   // ====== Methods ========================================================
   public:
   Flow(const uint64_t     measurementID,
        const uint32_t     flowID,
        const uint16_t     streamID,
        const TrafficSpec& trafficSpec,
        const int          controlSocketDescriptor = -1,
        const sctp_assoc_t controlAssocID          = 0);
   virtual ~Flow();
   

   inline uint64_t getMeasurementID() const {
      return(MeasurementID);
   }
   inline uint32_t getFlowID() const {
      return(FlowID);
   }
   inline uint16_t getStreamID() const {
      return(StreamID);
   }   
   inline int getSocketDescriptor() const {
      return(SocketDescriptor);
   }
   inline const TrafficSpec& getTrafficSpec() const {
      return(Traffic);
   }
   inline FlowStatus getOutputStatus() const {
      return(OutputStatus);
   }
   inline FlowStatus getInputStatus() const {
      return(InputStatus);
   }

   inline void endOfInput() {
      InputStatus = Off;
   }

   inline const FlowBandwidthStats& getCurrentBandwidthStats() const {
      return(CurrentBandwidthStats);
   }
   inline FlowBandwidthStats& getCurrentBandwidthStats() {
      return(CurrentBandwidthStats);
   }

   inline uint32_t nextOutboundFrameID() {
      return(++LastOutboundFrameID);
   }
   inline uint64_t nextOutboundSeqNumber() {
      return(++LastOutboundSeqNumber);
   }

   inline bool isRemoteAddressValid() const {
      return(RemoteAddressIsValid);
   }
   inline const sockaddr* getRemoteAddress() const {
      return(&RemoteAddress.sa);
   }

   inline OutputFile& getVectorFile() {
      return(VectorFile);
   }
   inline double getJitter() const {
      return(Jitter);
   }
   inline void setJitter(const double jitter) {
      Jitter = jitter;
   }
   inline double getDelay() const {
      return(Delay);
   }
   inline void setDelay(const double transitTime) {
      Delay = transitTime;
   }

   void updateTransmissionStatistics(const unsigned long long now,
                                     const size_t             addedFrames,
                                     const size_t             addedPackets,
                                     const size_t             addedBytes);
   void updateReceptionStatistics(const unsigned long long now,
                                  const size_t             addedFrames,
                                  const size_t             addedBytes,
                                  const double             delay,
                                  const double             jitter);

   
   void print(std::ostream& os, const bool printStatistics = false) const;
   void resetStatistics();

   void setSocketDescriptor(const int  socketDescriptor,
                            const bool originalSocketDescriptor = true);
   bool activate();
   bool deactivate();

   protected:
   virtual void run();
   

   // ====== Private Methods ================================================
   private:
   unsigned long long scheduleNextStatusChangeEvent(const unsigned long long now);
   unsigned long long scheduleNextTransmissionEvent() const;

      
   // ====== Flow Identification ============================================
   uint64_t           MeasurementID;
   uint32_t           FlowID;
   uint16_t           StreamID;

   // ====== Socket Management ==============================================
   int                SocketDescriptor;
   bool               OriginalSocketDescriptor;
   pollfd*            PollFDEntry;   // For internal usage by FlowManager

   int                RemoteControlSocketDescriptor;
   sctp_assoc_t       RemoteControlAssocID;
   sockaddr_union     RemoteAddress;
   bool               RemoteAddressIsValid;


   // ====== Timing =========================================================
   unsigned long long BaseTime;
   unsigned long long FirstTransmission;
   unsigned long long FirstReception;
   unsigned long long LastTransmission;
   unsigned long long LastReception;

   // ====== Traffic Specification ==========================================
   TrafficSpec        Traffic;
   FlowStatus         InputStatus;
   FlowStatus         OutputStatus;
   uint32_t           LastOutboundFrameID;     // ID of last outbound frame
   uint64_t           LastOutboundSeqNumber;   // ID of last outbound packet
   
   // ====== Statistics =====================================================
   OutputFile         VectorFile;
   FlowBandwidthStats CurrentBandwidthStats;
   FlowBandwidthStats LastBandwidthStats;
   double             Delay;    // Transit time of latest received packet
   double             Jitter;   // Current jitter value
};

#endif
