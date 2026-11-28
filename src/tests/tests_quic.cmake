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
#### QUIC FLOW INTEGRATION TESTS (RFC 9000 / lxin-quic)                  ####
#############################################################################

IF (HAVE_QUIC_FOR_TESTS)

# ====== 1. Bidirectional QUIC Flows ========================================
ADD_NETPERFMETER_FLOW_TEST(flow_quic_bidi_ipv4 127.0.0.1 quic const10:const1000:const10:const1000 plain 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_bidi_ipv6 ::1       quic const10:const1000:const10:const1000 plain 3)

# ====== 2. Server-Only TLS Authentication, Default Shorthand, CC & Bind ====
ADD_NETPERFMETER_FLOW_TEST(flow_quic_server_auth_only_ipv4 127.0.0.1 quic const10:const1000:const10:const1000 plain 3 --no-mtls)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_server_auth_only_ipv6 ::1       quic const10:const1000:const10:const1000 plain 3 --no-mtls)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_default_shorthand     127.0.0.1 quic default plain 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_cc_cubic              127.0.0.1 quic const10:const1000:const10:const1000:cc=cubic plain 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_cc_reno               127.0.0.1 quic const10:const1000:const10:const1000:cc=reno plain 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_local_bind_ipv4
   127.0.0.1 quic const10:const1000:const10:const1000 plain 3
   -L 127.0.0.1 -l 127.0.0.1
   --server-arg -L --server-arg 127.0.0.1 --server-arg -l --server-arg 127.0.0.1)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_local_bind_ipv6
   ::1 quic const10:const1000:const10:const1000 plain 3
   -L ::1 -l ::1
   --server-arg -L --server-arg ::1 --server-arg -l --server-arg ::1)

# ====== 3. Unidirectional QUIC Flows =======================================
ADD_NETPERFMETER_FLOW_TEST(flow_quic_upload_ipv4   127.0.0.1 quic const10:const1000:const0:const0 plain 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_upload_ipv6   ::1       quic const10:const1000:const0:const0 plain 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_download_ipv4 127.0.0.1 quic const0:const0:const10:const1000 plain 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_download_ipv6 ::1       quic const0:const0:const10:const1000 plain 3)

# ====== 3b. Compression Formats, Output Suppression & Defragtimeout ========
ADD_NETPERFMETER_FLOW_TEST(flow_quic_format_gz     127.0.0.1 quic const10:const1000:const10:const1000 gz 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_format_bz2    127.0.0.1 quic const10:const1000:const10:const1000 bz2 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_format_xz     127.0.0.1 quic const10:const1000:const10:const1000 xz 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_format_zst    127.0.0.1 quic const10:const1000:const10:const1000 zst 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_no_scalar     127.0.0.1 quic const10:const1000:const10:const1000 plain 3 --no-scalar)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_no_vector     127.0.0.1 quic const10:const1000:const10:const1000 plain 3 --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_no_outputs    127.0.0.1 quic const10:const1000:const10:const1000 plain 3 --no-scalar --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_defragtimeout 127.0.0.1 quic const10:const4000:const10:const4000:maxmsgsize=1000:defragtimeout=2500 plain 3)

# ====== 4. Homogeneous Multi-Connection QUIC Tests (Multiple Sockets) ======
# 2 concurrent QUIC connections over IPv4 (2x 10 fps, 1000 B)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_2connections_ipv4
   127.0.0.1 quic const10:const1000:const10:const1000 plain 3
   -quic const10:const1000:const10:const1000)

# 4 concurrent QUIC connections over IPv4 (4x 10 fps, 1000 B) - 5s for scheduler stability
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_4connections_ipv4
   127.0.0.1 quic const10:const1000:const10:const1000 plain 5
   -quic const10:const1000:const10:const1000
   -quic const10:const1000:const10:const1000
   -quic const10:const1000:const10:const1000)

# 2 concurrent QUIC connections over IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_2connections_ipv6
   ::1       quic const10:const1000:const10:const1000 plain 3
   -quic const10:const1000:const10:const1000)

# 4 concurrent QUIC connections over IPv6 - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_4connections_ipv6
   ::1       quic const10:const1000:const10:const1000 plain 5
   -quic const10:const1000:const10:const1000
   -quic const10:const1000:const10:const1000
   -quic const10:const1000:const10:const1000)

# 2 competing QUIC connections: interactive small frames (25 fps, 100 B) vs bulk frames (10 fps, 1400 B)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_heterogeneous_rates_sizes
   127.0.0.1 quic const25:const100:const25:const100 plain 5
   -quic const10:const1400:const10:const1400)


# ====== 5. Single-Connection Multi-Stream QUIC Tests (Multiple Streams) ====
# 2 streams within 1 QUIC connection over IPv4
ADD_NETPERFMETER_FLOW_TEST(flow_quic_multistream_2streams_ipv4
   127.0.0.1 quic const10:const1000:const10:const1000 plain 3
   --stream const10:const500:const10:const500)

# 2 streams within 1 QUIC connection over IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_quic_multistream_2streams_ipv6
   ::1       quic const10:const1000:const10:const1000 plain 3
   --stream const10:const500:const10:const500)

# 4 streams within 1 QUIC connection (5, 10, 15, and 20 fps) - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_quic_multistream_4streams_ipv4
   127.0.0.1 quic const5:const500:const5:const500 plain 5
   --stream const10:const500:const10:const500
   --stream const15:const500:const15:const500
   --stream const20:const500:const20:const500)

# Asymmetric multi-stream: Stream 0 upload-only, Stream 1 download-only in 1 connection
ADD_NETPERFMETER_FLOW_TEST(flow_quic_multistream_asymmetric_split
   127.0.0.1 quic const15:const1000:const0:const0 plain 3
   --stream const0:const0:const15:const1000)

# 2 streams within 1 QUIC connection with message chunking (maxmsgsize=500)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_multistream_2streams_fragmented
   127.0.0.1 quic const10:const2000:const10:const2000:maxmsgsize=500 plain 3
   --stream const10:const2000:const10:const2000:maxmsgsize=500)

# 2 streams within 1 QUIC connection with staggered On-Off schedules - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_quic_multistream_staggered_onoff
   127.0.0.1 quic const20:const1000:const0:const0:onoff=1,3 plain 5
   --stream const20:const1000:const0:const0:onoff=2,4)

# 2 streams within 1 QUIC connection with control channel over QUIC
ADD_NETPERFMETER_FLOW_TEST(flow_quic_multistream_2streams_ctrl_quic
   127.0.0.1 quic const10:const1000:const10:const1000 plain 3
   --stream const10:const500:const10:const500 --control-over-quic)

# Traffic Class, IPv6 Flow Label, and per-flow QUIC idle timeout (enabled, disabled, multi-stream matching)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_tc_and_idle_timeout_ipv4
   127.0.0.1 quic const10:const1000:const10:const1000:tc=AF41:hbinterval=5000:hbassocmaxrtx=4 plain 3
   --control-over-quic -Q CS6 --server-arg -Q --server-arg CS6
   --expect-config "FLOW0_TRAFFIC_CLASS=\"0x88\"")
ADD_NETPERFMETER_FLOW_TEST(flow_quic_tc_fl_hb_disabled_ipv6
   ::1       quic const10:const1000:const10:const1000:tc=EF:fl=auto:hbinterval=0 plain 3
   --expect-config "FLOW0_HBINTERVAL=\"0\"")
ADD_NETPERFMETER_FLOW_TEST(flow_quic_multistream_shared_tc_hb
   127.0.0.1 quic const10:const1000:const10:const1000:tc=AF21:hbinterval=5000:hbassocmaxrtx=4 plain 3
   --stream const10:const500:const10:const500:tc=AF21:hbinterval=5000:hbassocmaxrtx=4
   --expect-config "FLOW1_TRAFFIC_CLASS=\"0x48\"")


# ====== 6. Heterogeneous Cross-Traffic Concurrency =========================
# Competing QUIC + TCP connections
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_tcp_cross_traffic
   127.0.0.1 quic const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000)

# Competing QUIC + UDP flows
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_udp_cross_traffic
   127.0.0.1 quic const10:const1000:const10:const1000 plain 3
   -udp const10:const1000:const10:const1000)

# Concurrent QUIC + TCP + UDP competing flows
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_tcp_udp_cross
   127.0.0.1 quic const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)

IF (HAVE_MPTCP_FOR_TESTS)
   # Competing MPTCP + QUIC connections
   ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_quic_cross_traffic
      127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
      -quic const10:const1000:const10:const1000)
ENDIF()

IF (HAVE_SCTP_FOR_TESTS)
   # Full quad-protocol concurrency (QUIC + SCTP + TCP + UDP) - 5s
   ADD_NETPERFMETER_FLOW_TEST(flow_multi_all_protocols_concurrent
      127.0.0.1 quic const10:const1000:const10:const1000 plain 5
      -sctp const10:const1000:const10:const1000
      -tcp  const10:const1000:const10:const1000
      -udp  const10:const1000:const10:const1000)
ENDIF()

IF (HAVE_SCTP_FOR_TESTS AND HAVE_MPTCP_FOR_TESTS)
   # Full 5-protocol concurrency (QUIC + MPTCP + SCTP + TCP + UDP) - 5s
   ADD_NETPERFMETER_FLOW_TEST(flow_multi_all_5_protocols_concurrent
      127.0.0.1 quic const10:const1000:const10:const1000 plain 5
      -mptcp const10:const1000:const10:const1000
      -sctp  const10:const1000:const10:const1000
      -tcp   const10:const1000:const10:const1000
      -udp   const10:const1000:const10:const1000)
ENDIF()

IF (HAVE_SCTP_FOR_TESTS AND HAVE_MPTCP_FOR_TESTS AND HAVE_DCCP_FOR_TESTS)
   # Full 6-protocol concurrency (QUIC + MPTCP + SCTP + DCCP + TCP + UDP) - 5s
   ADD_NETPERFMETER_FLOW_TEST(flow_multi_all_6_protocols_concurrent
      127.0.0.1 quic const10:const1000:const10:const1000 plain 5
      -mptcp const10:const1000:const10:const1000
      -sctp  const10:const1000:const10:const1000
      -dccp  const10:const1000:const10:const1000
      -tcp   const10:const1000:const10:const1000
      -udp   const10:const1000:const10:const1000)
ENDIF()


# ====== 7. Control Channel Over QUIC =======================================
# Control channel using QUIC with QUIC data flow over IPv4 and IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_quic_ctrl_quic_bidi_ipv4
   127.0.0.1 quic const10:const1000:const10:const1000 plain 3 --control-over-quic)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_ctrl_quic_bidi_ipv6
   ::1       quic const10:const1000:const10:const1000 plain 3 --control-over-quic)

# Control channel using QUIC with TCP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_ctrl_quic_bidi_ipv4
   127.0.0.1 tcp  const10:const1000:const10:const1000 plain 3 --control-over-quic)

# Control channel using QUIC with UDP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_udp_ctrl_quic_bidi_ipv4
   127.0.0.1 udp  const10:const1000:const10:const1000 plain 3 --control-over-quic)

# 2 QUIC connections with control channel over QUIC
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quic_2connections_ctrl_quic
   127.0.0.1 quic const10:const1000:const10:const1000 plain 3
   -quic const10:const500:const10:const500 --control-over-quic)

# QUIC PING keep-alive probes keep an idle QUIC control channel alive well past max_idle_timeout
# (hbinterval=500ms, hbassocmaxrtx=3 -> max_idle_timeout=1.5s, runtime=5s > 3x max_idle_timeout)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_ctrl_keepalive_survives_idle_ipv4
   127.0.0.1 udp const10:const500:const10:const500 plain 5
   --control-over-quic
   --control-hbinterval 500 --control-hbassocmaxrtx 3
   --server-arg --control-hbinterval --server-arg 500
   --server-arg --control-hbassocmaxrtx --server-arg 3
   --expect-config "CONTROL_HBINTERVAL=500")
ADD_NETPERFMETER_FLOW_TEST(flow_quic_ctrl_keepalive_survives_idle_ipv6
   ::1 udp const10:const500:const10:const500 plain 5
   --control-over-quic
   --control-hbinterval 500 --control-hbassocmaxrtx 3
   --server-arg --control-hbinterval --server-arg 500
   --server-arg --control-hbassocmaxrtx --server-arg 3)

# QUIC PING keep-alive probes keep silent On-Off QUIC data flows alive past max_idle_timeout
# (0..3s silent > 1.5s max_idle_timeout, then 3..5s active; also verifies passive max_idle_timeout=0 negotiation)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_data_keepalive_survives_onoff_silence_ipv4
   127.0.0.1 quic const20:const1000:const0:const0:onoff=3:hbinterval=500:hbassocmaxrtx=3 plain 5
   --expect-config "FLOW0_HBINTERVAL=\"500\"")
ADD_NETPERFMETER_FLOW_TEST(flow_quic_data_keepalive_survives_onoff_silence_ipv6
   ::1 quic const20:const1000:const0:const0:onoff=3:hbinterval=500:hbassocmaxrtx=3 plain 5)

# Multi-stream QUIC data + QUIC control both surviving silent intervals past max_idle_timeout
ADD_NETPERFMETER_FLOW_TEST(flow_quic_multistream_keepalive_survives_onoff_silence
   127.0.0.1 quic const20:const1000:const0:const0:onoff=3:hbinterval=500:hbassocmaxrtx=3 plain 5
   --stream const20:const500:const0:const0:onoff=3:hbinterval=500:hbassocmaxrtx=3
   --control-over-quic
   --control-hbinterval 500 --control-hbassocmaxrtx 3
   --server-arg --control-hbinterval --server-arg 500
   --server-arg --control-hbassocmaxrtx --server-arg 3)

IF (HAVE_SCTP_FOR_TESTS)
   # Control channel using QUIC with SCTP data flow
   ADD_NETPERFMETER_FLOW_TEST(flow_sctp_ctrl_quic_bidi_ipv4
      127.0.0.1 sctp const10:const1000:const10:const1000 plain 3 --control-over-quic)

   # Multi-association, multi-stream QoS SCTP (3 associations x 3 streams = 9 flows) over QUIC control
   # running > 3x max_idle_timeout (1.5s vs 5s runtime) with --loglevel 4 on server
   ADD_NETPERFMETER_FLOW_TEST(flow_sctp_3assocs_9flows_ctrl_quic_keepalive
      127.0.0.1 sctp const10:const200:const10:const200:tc=BE plain 5
      --stream const10:const500:const10:const500:onoff=2,4:tc=BE
      --stream const0:const0:const10:const1543:maxmsgsize=1400:tc=BE
      -sctp const10:const200:const10:const200:tc=AF21
      --stream const10:const500:const10:const500:onoff=2,4:tc=AF21
      --stream const0:const0:const10:const1543:maxmsgsize=1400:tc=AF21
      -sctp const10:const200:const10:const200:tc=EF
      --stream const10:const500:const10:const500:onoff=2,4:tc=EF
      --stream const0:const0:const10:const1543:maxmsgsize=1400:tc=EF
      --control-over-quic
      --control-hbinterval 500 --control-hbassocmaxrtx 3
      --server-arg --loglevel --server-arg 4
      --server-arg --control-hbinterval --server-arg 500
      --server-arg --control-hbassocmaxrtx --server-arg 3)
ENDIF()

IF (HAVE_MPTCP_FOR_TESTS)
   # Control channel using QUIC with MPTCP data flow
   ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_ctrl_quic_bidi_ipv4
      127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3 --control-over-quic)

   # Control channel using MPTCP with QUIC data flow
   ADD_NETPERFMETER_FLOW_TEST(flow_quic_ctrl_mptcp_bidi_ipv4
      127.0.0.1 quic  const10:const1000:const10:const1000 plain 3 --control-over-mptcp)
ENDIF()


# ===========================================================================
# 50 REALISTIC QUIC APPLICATION DISTRIBUTION PROFILES (Profiles 151 - 200)
# (RFC 9000 QUIC, RFC 9114 HTTP/3, RFC 9297 WebTransport, RFC 9250 DoQ)
# ===========================================================================

# ====== 22. HTTP/3 & WebTransport Microservices (RFC 9114, RFC 9297) =======
# 151. HTTP/3 REST Microservice API (~30 fps, 350 B request -> Exp mean 1000 B response)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_h3_rest_api
   127.0.0.1 quic uniform25,35:uniform250,450:uniform25,35:exp1000 plain 8)

# 152. Bidirectional gRPC over HTTP/3 over IPv6 (~35 fps, 400 B symmetric, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_h3_grpc_bidi_v6
   ::1       quic uniform30,40:truncnormal400,40:uniform30,40:truncnormal400,40 plain 8)

# 153. WebTransport Unreliable Datagram Stream (60 fps, 250 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_webtransport_datagram
   127.0.0.1 quic uniform55,65:uniform200,300:uniform55,65:uniform200,300 plain 8)

# 154. HTTP/3 Server Push Asset Stream (15 fps, Pareto mean 1500 B, pareto)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_h3_server_push
   127.0.0.1 quic const15:pareto1000,3:const15:pareto1000,3 plain 8)

# 155. GraphQL Complex Queries over HTTP/3 over IPv6 (20 fps, 300 B query -> Exp 1200 B reply)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_h3_graphql_query_v6
   ::1       quic const20:uniform200,400:const20:exp1200 plain 8)

# 156. WebTransport Reliable Stream Synchronization (40 fps, 500 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_webtransport_bidi_stream
   127.0.0.1 quic const40:truncnormal500,40:const40:truncnormal500,40 plain 8)

# 157. HTTP/3 Segmented Asset Transfer with Message Chunking (10 fps, mean 3000 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_h3_chunked_asset_frag
   127.0.0.1 quic const10:truncnormal3000,200:const10:truncnormal3000,200:maxmsgsize=1000 plain 8)

# 158. Server-Sent Events (SSE) Push over HTTP/3 over IPv6 (25 fps, 180 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_h3_event_source_v6
   ::1       quic uniform20,30:uniform120,240:uniform20,30:uniform120,240 plain 8)


# ====== 23. Media over QUIC (MoQ) & Real-Time Audio/Video ==================
# 159. Media over QUIC (MoQ) Ultra-Low Latency Video Ingest (30 fps, Pareto mean 2250 B)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_moq_live_ingest
   127.0.0.1 quic const30:pareto1500,3:const30:pareto1500,3 plain 8)

# 160. MoQ Multi-Track Opus Stereo Audio Stream over IPv6 (50 fps, 400 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_moq_audio_track_v6
   ::1       quic const50:uniform300,500:const50:uniform300,500 plain 8)

# 161. MoQ Subgroup Object Chunking with Segmentation (20 fps, 3600 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_moq_subgroup_frag
   127.0.0.1 quic const20:truncnormal3600,250:const20:truncnormal3600,250:maxmsgsize=1200 plain 8)

# 162. AV1 Low-Latency Interactive Video Feed (60 fps, 1100 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_video_av1_cbr
   127.0.0.1 quic truncnormal60,3:uniform1000,1200:truncnormal60,3:uniform1000,1200 plain 8)

# 163. WebRTC Media Engine over QUIC Transport over IPv6 (45 fps, 800 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_webrtc_media_over_quic_v6
   ::1       quic uniform40,50:uniform600,1000:uniform40,50:uniform600,1000 plain 8)

# 164. 4K CCTV H.265 Security Camera Stream (25 fps, Pareto mean 1800 B)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_surveillance_h265
   127.0.0.1 quic const25:pareto1200,3:const25:pareto1200,3 plain 8)

# 165. Drone FPV Teleoperation Video Feed (60 fps, 750 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_drone_fpv_video
   127.0.0.1 quic const60:truncnormal750,50:const60:truncnormal750,50 plain 8)


# ====== 24. DNS over Dedicated QUIC (DoQ, RFC 9250) & Security =============
# 166. DNS-over-QUIC (DoQ) Stub Resolver Queries (20 fps, 100 B request -> 300 B response)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_doq_dns_queries
   127.0.0.1 quic uniform15,25:uniform80,120:uniform15,25:uniform200,400 plain 8)

# 167. DoQ Recursive Resolver Authoritative Sync over IPv6 (30 fps, 150 B -> Exp 600 B)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_doq_recursive_sync_v6
   ::1       quic const30:uniform100,200:const30:exp600 plain 8)

# 168. DoQ AXFR/IXFR Incremental Zone Transfer with Chunking (8 fps, 2800 B, maxmsgsize=900)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_doq_zone_transfer_frag
   127.0.0.1 quic const8:truncnormal2800,200:const8:truncnormal2800,200:maxmsgsize=900 plain 8)

# 169. Certificate Transparency SCT Verification Audit (15 fps, 500 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_cert_transparency_log
   127.0.0.1 quic uniform12,18:uniform400,600:uniform12,18:uniform400,600 plain 8)

# 170. Real-Time OCSP Revocation Status Probing over IPv6 (25 fps, 350 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_ocsp_stapling_check_v6
   ::1       quic const25:truncnormal350,30:const25:truncnormal350,30 plain 8)

# 171. Cloud KMS Key Attestation & Secret Exchange (10 fps, 700 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_kms_key_attestation
   127.0.0.1 quic const10:uniform600,800:const10:uniform600,800 plain 8)


# ====== 25. Cloud Gaming, Metaverse & Virtual Worlds =======================
# 172. 120Hz Competitive Gaming Ultra-Low Latency Controller Input (~120 fps, ~80 B, realistic input jitter)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_gaming_controller_input
   127.0.0.1 quic truncnormal120,4:uniform70,90:truncnormal120,4:uniform70,90 plain 8)

# 173. Multiplayer Game Entity World State Updates over IPv6 (64 fps, 160 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_gaming_state_sync_v6
   ::1       quic truncnormal64,3:uniform120,200:truncnormal64,3:uniform120,200 plain 8)

# 174. Bilateral Telepresence Haptic Force Feedback Loop (~100 fps, ~100 B, realistic input jitter)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_haptic_feedback_stream
   127.0.0.1 quic truncnormal100,3:uniform90,110:truncnormal100,3:uniform90,110 plain 8)

# 175. AR/VR 6-DoF Head/Hand Spatial Pose Tracking over IPv6 (90 fps, 120 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_ar_pose_tracking_v6
   ::1       quic truncnormal90,4:uniform90,150:truncnormal90,4:uniform90,150 plain 8)

# 176. Volumetric 3D Holographic Mesh Sync with Segmentation (12 fps, 3500 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_volumetric_mesh_frag
   127.0.0.1 quic const12:truncnormal3500,200:const12:truncnormal3500,200:maxmsgsize=1000 plain 8)

# 177. 3D Spatial Audio Spatialization Coordinate Stream (50 fps, 140 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_spatial_audio_metadata
   127.0.0.1 quic uniform45,55:uniform100,180:uniform45,55:uniform100,180 plain 8)


# ====== 26. MASQUE, Proxying & Encrypted Tunneling (RFC 9298) ==============
# 178. MASQUE CONNECT-UDP Tunnel Packet Forwarding (~50 fps, 450 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_masque_connect_udp
   127.0.0.1 quic uniform40,60:uniform300,600:uniform40,60:uniform300,600 plain 8)

# 179. Enterprise WireGuard/QUIC Encrypted Mesh Backhaul over IPv6 (40 fps, 800 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_vpn_mesh_backhaul_v6
   ::1       quic const40:truncnormal800,60:const40:truncnormal800,60 plain 8)

# 180. MASQUE IP Proxying Tunnel with MTU Segmentation (20 fps, 2400 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_masque_ip_tunnel_frag
   127.0.0.1 quic const20:truncnormal2400,150:const20:truncnormal2400,150:maxmsgsize=1200 plain 8)

# 181. Oblivious HTTP (OHTTP) Encrypted Relay Exchange (25 fps, 350 B request -> Exp 900 B reply)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_oblivious_http_relay
   127.0.0.1 quic uniform20,30:uniform200,500:uniform20,30:exp900 plain 8)

# 182. Cloudflare WARP-style Edge Ingress Tunnel over IPv6 (35 fps, 650 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_split_tunnel_ingress_v6
   ::1       quic uniform30,40:uniform500,800:uniform30,40:uniform500,800 plain 8)


# ====== 27. Financial FinTech, Market Feeds & Microsecond Quotes ===========
# 183. ITCH-style Level-2 Market Depth Feed over QUIC (80 fps, 120 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_fin_itch_feed
   127.0.0.1 quic truncnormal80,4:uniform90,150:truncnormal80,4:uniform90,150 plain 8)

# 184. Crypto Perpetual Futures Order Book Stream over IPv6 (40 fps, Pareto mean 750 B)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_fin_crypto_l2_book_v6
   ::1       quic const40:pareto500,3:const40:pareto500,3 plain 8)

# 185. FIX Protocol Direct Market Access over QUIC (50 fps, 320 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_fin_fix_orders
   127.0.0.1 quic uniform40,60:uniform220,420:uniform40,60:uniform220,420 plain 8)

# 186. High-Value Inter-Bank Clearing Batch Transfer (10 fps, 3200 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_fin_settlement_batch_frag
   127.0.0.1 quic const10:truncnormal3200,200:const10:truncnormal3200,200:maxmsgsize=1000 plain 8)

# 187. Trade Allocation Drop Copy Execution Reports over IPv6 (30 fps, 450 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_fin_drop_copy_v6
   ::1       quic const30:truncnormal450,35:const30:truncnormal450,35 plain 8)

# 188. Ultra-Low Latency Pre-Trade Risk Heartbeat (~100 fps, ~64 B, realistic input jitter)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_fin_hft_risk_heartbeat
   127.0.0.1 quic truncnormal100,3:uniform56,72:truncnormal100,3:uniform56,72 plain 8)


# ====== 28. IoT, Connected Vehicles (C-V2X) & Smart Grid ===================
# 189. C-V2X Cooperative Awareness Safety Message Beacon (10 fps, 350 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_v2x_cam_safety
   127.0.0.1 quic truncnormal10,1:uniform250,450:truncnormal10,1:uniform250,450 plain 8)

# 190. ISO 15118 EV High-Power Charging Telemetry over IPv6 (5 fps, 250 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_ev_charging_telemetry_v6
   ::1       quic const5:uniform180,320:const5:uniform180,320 plain 8)

# 191. IEEE C37.118 Substation Phasor Measurement Unit (50 fps, 140 B, const)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_smartgrid_synchrophasor
   127.0.0.1 quic const50:const140:const50:const140 plain 8)

# 192. Autonomous AGV Warehouse Robot LIDAR Telemetry (40 fps, 1200 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_industrial_agv_telemetry
   127.0.0.1 quic uniform35,45:uniform1000,1400:uniform35,45:uniform1000,1400 plain 8)

# 193. CoAP-over-QUIC Environmental Sensor Array over IPv6 (15 fps, 120 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_coap_sensor_mesh_v6
   ::1       quic uniform10,20:uniform90,150:uniform10,20:uniform90,150 plain 8)

# 194. Factory Floor PLC Line Polling & State Verification (20 fps, 220 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_scada_plc_line_polling
   127.0.0.1 quic const20:truncnormal220,20:const20:truncnormal220,20 plain 8)


# ====== 29. Satellite, 5G NTN, Aerospace & Distributed AI ==================
# 195. LEO Constellation Satellite Telemetry Downlink (25 fps, 750 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_sat_leo_downlink
   127.0.0.1 quic const25:uniform500,1000:const25:uniform500,1000 plain 8)

# 196. 5G Non-Terrestrial Network (NTN) Beam Handover over IPv6 (15 fps, 400 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_5g_ntn_handover_v6
   ::1       quic uniform12,18:truncnormal400,30:uniform12,18:truncnormal400,30 plain 8)

# 197. Federated Learning Edge Gradient Updates with Segmentation (5 fps, 3600 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_ai_federated_gradient_frag
   127.0.0.1 quic const5:truncnormal3600,250:const5:truncnormal3600,250:maxmsgsize=1200 plain 8)

# 198. Autonomous Drone Swarm Mesh Mission Control over IPv6 (30 fps, 250 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_uav_swarm_mission_ctrl_v6
   ::1       quic uniform25,35:uniform180,320:uniform25,35:uniform180,320 plain 8)

# 199. Space Tracking Radar Target Plot Vector Stream (50 fps, 400 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_deepspace_radar_plots
   127.0.0.1 quic truncnormal50,3:uniform300,500:truncnormal50,3:uniform300,500 plain 8)

# 200. Kubernetes Edge Mesh Mutual-TLS QUIC Heartbeat (40 fps, 64 B, const)
ADD_NETPERFMETER_FLOW_TEST(profile_quic_cloud_service_mesh_hb
   127.0.0.1 quic const40:const64:const40:const64 plain 8)

ENDIF()
