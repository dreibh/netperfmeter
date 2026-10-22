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

#include <cstdint>
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
   std::map<int, Socket*>::iterator iterator = SocketMap.begin();
   while(iterator != SocketMap.end()) {
      Socket* socket = iterator->second;
      deregisterSocket(socket->SocketDescriptor);
      iterator = SocketMap.begin();
   }
}


// ###### Register a socket #################################################
bool MessageReader::registerSocket(const int    protocol,
                                   const int    sd,
                                   const bool   mustBeNew,
                                   const size_t maxMessageSize)
{
   Socket*                          socket;
   std::map<int, Socket*>::iterator found = SocketMap.find(sd);
   if(found == SocketMap.end()) {
      assure(maxMessageSize >= sizeof(TLVHeader));

      socket = new Socket;
      assure(socket != nullptr);
      socket->MessageBuffer = new char[maxMessageSize];
      assure(socket->MessageBuffer != nullptr);
      socket->MessageBufferSize = maxMessageSize;
      socket->MessageSize       = 0;
      socket->BytesRead         = 0;
      socket->Status            = Socket::MRS_WaitingForHeader;
      socket->Protocol          = protocol;
      socket->SocketDescriptor  = sd;
      socket->UseCount          = 1;
      SocketMap.insert(std::pair<int, Socket*>(sd, socket));
   }
   else {
      if(mustBeNew) {
         LOG_FATAL
         // TRANSLATORS: Fatal error when attempting to register an already existing socket. Placeholder: {} is socket descriptor.
         stdlog << formatText(gettext("Socket {} should be new, but MessageReader already has a state!"),
                              sd) << "\n";
         abort();
         LOG_END_FATAL
      }
      socket = found->second;
      socket->UseCount++;
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
         delete[] socket->MessageBuffer;
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
   Socket* socket = getSocket(sd);
   if(socket != nullptr) {
      // ===== UDP ==========================================================
      if(socket->Protocol == IPPROTO_UDP) {
         // For UDP, reading always returns a full message. There is no
         // partial reading!
         const ssize_t received =
            ext_recvfrom(socket->SocketDescriptor,
                         buffer, bufferSize,
                         *msgFlags, from, fromSize);
         return received;
      }

      // ====== Find out the number of bytes to read ========================
      ssize_t received;
      size_t  bytesToRead;
      if( (socket->Protocol == IPPROTO_TCP)
#if defined(HAVE_SCTP)
          || (socket->Protocol == IPPROTO_SCTP)
#endif
#if defined(HAVE_MPTCP)
          || (socket->Protocol == IPPROTO_MPTCP)
#endif
#if defined(HAVE_QUIC)
          || (socket->Protocol == IPPROTO_QUIC)
#endif
          || (false)   /* Avoid compile warning without any #ifdef case */
        ) {
         // SCTP, TCP/MPTCP and QUIC can return partial messages upon recv() calls.
         // TCP/MPTCP and QUIC may even return multiple messages, if the buffer
         // size is large enough!
         if(socket->Status == Socket::MRS_WaitingForHeader) {
            assure(sizeof(TLVHeader) >= socket->BytesRead);
            bytesToRead = sizeof(TLVHeader) - socket->BytesRead;
         }
         else if(socket->Status == Socket::MRS_PartialRead) {
            bytesToRead = socket->MessageSize - socket->BytesRead;
         }
         else {
#if defined(HAVE_SCTP)
            if(socket->Protocol == IPPROTO_SCTP) {
               // An error occurred before. Reset and try again ...
               socket->Status    = Socket::MRS_WaitingForHeader;
               socket->BytesRead = 0;
               bytesToRead       = sizeof(TLVHeader);
            }
            else {
#endif
               // Not useful to retry when synchronization has been lost!
               return MRRM_STREAM_ERROR;
#if defined(HAVE_SCTP)
            }
#endif
         }
         assure(bytesToRead + socket->BytesRead <= socket->MessageBufferSize);
      }
      else {
         // DCCP and UDP will always return only a single message on recv() calls.
         bytesToRead = socket->MessageBufferSize;
      }


      // ====== Read from socket ============================================
      int flags;
      if(msgFlags == nullptr) {
         flags    = 0;
         msgFlags = &flags;
      }
      if(from) {
         memset(from, 0, *fromSize);   // Clear address (Valgrind report)
      }
      if(0) { /* Dummy for following "else if" in #if ... #endif block */ }
#if defined(HAVE_SCTP)
      else if(socket->Protocol == IPPROTO_SCTP) {
         sctp_sndrcvinfo sinfo;
         received = sctp_recvmsg(socket->SocketDescriptor,
                                 &socket->MessageBuffer[socket->BytesRead], bytesToRead,
                                 from, fromSize, &sinfo, msgFlags);
         if(streamID != nullptr) {
            *streamID = static_cast<int64_t>(sinfo.sinfo_stream);
         }
      }
#endif
#if defined(HAVE_QUIC)
      else if(socket->Protocol == IPPROTO_QUIC) {
         uint32_t flags = 0;
         received = quic_recvmsg(socket->SocketDescriptor,
                                 &socket->MessageBuffer[socket->BytesRead], bytesToRead,
                                 streamID, &flags);
      }
#endif
      else {
         received = ext_recvfrom(socket->SocketDescriptor,
                                 &socket->MessageBuffer[socket->BytesRead], bytesToRead,
                                 *msgFlags, from, fromSize);
      }


      // ====== Handle received data ========================================
      if(received > 0) {
         socket->BytesRead += static_cast<size_t>(received);
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
               // need to look for MSG_EOF!
            }
            // ====== Handle TLV header =====================================
            else {
#endif
               if(socket->BytesRead >= sizeof(TLVHeader)) {
                  const auto* header = reinterpret_cast<const TLVHeader*>(socket->MessageBuffer);
#if defined(DEBUG_MESSAGEREADER)
                  printf("Socket %d:   header T=%u F=%02x L=%u   [Header]\n",
                          socket->SocketDescriptor,
                          (unsigned int)header->Type, (unsigned int)header->Flags,
                         be16toh(header->Length));
#endif
                  socket->MessageSize = be16toh(header->Length);
                  if(socket->MessageSize < sizeof(TLVHeader)) {
                     LOG_WARNING
                     // TRANSLATORS: Warning when message length is smaller than TLV header size. Placeholder: {} is socket descriptor.
                     stdlog << formatText(gettext("Message size < TLV size in message from socket {}!"),
                                          socket->SocketDescriptor) << "\n";
                     LOG_END
                     socket->Status = Socket::MRS_StreamError;
                     return MRRM_STREAM_ERROR;
                  }
                  else if(socket->MessageSize > socket->MessageBufferSize) {
                     LOG_WARNING
                     // TRANSLATORS: Warning when message size exceeds the internal buffer capacity. Placeholder: {} is socket descriptor.
                     stdlog << formatText(gettext("Message too large to fit buffer for message from socket {}!"),
                                          socket->SocketDescriptor) << "\n";
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
                        LOG_WARNING
                        // TRANSLATORS: Warning when SCTP message end occurs before TLV payload completes. Placeholders: {} 1: bytes read, 2: expected bytes, 3: socket descriptor.
                        stdlog << formatText(gettext("SCTP message end before TLV message end (read {}, expected {}) for message from socket {}!"),
                                             static_cast<unsigned int>(socket->BytesRead),
                                             static_cast<unsigned int>(socket->MessageSize),
                                             socket->SocketDescriptor) << "\n";
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
            if(socket->MessageSize > bufferSize) {
               LOG_WARNING
               // TRANSLATORS: Warning when destination buffer is smaller than received message size. Placeholders: {} 1: destination buffer size, 2: message size, 3: socket descriptor.
               stdlog << formatText(gettext("Buffer size is too small (buffer {}, message {}) for message from socket {}!"),
                                    bufferSize, socket->MessageSize, socket->SocketDescriptor) << "\n";
               LOG_END
               socket->Status = Socket::MRS_StreamError;
               return MRRM_STREAM_ERROR;
            }
            if((socket->Protocol == IPPROTO_SCTP) && (!(*msgFlags & MSG_EOR))) {
               LOG_WARNING
               // TRANSLATORS: Warning when TLV message boundary does not align with SCTP message boundary. Placeholder: {} is socket descriptor.
               stdlog << formatText(gettext("TLV message end does not match with SCTP message end in message from socket {}!"),
                                    socket->SocketDescriptor) << "\n";
               LOG_END
               socket->Status = Socket::MRS_StreamError;
               return MRRM_STREAM_ERROR;
            }
            received = static_cast<ssize_t>(socket->MessageSize);
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
         return MRRM_SOCKET_ERROR;
      }

      // ====== Shutdown ====================================================
      else {   // received == 0
         return received;
      }
   }

   LOG_ERROR
   // TRANSLATORS: Error message when attempting to read from an invalid socket descriptor. Placeholder: {} is socket descriptor.
   stdlog << formatText(gettext("Invalid socket {}!"), sd) << "\n";
   LOG_END
   return MRRM_BAD_SOCKET;
}
