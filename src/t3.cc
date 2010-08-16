#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <set>
#include <vector>
#include <iostream>


class QoSStats
{
   // ====== Public methods =================================================
   public:
   QoSStats();
   ~QoSStats();

   double getTrackingInterval() const {
      return(TrackingInterval);
   }
   void setTrackingInterval(const double trackingInterval) {
      TrackingInterval = trackingInterval;
   }

   void add(const double now, const unsigned int value);
   void dump(const double now);

   // ====== Private data ===================================================
   private:
   struct QoSStatsEntry {
      double       TimeStamp;
      unsigned int Value;
   };
   struct QoSStatsEntryLess
   {
      inline bool operator()(const QoSStatsEntry* a, const QoSStatsEntry* b) const {
         return(a->TimeStamp < b->TimeStamp);
      }
   };
   typedef std::set<QoSStatsEntry*, QoSStatsEntryLess> QoSStatsEntryType;

   void purge(const double now);

   double             TrackingInterval;
   unsigned long long ValueSum;
   QoSStatsEntryType  QoSStatsSet;
};


// ###### Constructor #######################################################
QoSStats::QoSStats()
{
   TrackingInterval = 1.0;
   ValueSum         = 0;
}


// ###### Destructor ########################################################
QoSStats::~QoSStats()
{
   QoSStatsEntryType::iterator first = QoSStatsSet.begin();
   while(first != QoSStatsSet.end()) {
      delete *first;
      QoSStatsSet.erase(first);
      first = QoSStatsSet.begin();
   }
}


// ###### Purge out-of-date values ##########################################
void QoSStats::purge(const double now)
{
   QoSStatsEntryType::iterator first = QoSStatsSet.begin();
   while(first != QoSStatsSet.end()) {

      QoSStatsEntry* statEntry = *first;
      if(statEntry->TimeStamp >= now - TrackingInterval) {
         break;
      }
      assert(ValueSum >=  statEntry->Value);
      ValueSum -= statEntry->Value;
      delete statEntry;
      QoSStatsSet.erase(first);

      first = QoSStatsSet.begin();
   }
}


// ###### Add value #########################################################
void QoSStats::add(const double now, const unsigned int value)
{
   // ====== Add new entry ==================================================
   QoSStatsEntry* statEntry = new QoSStatsEntry;
   statEntry->TimeStamp = now;
   statEntry->Value     = value;
   QoSStatsSet.insert(statEntry);
   ValueSum += statEntry->Value;

   // ====== Get rid of out-of-date entries =================================
   purge(now);
}


void QoSStats::dump(const double now)
{
   const QoSStatsEntryType::iterator first = QoSStatsSet.begin();
   QoSStatsEntryType::iterator       last  = QoSStatsSet.end();
   if(first != last) {
      last--;   // There must be at least one element ...
      if(first != last) {   // There are at least two elements ...
         printf("INT=%1.3f\n", (*last)->TimeStamp - (*first)->TimeStamp);
      }
   }
   else puts("empty");
}


int main(int argc, char** argv)
{
   QoSStats stat;

   stat.dump(4);

   stat.add(1, 1000);
   stat.add(2, 1000);
   stat.add(3, 1000);

   stat.dump(4);
}
