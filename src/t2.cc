#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>

struct Fragment
{
   uint64_t SeqNumber;
   uint16_t Length;
};

struct Frame
{
   unsigned long long            LastUpdate;
   std::map<uint64_t, Fragment*> FragmentSet;
};


class Defragmenter
{
   public:
   Defragmenter();
   ~Defragmenter();
   
   private:
   std::map<uint64_t, Frame*> FrameSet;
};


Defragmenter::Defragmenter()
{
}


Defragmenter::~Defragmenter()
{
}


using namespace std;

int main(int argc, char** argv)
{
}
