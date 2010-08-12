#include <stdio.h>
#include <stdlib.h>
#include <set>
#include <vector>
#include <iostream>


struct StatEntry
{
   double       TimeStamp;
   unsigned int Value;
};


class Stat
{
   public:
      Stat();

   double getTrackingInterval() const {
      return(TrackingInterval);
   }
   void setTrackingInterval(const double trackingInterval) {
      TrackingInterval = trackingInterval;
   }

   void add(const double now, const unsigned int value);
   void dump(const double now);


   private:
   struct Less
   {
      inline bool operator()(const StatEntry* a, const StatEntry* b) const {
         return(a->TimeStamp < b->TimeStamp);
      }
   };
   typedef std::set<StatEntry*, Less> StatEntryType;

   void purge(const double now);

   double             TrackingInterval;
   unsigned long long ValueSum;
   StatEntryType      StatSet;
};


Stat::Stat()
{
   TrackingInterval = 1.0;
   ValueSum         = 0;
}

void Stat::purge(const double now)
{
   StatEntryType::iterator first = StatSet.begin();
   while(first != StatSet.end()) {
      if((*first)->TimeStamp >= now - TrackingInterval) {
         break;
      }
      puts("---del---");
      delete *first;
      StatSet.erase(first);
      first = StatSet.begin();
   }
}

void Stat::add(const double now, const unsigned int value)
{
   StatEntry* statEntry = new StatEntry;
   statEntry->TimeStamp = now;
   statEntry->Value     = value;
   StatSet.insert(statEntry);
   ValueSum += statEntry->Value;

   purge(now);
}

void Stat::dump(const double now)
{
   const StatEntryType::iterator first = StatSet.begin();
   StatEntryType::iterator       last  = StatSet.end();
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
   Stat stat;

   stat.dump(4);

   stat.add(1, 1000);
   stat.add(2, 1000);
   stat.add(3, 1000);

   stat.dump(4);
}
