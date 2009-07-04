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
   inline unsigned long long getLine() const {
      return(Line);
   }
   inline unsigned long long nextLine() {
      return(++Line);
   }

   // ====== Private Data ===================================================
   private:
   std::string        Name;
   unsigned long long Line;
   FILE*              File;
   BZFILE*            BZFile;
   bool               WriteError;
};




class TrafficSpec   // ???????????? NAME -> FlowTrafficSpec
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

   double                 ReliableMode;
   double                 OrderedMode;

   double                 OutboundFrameRate;
   double                 OutboundFrameSize;
   double                 InboundFrameRate;
   double                 InboundFrameSize;
   uint8_t                OutboundFrameRateRng;
   uint8_t                OutboundFrameSizeRng;
   uint8_t                InboundFrameRateRng;
   uint8_t                InboundFrameSizeRng;
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
   unsigned long long TransmittedBytes;
   unsigned long long TransmittedPackets;
   unsigned long long TransmittedFrames;

   unsigned long long ReceivedBytes;
   unsigned long long ReceivedPackets;
   unsigned long long ReceivedFrames;
   
   unsigned long long LostBytes;
   unsigned long long LostPackets;
   unsigned long long LostFrames;
};


FlowBandwidthStats operator+(const FlowBandwidthStats& s1, const FlowBandwidthStats& s2);
FlowBandwidthStats operator-(const FlowBandwidthStats& s1, const FlowBandwidthStats& s2);




class Measurement : public Mutex
{
   friend class MeasurementManager;

   // ====== Public Methods =================================================
   public:
   Measurement();
   ~Measurement();

   const uint64_t getMeasurementID() const {
      return(MeasurementID);
   }
   const std::string& getVectorNamePattern() const {
      return(VectorNamePattern);
   }
   const OutputFile& getVectorFile() const {
      return(VectorFile);
   }
   const std::string& getScalarNamePattern() const {
      return(ScalarNamePattern);
   }
   const OutputFile& getScalarFile() const {
      return(ScalarFile);
   }
   inline unsigned long long getFirstStatisticsEvent() const {
      return(FirstStatisticsEvent);
   }
   
   bool initialize(const unsigned long long now,
                   const uint64_t           measurementID,
                   const char*              vectorName,
                   const bool               compressVectorFile,
                   const char*              scalarName,
                   const bool               compressScalarFile);
   void finish();
   
   void writeScalarStatistics(const unsigned long long now);
   void writeVectorStatistics(const unsigned long long now,
                              size_t&                  globalFlows,
                              FlowBandwidthStats&      globalStats,
                              FlowBandwidthStats&      relGlobalStats);


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

   unsigned long long LastTransmission;
   unsigned long long FirstTransmission;
   unsigned long long LastReception;
   unsigned long long FirstReception;
};


class MeasurementManager : public Mutex
{
   // ====== Protected Methods ==============================================
   protected:
   MeasurementManager();
   ~MeasurementManager();
   
   // ====== Public Methods =================================================
   public:
   inline static MeasurementManager* getMeasurementManager() {
      return(&MeasurementManagerSingleton);
   }

   bool addMeasurement(Measurement* measurement);
   Measurement* findMeasurement(const uint64_t measurementID);
   void removeMeasurement(Measurement* measurement);

   void printMeasurements(std::ostream& os);

   unsigned long long getNextEvent();
   void handleEvents(const unsigned long long now);

   // ====== Private Data ===================================================
   private:
   static MeasurementManager        MeasurementManagerSingleton;
   std::map<uint64_t, Measurement*> MeasurementSet;
   unsigned long long               DisplayInterval;
   unsigned long long               FirstDisplayEvent;
   unsigned long long               LastDisplayEvent;
   unsigned long long               NextDisplayEvent;
   size_t                           GlobalFlows;      // For displaying only
   FlowBandwidthStats               GlobalStats;      // For displaying only
   FlowBandwidthStats               RelGlobalStats;   // For displaying only
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
   inline std::vector<Flow*>& getFlowSet() {   // Internal usage only!
      return(FlowSet);
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
   void print(std::ostream& os,
              const bool    printStatistics);

   bool startMeasurement(const uint64_t           measurementID,
                         const unsigned long long now,
                         const char*              vectorNamePattern,
                         const bool               compressVectorFile,
                         const char*              scalarNamePattern,
                         const bool               compressScalarFile,
                         const bool               printFlows = false);
   void stopMeasurement(const uint64_t            measurementID,
                        const bool                printFlows = false,
                        const unsigned long long  now        = getMicroTime());
                         
   void writeScalarStatistics(const uint64_t           measurementID,
                              const unsigned long long now,
                              OutputFile&              scalarFile,
                              const unsigned long long firstStatisticsEvent);
   void writeVectorStatistics(const uint64_t           measurementID,
                              const unsigned long long now,
                              OutputFile&              vectorFile,
                              size_t&                  globalFlows,
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
   FlowBandwidthStats CurrentGlobalStats;
   FlowBandwidthStats LastGlobalStats;
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
   inline sctp_assoc_t getRemoteControlAssocID() const {
      return(RemoteControlAssocID);
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
//  ????  inline unsigned long long getFirstTransmission() const {
//       return(FirstTransmission);
//    }
//    inline unsigned long long getLastTransmission() const {
//       return(LastTransmission);
//    }
//    inline unsigned long long getFirstReception() const {
//       return(FirstReception);
//    }
//    inline unsigned long long getLastReception() const {
//       return(LastReception);
//    }

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
   inline const OutputFile& getVectorFile() const {
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

   inline static std::string getNodeOutputName(const std::string& pattern,
                                               const char*        type,
                                               const std::string  extension = "") {
      std::string prefix;
      std::string suffix;
      dissectName(pattern, prefix, suffix);
      const std::string result = prefix + "-" + std::string(type) +
                                 std::string(extension) + suffix;
      return(result);
   }

   bool initializeVectorFile(const char* name, const bool compressed);
   void updateTransmissionStatistics(const unsigned long long now,
                                     const size_t             addedFrames,
                                     const size_t             addedPackets,
                                     const size_t             addedBytes);
   void updateReceptionStatistics(const unsigned long long now,
                                  const size_t             addedFrames,
                                  const size_t             addedBytes,
                                  const unsigned long long seqNumber,
                                  const double             delay,
                                  const double             delayDiff,
                                  const double             jitter);

   
   void print(std::ostream& os, const bool printStatistics = false) const;
   void resetStatistics();

   void setSocketDescriptor(const int  socketDescriptor,
                            const bool originalSocketDescriptor = true);
   bool activate();
   void deactivate();

   protected:
   virtual void run();
   

   // ====== Private Methods ================================================
   private:
   unsigned long long scheduleNextTransmissionEvent() const;
   unsigned long long scheduleNextStatusChangeEvent(const unsigned long long now);
   void handleStatusChangeEvent(const unsigned long long now);

      
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
   unsigned long long NextStatusChangeEvent;
   
   // ====== Statistics =====================================================
   OutputFile         VectorFile;
   FlowBandwidthStats CurrentBandwidthStats;
   FlowBandwidthStats LastBandwidthStats;
   double             Delay;    // Transit time of latest received packet
   double             Jitter;   // Current jitter value
};

#endif
