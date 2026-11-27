# ==========================================================================
#         _   _      _   ____            __ __  __      _
#        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
#        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
#        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
#        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
#
#                  NetPerfMeter -- Network Performance Meter
#                 Copyright (C) 2009-2026 by Thomas Dreibholz
# ==========================================================================
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact:  dreibh@simula.no
# Homepage: https://www.nntb.no/~dreibh/netperfmeter/

#############################################################################
#### SATURATED BULK FLOW TESTS WITH MULTI-MESSAGE FRAMING                ####
#############################################################################

# ====== 1. Saturated TCP Flows with Message Segmentation ===================
# 16 KB frames segmented into 1,400 B messages (~12 messages per frame), saturated
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_saturated_large_frames
   127.0.0.1 tcp const0:const16000:const0:const16000:maxmsgsize=1400 plain 3)

# 64 KB jumbo frames segmented into 1,400 B messages (~46 messages per frame), saturated
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_saturated_jumbo_frames
   127.0.0.1 tcp const0:const65536:const0:const65536:maxmsgsize=1400 plain 3)

# Saturated upload-only TCP with large framed messages
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_saturated_upload_segmented
   127.0.0.1 tcp const0:const32000:const0:const0:maxmsgsize=1400 plain 3)


# ====== 2. Saturated MPTCP Flows with Message Segmentation =================
IF (HAVE_MPTCP_FOR_TESTS)

# 16 KB frames segmented into 1,400 B messages (~12 messages per frame), saturated
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_saturated_large_frames
   127.0.0.1 mptcp const0:const16000:const0:const16000:maxmsgsize=1400 plain 3)

# 64 KB jumbo frames segmented into 1,400 B messages (~46 messages per frame), saturated
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_saturated_jumbo_frames
   127.0.0.1 mptcp const0:const65536:const0:const65536:maxmsgsize=1400 plain 3)

# Saturated upload-only MPTCP with large framed messages
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_saturated_upload_segmented
   127.0.0.1 mptcp const0:const32000:const0:const0:maxmsgsize=1400 plain 3)

# Saturated MPTCP with control channel running over MPTCP
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_saturated_ctrl_mptcp
   127.0.0.1 mptcp const0:const16000:const0:const16000:maxmsgsize=1400 plain 3 --control-over-mptcp)

ENDIF()


# ====== 3. Saturated SCTP Flows with Multi-Message Chunking ================
IF (HAVE_SCTP_FOR_TESTS)

# Saturated SCTP bidirectional transfer: 16 KB frames fragmented into 1,400 B chunks
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_saturated_large_frames
   127.0.0.1 sctp const0:const16000:const0:const16000:maxmsgsize=1400 plain 3)

# Saturated SCTP transfer with 64 KB jumbo frames fragmented into 1,000 B chunks
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_saturated_jumbo_frames
   127.0.0.1 sctp const0:const65536:const0:const65536:maxmsgsize=1000 plain 3)

# Saturated SCTP multi-stream: Stream 0 (16 KB frames) and Stream 1 (32 KB frames)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_sctp_saturated_multistream
   127.0.0.1 sctp const0:const16000:const0:const16000:maxmsgsize=1400 plain 3
   -sctp const0:const32000:const0:const32000:maxmsgsize=1400)

# Saturated SCTP with control channel running over SCTP
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_saturated_ctrl_sctp
   127.0.0.1 sctp const0:const16000:const0:const16000:maxmsgsize=1400 plain 3 --control-over-sctp)

ENDIF()


# ====== 4. Saturated QUIC Flows with Multi-Message Framing =================
IF (HAVE_QUIC_FOR_TESTS)

# Saturated QUIC bidirectional transfer: 16 KB frames fragmented into 1,400 B chunks
ADD_NETPERFMETER_FLOW_TEST(flow_quic_saturated_large_frames
   127.0.0.1 quic const0:const16000:const0:const16000:maxmsgsize=1400 plain 3)

# Saturated QUIC transfer with 64 KB jumbo frames fragmented into 1,000 B chunks
ADD_NETPERFMETER_FLOW_TEST(flow_quic_saturated_jumbo_frames
   127.0.0.1 quic const0:const65536:const0:const65536:maxmsgsize=1000 plain 3)

# Saturated upload-only QUIC with large framed messages
ADD_NETPERFMETER_FLOW_TEST(flow_quic_saturated_upload_segmented
   127.0.0.1 quic const0:const32000:const0:const0:maxmsgsize=1400 plain 3)

# Saturated QUIC with control channel running over QUIC
ADD_NETPERFMETER_FLOW_TEST(flow_quic_saturated_ctrl_quic
   127.0.0.1 quic const0:const16000:const0:const16000:maxmsgsize=1400 plain 3 --control-over-quic)

# Concurrent saturated QUIC connections: Connection 0 (16 KB frames) and Connection 1 (32 KB frames)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_2connections_saturated
   127.0.0.1 quic const0:const16000:const0:const16000:maxmsgsize=1400 plain 3
   -quic const0:const32000:const0:const32000:maxmsgsize=1400)

ENDIF()
