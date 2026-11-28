/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2026 by Thomas Dreibholz
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

#ifndef MESSAGEREADER_H
#define MESSAGEREADER_H

#include "mutex.h"
#include "tools.h"

#include <cstddef>
#include <cstdint>
#include <ext_socket.h>
#include <map>
#include <string>
#include <sys/types.h>
#include <vector>


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

   bool registerSocket(const int             protocol,
                       const int             sd,
                       const bool            mustBeNew      = true,
                       const size_t          maxMessageSize = 65535,
                       const sockaddr_union* remoteAddress  = nullptr);
   bool deregisterSocket(const int sd);
   bool getRemoteAddress(const int sd, sockaddr_union& remoteAddress) const;
   std::string formatSocketEndpoint(const int              sd,
                                    const struct sockaddr* from = nullptr) const;

   ssize_t receiveMessage(const int        sd,
                          void*            buffer,
                          size_t           bufferSize,
                          sockaddr*        from     = nullptr,
                          socklen_t*       fromSize = nullptr,
                          int64_t*         streamID = nullptr,
                          int*             msgFlags = nullptr);
   size_t getAllSDs(int* sds, const size_t maxEntries);
   bool hasReadyMessage(const int sd) const;
   bool hasPartialMessage(const int sd) const;

   inline size_t size() const {
      ScopedLock guard(SocketMapMutex);
      return SocketMap.size();
   }

   inline int getSocketProtocol(const int sd) const {
      ScopedLock guard(SocketMapMutex);
      const std::map<int, Socket*>::const_iterator found = SocketMap.find(sd);
      return (found != SocketMap.end()) ? found->second->Protocol : -1;
   }

   // ====== Private Data ===================================================
   private:
   class ScopedLock {
      public:
      explicit ScopedLock(Mutex& mutex) : MyMutex(mutex) {
         MyMutex.lock();
      }
      ~ScopedLock() {
         MyMutex.unlock();
      }
      private:
      ScopedLock(const ScopedLock&);
      ScopedLock& operator=(const ScopedLock&);
      Mutex& MyMutex;
   };

   struct TLVHeader {
      uint8_t  Type;
      uint8_t  Flags;
      uint16_t Length;
   } __attribute__((packed));

   struct StreamBuffer {
      std::vector<char> Data;
      size_t            ReadOffset;
      bool              StreamError;

      StreamBuffer()
         : ReadOffset(0),
           StreamError(false) {
      }
   };

   struct Socket {
      enum MessageReaderStatus {
         MRS_WaitingForHeader = 0,
         MRS_PartialRead      = 1,
         MRS_StreamError      = 2
      };
      MessageReaderStatus             Status;
      int                             Protocol;
      int                             SocketDescriptor;
      unsigned int                    UseCount;
      char*                           MessageBuffer;
      size_t                          MessageBufferSize;
      size_t                          MessageSize;
      size_t                          BytesRead;
      std::map<int64_t, StreamBuffer> StreamBuffers;
      int64_t                         LastStreamID;
      sockaddr_union                  RemoteAddress;
      bool                            RemoteAddressIsValid;
   };

   static bool isStreamBufferReady(const StreamBuffer& sb,
                                   const size_t        maxMessageSize);
   static ssize_t extractBufferedStreamMessage(Socket*      socket,
                                               void*        buffer,
                                               const size_t bufferSize,
                                               int64_t*     streamID);

   inline Socket* getSocket(const int sd) {
      std::map<int, Socket*>::iterator found = SocketMap.find(sd);
      if(found != SocketMap.end()) {
         return found->second;
      }
      return nullptr;
   }

   mutable Mutex          SocketMapMutex;
   std::map<int, Socket*> SocketMap;
};

#endif
