#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>



// ###### Check filename for given suffix ###################################
bool hasSuffix(const std::string& name, const std::string& suffix)
{
   const size_t found = name.rfind(suffix);
   printf("V=%d %d %d\n",found,name.length(),suffix.length());

   if(found == name.length() - suffix.length()) {
      return(true);
   }
   return(false);
}


// ###### Dissect file name into prefix and suffix ##########################
void dissectName(const std::string& name,
                 std::string&       prefix,
                 std::string&       suffix)
{
   const size_t slash = name.find_last_of('/');
   size_t dot         = name.find_last_of('.');
   if((dot != std::string::npos) &&
      ((slash == std::string::npos) || (slash < dot)) ) {
      // There is a suffix which is part of the file name itself
      if(std::string(name, dot) == ".bz2") {
         // There is a .bz2 suffix. Look for the actual suffix.
         const size_t dot2 = name.find_last_of('.', dot - 1);
         if( (dot2 != std::string::npos) &&
             ((slash == std::string::npos) || (slash < dot2)) ) {
            // There is another suffix (*.bz2) *and* the second dot
            // is not part of a directory name
            dot = dot2;
         }
      }
      suffix = std::string(name, dot);
      prefix = std::string(name, 0, dot);
   }
   else {
      prefix = name;
      suffix = "";
   }

//    std::cout << name << " -> " << prefix << " | " << suffix << std::endl;
   std::cout << name << " -> " << prefix << " | " << suffix << "   " << (int)hasSuffix(name, ".bz2") << std::endl;
}


using namespace std;

int main(int argc, char** argv)
{
   string s1;
   string s2;
   
   dissectName("/tmp/test1.data.bz2", s1, s2);
   dissectName("/tmp/test1.data", s1, s2);
   dissectName("/tmp/test1", s1, s2);
   dissectName("/tmp.test1/.data.bz2", s1, s2);
   dissectName("/tmp.xy.xy/test1", s1, s2);
   dissectName("/tmp.zz/test1.bz2", s1, s2);
   dissectName("/tmp.zz/test1.data", s1, s2);
   dissectName("output.vec.bz2", s1, s2);
}
