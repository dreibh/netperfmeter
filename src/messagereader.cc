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

#include "assure.h"
#include "messagereader.h"
#include "loglevel.h"
#include "tools.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>


// #define DEBUG_SOCKETS
// #define DEBUG_MESSAGEREADER



// ###### Constructor #######################################################
MessageReader::MessageReader()
{
}


// ###### Destructor ########################################################
MessageReader::~MessageReader()
{
   ScopedLock guard(SocketMapMutex);
   std::map<int, Socket*>::iterator iterator = SocketMap.begin();
   while(iterator != SocketMap.end()) {
      Socket* socket = iterator->second;
      SocketMap.erase(iterator);
      delete [] socket->MessageBuffer;
      delete socket;
      iterator = SocketMap.begin();
   }
}


// ###### Check whether a multiplexed stream buffer has a ready message #####
bool MessageReader::isStreamBufferReady(const StreamBuffer& sb,
                                        const size_t        maxMessageSize)
{
   if(sb.StreamError) {
      return true;
   }
   const size_t avail = sb.Data.size() - sb.ReadOffset;
   if(avail < sizeof(TLVHeader)) {
      return false;
   }
   const TLVHeader* header  = (const TLVHeader*)(sb.Data.data() + sb.ReadOffset);
   const size_t     msgSize = be16toh(header->Length);
   if((msgSize < sizeof(TLVHeader)) || (msgSize > maxMessageSize)) {
      return true;   // Malformed header -> will report MRRM_STREAM_ERROR immediately
   }
   return avail >= msgSize;
}


// ###### Extract one complete TLV message from per-stream buffers ##########
ssize_t MessageReader::extractBufferedStreamMessage(Socket*      socket,
                                                    void*        buffer,
                                                    const size_t bufferSize,
                                                    int64_t*     streamID)
{
   if(socket->StreamBuffers.empty()) {
      return MRRM_PARTIAL_READ;
   }

   // Round-robin starting strictly after LastStreamID
   std::map<int64_t, StreamBuffer>::iterator startIt =
      socket->StreamBuffers.upper_bound(socket->LastStreamID);
   if(startIt == socket->StreamBuffers.end()) {
      startIt = socket->StreamBuffers.begin();
   }

   std::map<int64_t, StreamBuffer>::iterator it = startIt;
   do {
      const int64_t sid = it->first;
      StreamBuffer& sb  = it->second;

      if(sb.StreamError) {
         return MRRM_STREAM_ERROR;
      }

      const size_t avail = sb.Data.size() - sb.ReadOffset;
      if(avail >= sizeof(TLVHeader)) {
         const TLVHeader* header  = (const TLVHeader*)(sb.Data.data() + sb.ReadOffset);
         const size_t     msgSize = be16toh(header->Length);
         if(msgSize < sizeof(TLVHeader)) {
            const std::string peerStr =
               resolvePeerAddress(socket->SocketDescriptor,
                                  socket->RemoteAddressIsValid ? &socket->RemoteAddress.sa : nullptr);
            LOG_WARNING
            stdlog << format("Message size < TLV size in message on %s socket %d from %s!",
                             getProtocolName(socket->Protocol),
                             socket->SocketDescriptor,
                             peerStr.c_str()) << "\n";
            LOG_END
            sb.StreamError = true;
            return MRRM_STREAM_ERROR;
         }
         if(msgSize > socket->MessageBufferSize) {
            const std::string peerStr =
               resolvePeerAddress(socket->SocketDescriptor,
                                  socket->RemoteAddressIsValid ? &socket->RemoteAddress.sa : nullptr);
            LOG_WARNING
            stdlog << format("Message too large to fit buffer for message on %s socket %d from %s!",
                             getProtocolName(socket->Protocol),
                             socket->SocketDescriptor,
                             peerStr.c_str()) << "\n";
            LOG_END
            sb.StreamError = true;
            return MRRM_STREAM_ERROR;
         }

         if(avail >= msgSize) {
            if(msgSize > bufferSize) {
               const std::string peerStr =
                  resolvePeerAddress(socket->SocketDescriptor,
                                     socket->RemoteAddressIsValid ? &socket->RemoteAddress.sa : nullptr);
               LOG_WARNING
               stdlog << format("Buffer size is too small (buffer %zu, message %zu) for message on %s socket %d from %s!",
                                bufferSize, msgSize,
                                getProtocolName(socket->Protocol),
                                socket->SocketDescriptor,
                                peerStr.c_str()) << "\n";
               LOG_END
               sb.StreamError = true;
               return MRRM_STREAM_ERROR;
            }

            memcpy(buffer, sb.Data.data() + sb.ReadOffset, msgSize);
            sb.ReadOffset += msgSize;
            if(sb.ReadOffset >= sb.Data.size()) {
               socket->StreamBuffers.erase(it);
            }
            else if(sb.ReadOffset >= 4096) {
               sb.Data.erase(sb.Data.begin(),
                             sb.Data.begin() + (std::ptrdiff_t)sb.ReadOffset);
               sb.ReadOffset = 0;
            }

            socket->LastStreamID = sid;
            if(streamID != nullptr) {
               *streamID = sid;
            }
            return (ssize_t)msgSize;
         }
      }

      ++it;
      if(it == socket->StreamBuffers.end()) {
         it = socket->StreamBuffers.begin();
      }
   } while(it != startIt);

   return MRRM_PARTIAL_READ;
}


// ###### Check whether a buffered message is ready without a syscall #######
bool MessageReader::hasReadyMessage(const int sd) const
{
   ScopedLock guard(SocketMapMutex);
   const std::map<int, Socket*>::const_iterator found = SocketMap.find(sd);
   if(found == SocketMap.end()) {
      return false;
   }
   const Socket* socket = found->second;
   for(std::map<int64_t, StreamBuffer>::const_iterator it = socket->StreamBuffers.begin();
       it != socket->StreamBuffers.end(); ++it) {
      if(isStreamBufferReady(it->second, socket->MessageBufferSize)) {
         return true;
      }
   }
   return false;
}


// ###### Check whether an incomplete TLV message is buffered on socket #####
bool MessageReader::hasPartialMessage(const int sd) const
{
   ScopedLock guard(SocketMapMutex);
   const std::map<int, Socket*>::const_iterator found = SocketMap.find(sd);
   if(found == SocketMap.end()) {
      return false;
   }
   const Socket* socket = found->second;
   if(socket->BytesRead > 0) {
      return true;
   }
   for(std::map<int64_t, StreamBuffer>::const_iterator it = socket->StreamBuffers.begin();
       it != socket->StreamBuffers.end(); ++it) {
      if(it->second.Data.size() > it->second.ReadOffset) {
         return true;
      }
   }
   return false;
}


// ###### Query cached remote address of a registered socket ################
bool MessageReader::getRemoteAddress(const int sd, sockaddr_union& remoteAddress) const
{
   ScopedLock guard(SocketMapMutex);
   const std::map<int, Socket*>::const_iterator found = SocketMap.find(sd);
   if((found != SocketMap.end()) && found->second->RemoteAddressIsValid) {
      remoteAddress = found->second->RemoteAddress;
      return true;
   }
   memset(&remoteAddress, 0, sizeof(remoteAddress));
   return false;
}


// ###### Format peer endpoint for a registered socket ######################
std::string MessageReader::formatSocketEndpoint(const int              sd,
                                                const struct sockaddr* from) const
{
   ScopedLock guard(SocketMapMutex);
   const struct sockaddr* cachedAddr = nullptr;
   const std::map<int, Socket*>::const_iterator found = SocketMap.find(sd);
   if((found != SocketMap.end()) && found->second->RemoteAddressIsValid) {
      cachedAddr = &found->second->RemoteAddress.sa;
   }
   return resolvePeerAddress(sd, from, cachedAddr);
}


// ###### Register a socket #################################################
bool MessageReader::registerSocket(const int             protocol,
                                   const int             sd,
                                   const bool            mustBeNew,
                                   const size_t          maxMessageSize,
                                   const sockaddr_union* remoteAddress)
{
   ScopedLock                       guard(SocketMapMutex);
   Socket*                          socket;
   std::map<int, Socket*>::iterator found = SocketMap.find(sd);
   if(found == SocketMap.end()) {
      assure(maxMessageSize >= sizeof(TLVHeader));

      socket = new Socket;
      assure(socket != nullptr);
      socket->MessageBuffer = new char[maxMessageSize];
      assure(socket->MessageBuffer != nullptr);
      socket->MessageBufferSize    = maxMessageSize;
      socket->MessageSize          = 0;
      socket->BytesRead            = 0;
      socket->Status               = Socket::MRS_WaitingForHeader;
      socket->Protocol             = protocol;
      socket->SocketDescriptor     = sd;
      socket->UseCount             = 1;
      socket->LastStreamID         = -1;
      socket->RemoteAddressIsValid = false;
      memset(&socket->RemoteAddress, 0, sizeof(socket->RemoteAddress));
      SocketMap.insert(std::pair<int, Socket*>(sd, socket));
   }
   else {
      if(mustBeNew) {
         LOG_FATAL
         stdlog << format("Socket %d should be new, but MessageReader already has a state!",
                          sd) << "\n";
         abort();
         LOG_END_FATAL
      }
      socket = found->second;
      socket->UseCount++;
   }

   if((remoteAddress != nullptr) &&
      ((remoteAddress->sa.sa_family == AF_INET) || (remoteAddress->sa.sa_family == AF_INET6))) {
      socket->RemoteAddress        = *remoteAddress;
      socket->RemoteAddressIsValid = true;
   }
   else if((!socket->RemoteAddressIsValid) && (protocol != IPPROTO_UDP) && (sd >= 0)) {
      sockaddr_union peer;
      socklen_t      peerLen = sizeof(peer);
      memset(&peer, 0, sizeof(peer));
      if((ext_getpeername(sd, &peer.sa, &peerLen) == 0) &&
         (peerLen > 0) &&
         ((peer.sa.sa_family == AF_INET) || (peer.sa.sa_family == AF_INET6))) {
         socket->RemoteAddress        = peer;
         socket->RemoteAddressIsValid = true;
      }
   }
#if defined(DEBUG_SOCKETS)
   printf("RegisterSocket: UseCount[sd=%d,proto=%d]=%u\n",
          socket->SocketDescriptor, socket->Protocol, (unsigned int)socket->UseCount);
#endif
   return true;
}


// ###### Deregister a socket ###############################################
bool MessageReader::deregisterSocket(const int sd)
{
   ScopedLock                       guard(SocketMapMutex);
   std::map<int, Socket*>::iterator found = SocketMap.find(sd);
   if(found != SocketMap.end()) {
      Socket* socket = found->second;
      socket->UseCount--;
#if defined(DEBUG_SOCKETS)
      printf("DeregisterSocket: UseCount[sd=%d,proto=%d]=%u\n",
             socket->SocketDescriptor, socket->Protocol, (unsigned int)socket->UseCount);
#endif
      if(socket->UseCount == 0) {
         SocketMap.erase(found);
         delete [] socket->MessageBuffer;
         delete socket;
         return true;
      }
      return false;   // Socket is still in use!
   }
   return true;
}


// ###### Get all socket descriptors ########################################
size_t MessageReader::getAllSDs(int* sds, const size_t maxEntries)
{
   ScopedLock guard(SocketMapMutex);
   assure(maxEntries >= SocketMap.size());
   size_t count = 0;
   for(std::map<int, Socket*>::iterator iterator = SocketMap.begin(); iterator != SocketMap.end(); iterator++) {
      sds[count++] = iterator->second->SocketDescriptor;
   }
   return count;
}


// ###### Receive full message ##############################################
ssize_t MessageReader::receiveMessage(const int        sd,
                                      void*            buffer,
                                      size_t           bufferSize,
                                      sockaddr*        from,
                                      socklen_t*       fromSize,
                                      int64_t*         streamID,
                                      int*             msgFlags)
{
   ScopedLock guard(SocketMapMutex);
   Socket*    socket = getSocket(sd);
   if(socket != nullptr) {
      int flags = 0;
      if(msgFlags == nullptr) {
         msgFlags = &flags;
      }
      if(streamID != nullptr) {
         *streamID = 0;
      }
      sockaddr_union dummyFrom;
      socklen_t      dummyFromSize = sizeof(dummyFrom);
      if((from == nullptr) || (fromSize == nullptr)) {
         from     = &dummyFrom.sa;
         fromSize = &dummyFromSize;
      }
      if(from != nullptr) {
         memset(from, 0, *fromSize);   // Clear address (Valgrind report)
      }

      // ===== UDP ==========================================================
      if(socket->Protocol == IPPROTO_UDP) {
         // For UDP, reading always returns a full message. There is no
         // partial reading!
         const ssize_t received =
            ext_recvfrom(socket->SocketDescriptor,
                         buffer, bufferSize,
                         *msgFlags, from, fromSize);
         if(received < 0) {
            if((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
               return MRRM_PARTIAL_READ;
            }
            return MRRM_SOCKET_ERROR;
         }
         return received;
      }

      // ====== QUIC (multi-stream byte-stream multiplexed on one sd) =======
#if defined(HAVE_QUIC)
      if(socket->Protocol == IPPROTO_QUIC) {
         // 1. Deliver any complete TLV message already buffered in RAM first:
         const ssize_t bufferedResult =
            extractBufferedStreamMessage(socket, buffer, bufferSize, streamID);
         if(bufferedResult != MRRM_PARTIAL_READ) {
            return bufferedResult;
         }

         // 2. Read next chunk from kernel QUIC socket (forwarding MSG_DONTWAIT):
         uint32_t quicFlags = 0;
#if defined(MSG_DONTWAIT)
         if((*msgFlags) & MSG_DONTWAIT) {
            quicFlags |= MSG_DONTWAIT;
         }
#endif
         int64_t rxStreamID = 0;
         const ssize_t received =
            quic_recvmsg(socket->SocketDescriptor,
                         (char*)socket->MessageBuffer, socket->MessageBufferSize,
                         &rxStreamID, &quicFlags);
         if(received > 0) {
            StreamBuffer& sb = socket->StreamBuffers[rxStreamID];
            if(sb.StreamError) {
               return MRRM_STREAM_ERROR;
            }
            sb.Data.insert(sb.Data.end(),
                           socket->MessageBuffer,
                           socket->MessageBuffer + received);
            return extractBufferedStreamMessage(socket, buffer, bufferSize, streamID);
         }
         else if(received < 0) {
            if((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
               return MRRM_PARTIAL_READ;
            }
            return MRRM_SOCKET_ERROR;
         }
         else {
            return 0;
         }
      }
#endif

      // ====== Find out the number of bytes to read ========================
      ssize_t received;
      size_t  bytesToRead;
#if defined(HAVE_SCTP)
      if(socket->Protocol == IPPROTO_SCTP) {
         // SCTP preserves message boundaries (MSG_EOR) and never merges two
         // messages or notifications into one recvmsg() call. Reading into
         // the full remaining buffer avoids splitting SCTP notifications
         // mid-header (where KAME/NetBSD SCTP may not set MSG_NOTIFICATION
         // until MSG_EOR is reached).
         if(socket->Status == Socket::MRS_StreamError) {
            // An error occurred before. Reset and try again ...
            socket->Status    = Socket::MRS_WaitingForHeader;
            socket->BytesRead = 0;
         }
         bytesToRead = socket->MessageBufferSize - socket->BytesRead;
         assure(bytesToRead + socket->BytesRead <= socket->MessageBufferSize);
      }
      else
#endif
      if( (socket->Protocol == IPPROTO_TCP)
#if defined(HAVE_MPTCP)
          || (socket->Protocol == IPPROTO_MPTCP)
#endif
          || (false)   /* Avoid compile warning without any #ifdef case */
        ) {
         // TCP/MPTCP can return partial messages upon recv() calls.
         // Reading exact header then exact body size avoids consuming bytes
         // belonging to the next message on the stream.
         if(socket->Status == Socket::MRS_WaitingForHeader) {
            assure(sizeof(TLVHeader) >= socket->BytesRead);
            bytesToRead = sizeof(TLVHeader) - socket->BytesRead;
         }
         else if(socket->Status == Socket::MRS_PartialRead) {
            bytesToRead = socket->MessageSize - socket->BytesRead;
         }
         else {
            // Not useful to retry when synchronization has been lost!
            return MRRM_STREAM_ERROR;
         }
         assure(bytesToRead + socket->BytesRead <= socket->MessageBufferSize);
      }
      else {
         // DCCP will always return only a single message on recv() calls.
         bytesToRead = socket->MessageBufferSize;
      }


      // ====== Read from socket ============================================
      if(0) { /* Dummy for following "else if" in #if ... #endif block */ }
#if defined(HAVE_SCTP)
      else if(socket->Protocol == IPPROTO_SCTP) {
         sctp_sndrcvinfo sinfo;
         memset(&sinfo, 0, sizeof(sinfo));
         received = sctp_recvmsg(socket->SocketDescriptor,
                                 (char*)&socket->MessageBuffer[socket->BytesRead], bytesToRead,
                                 from, fromSize, &sinfo, msgFlags);
         if((received > 0) && (streamID != nullptr) && !((*msgFlags) & MSG_NOTIFICATION)) {
            *streamID = (int64_t)sinfo.sinfo_stream;
         }
      }
#endif
      else {
         received = ext_recvfrom(socket->SocketDescriptor,
                                 (char*)&socket->MessageBuffer[socket->BytesRead], bytesToRead,
                                 *msgFlags, from, fromSize);
      }


      // ====== Handle received data ========================================
      if(received > 0) {
         if((!socket->RemoteAddressIsValid) &&
            (socket->Protocol != IPPROTO_UDP) &&
            (from != nullptr) &&
            ((from->sa_family == AF_INET) || (from->sa_family == AF_INET6))) {
            memcpy(&socket->RemoteAddress, from, getSocklen(from));
            socket->RemoteAddressIsValid = true;
         }
         socket->BytesRead += (size_t)received;
         // ====== Handle message header ====================================
         if(socket->Status == Socket::MRS_WaitingForHeader) {
#if defined(HAVE_SCTP)
            // ====== Handle SCTP notification header =======================
            if((socket->Protocol == IPPROTO_SCTP) && (*msgFlags & MSG_NOTIFICATION)) {
#if defined(DEBUG_MESSAGEREADER)
                  printf("Socket %d:   notification L=%u\n",
                         socket->SocketDescriptor,
                         (unsigned int)sizeof(sctp_notification));
#endif
               socket->MessageSize = sizeof(sctp_notification);   // maximum length
               socket->Status      = Socket::MRS_PartialRead;
               // SCTP notification has no TLV header, but must be handled like
               // a message. The actual length of the notification is unknown, we
               // need to look for MSG_EOR!
            }
            // ====== Handle TLV header =====================================
            else {
#endif
               if(socket->BytesRead >= sizeof(TLVHeader)) {
                  const TLVHeader* header = (const TLVHeader*)socket->MessageBuffer;
#if defined(DEBUG_MESSAGEREADER)
                  printf("Socket %d:   header T=%u F=%02x L=%u   [Header]\n",
                          socket->SocketDescriptor,
                          (unsigned int)header->Type, (unsigned int)header->Flags,
                         be16toh(header->Length));
#endif
                  socket->MessageSize = be16toh(header->Length);
                  if(socket->MessageSize < sizeof(TLVHeader)) {
                     const std::string peerStr =
                        resolvePeerAddress(socket->SocketDescriptor, from,
                                           socket->RemoteAddressIsValid ? &socket->RemoteAddress.sa : nullptr);
                     LOG_WARNING
                     stdlog << format("Message size < TLV size in message on %s socket %d from %s!",
                                      getProtocolName(socket->Protocol),
                                      socket->SocketDescriptor,
                                      peerStr.c_str()) << "\n";
                     LOG_END
                     socket->Status = Socket::MRS_StreamError;
                     return MRRM_STREAM_ERROR;
                  }
                  else if(socket->MessageSize > socket->MessageBufferSize) {
                     const std::string peerStr =
                        resolvePeerAddress(socket->SocketDescriptor, from,
                                           socket->RemoteAddressIsValid ? &socket->RemoteAddress.sa : nullptr);
                     LOG_WARNING
                     stdlog << format("Message too large to fit buffer for message on %s socket %d from %s!",
                                      getProtocolName(socket->Protocol),
                                      socket->SocketDescriptor,
                                      peerStr.c_str()) << "\n";
                     LOG_END
                     socket->Status = Socket::MRS_StreamError;
                     return MRRM_STREAM_ERROR;
                  }
                  socket->Status = Socket::MRS_PartialRead;
               }
               else {
                  return MRRM_PARTIAL_READ;
               }
#if defined(HAVE_SCTP)
            }
#endif
            // Continue here with MRS_PartialRead status!
            // (will return MRRM_PARTIAL_READ, or message on header-only message)
         }

         // ====== Handle message payload ===================================
         if(socket->Status == Socket::MRS_PartialRead) {
#if defined(DEBUG_MESSAGEREADER)
            printf("Socket %d:   partial read T=%u F=%02x L=%u   [%u/%u]\n",
                   socket->SocketDescriptor,
                   ((const TLVHeader*)socket->MessageBuffer)->Type,
                   ((const TLVHeader*)socket->MessageBuffer)->Flags,
                   be16toh(((const TLVHeader*)socket->MessageBuffer)->Length),
                   (unsigned int)socket->BytesRead,
                   (unsigned int)socket->MessageSize);
#endif

            // ====== Partially read message ================================
            if(socket->BytesRead < socket->MessageSize) {
#if defined(HAVE_SCTP)
               if(socket->Protocol == IPPROTO_SCTP) {
                  if(*msgFlags & MSG_EOR) {   // end of SCTP message
                     if(!(*msgFlags & MSG_NOTIFICATION)) {   // data message
                        const std::string peerStr =
                           resolvePeerAddress(socket->SocketDescriptor, from,
                                              socket->RemoteAddressIsValid ? &socket->RemoteAddress.sa : nullptr);
                        LOG_WARNING
                        stdlog << format("SCTP message end before TLV message end (read %u, expected %u) for message on %s socket %d from %s!",
                                         (unsigned int)socket->BytesRead,
                                         (unsigned int)socket->MessageSize,
                                         getProtocolName(socket->Protocol),
                                         socket->SocketDescriptor,
                                         peerStr.c_str()) << "\n";
                        LOG_END
                        socket->Status = Socket::MRS_StreamError;
                        return MRRM_STREAM_ERROR;
                     }
                     // This is the end of the SCTP notification. The message
                     // is complete here. Return it to the caller.
                     socket->MessageSize = socket->BytesRead;
                  }
                  else {
                     return MRRM_PARTIAL_READ;
                  }
               }
               else {
#endif
                  return MRRM_PARTIAL_READ;
#if defined(HAVE_SCTP)
               }
#endif
            }

            // ====== Completed reading =====================================
#if defined(HAVE_SCTP)
            if((socket->Protocol == IPPROTO_SCTP) &&
               (!(*msgFlags & MSG_NOTIFICATION)) &&
               (socket->BytesRead > socket->MessageSize)) {
               const std::string peerStr =
                  resolvePeerAddress(socket->SocketDescriptor, from,
                                     socket->RemoteAddressIsValid ? &socket->RemoteAddress.sa : nullptr);
               LOG_WARNING
               stdlog << format("TLV message end does not match with SCTP message end in message on %s socket %d from %s!",
                                getProtocolName(socket->Protocol),
                                socket->SocketDescriptor,
                                peerStr.c_str()) << "\n";
               LOG_END
               socket->Status = Socket::MRS_StreamError;
               return MRRM_STREAM_ERROR;
            }
#endif
            if(socket->MessageSize > bufferSize) {
               const std::string peerStr =
                  resolvePeerAddress(socket->SocketDescriptor, from,
                                     socket->RemoteAddressIsValid ? &socket->RemoteAddress.sa : nullptr);
               LOG_WARNING
               stdlog << format("Buffer size is too small (buffer %zu, message %zu) for message on %s socket %d from %s!",
                                bufferSize, socket->MessageSize,
                                getProtocolName(socket->Protocol),
                                socket->SocketDescriptor,
                                peerStr.c_str()) << "\n";
               LOG_END
               socket->Status = Socket::MRS_StreamError;
               return MRRM_STREAM_ERROR;
            }
#if defined(HAVE_SCTP)
            if((socket->Protocol == IPPROTO_SCTP) && (!(*msgFlags & MSG_EOR))) {
               const std::string peerStr =
                  resolvePeerAddress(socket->SocketDescriptor, from,
                                     socket->RemoteAddressIsValid ? &socket->RemoteAddress.sa : nullptr);
               LOG_WARNING
               stdlog << format("TLV message end does not match with SCTP message end in message on %s socket %d from %s!",
                                getProtocolName(socket->Protocol),
                                socket->SocketDescriptor,
                                peerStr.c_str()) << "\n";
               LOG_END
               socket->Status = Socket::MRS_StreamError;
               return MRRM_STREAM_ERROR;
            }
#endif
            received = (ssize_t)socket->MessageSize;
            memcpy(buffer, socket->MessageBuffer, socket->MessageSize);
            socket->Status      = Socket::MRS_WaitingForHeader;
            socket->MessageSize = 0;
            socket->BytesRead   = 0;
            return received;
         }
         return MRRM_BAD_SOCKET;
      }

      // ====== Handle read errors ==========================================
      else if(received < 0) {
         if((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
            return MRRM_PARTIAL_READ;
         }
         return MRRM_SOCKET_ERROR;
      }

      // ====== Shutdown ====================================================
      else {   // received == 0
         return received;
      }
   }

   LOG_ERROR
   stdlog << format("Invalid socket %d!", sd) << "\n";
   LOG_END
   return MRRM_BAD_SOCKET;
}
