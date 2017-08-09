/*
 * ==========================================================================
 *                  NetPerfMeter -- Network Performance Meter                 
 *                 Copyright (C) 2009-2017 by Thomas Dreibholz
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
 * Contact:  dreibh@iem.uni-due.de
 * Homepage: https://www.uni-due.de/~be0001/netperfmeter/
 */

#ifndef MESSAGEREADER_H
#define MESSAGEREADER_H

#include <sys/types.h>
#include <cstddef>
#include <map>

#ifndef WITH_NEAT
#include "ext_socket.h"
#else
#include <neat-socketapi.h>
#include <netinet/sctp.h>
#endif


#define MRRM_SOCKET_ERROR (ssize_t)-1
#define MRRM_STREAM_ERROR (ssize_t)-2
#define MRRM_PARTIAL_READ (ssize_t)-3
#define MRRM_BAD_SOCKET   (ssize_t)-4

class MessageReader
{
   // ====== Public Methods =================================================
   public:
   MessageReader();
   ~MessageReader();

   bool registerSocket(const int    protocol,
                       const int    sd,
                       const size_t maxMessageSize = 65535);
   bool deregisterSocket(const int sd);

   ssize_t receiveMessage(const int        sd,
                          void*            buffer,
                          size_t           bufferSize,
                          sockaddr*        from     = NULL,
                          socklen_t*       fromSize = NULL,
                          sctp_sndrcvinfo* sinfo    = NULL,
                          int*             msgFlags = NULL);
   size_t getAllSDs(int* sds, const size_t maxEntries);
   
   inline size_t size() {
      return(SocketMap.size()); 
   }

   // ====== Private Data ===================================================
   private:
   struct TLVHeader {
      uint8_t  Type;
      uint8_t  Flags;
      uint16_t Length;
   } __attribute__((packed));

   struct Socket {
      enum MessageReaderStatus {
         MRS_WaitingForHeader = 0,
         MRS_PartialRead      = 1,
         MRS_StreamError      = 2
      };
      MessageReaderStatus Status;
      int                 Protocol;
      int                 SocketDescriptor;
      size_t              UseCount;
      char*               MessageBuffer;
      size_t              MessageBufferSize;
      size_t              MessageSize;
      size_t              BytesRead;
   };

   inline Socket* getSocket(const int sd) {
      std::map<int, Socket*>::iterator found = SocketMap.find(sd);
      if(found != SocketMap.end()) {
         return(found->second);
      }
      return(NULL);
   }

   std::map<int, Socket*> SocketMap;
};

#endif
