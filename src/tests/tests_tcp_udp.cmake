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
#### FLOW INTEGRATION TESTS (TCP & UDP)                                  ####
#############################################################################

# ====== 1. Bidirectional Flows =============================================
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_bidi_ipv4 127.0.0.1 tcp const10:const1000:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_bidi_ipv6 ::1       tcp const10:const1000:const10:const1000 plain)

ADD_NETPERFMETER_FLOW_TEST(flow_udp_bidi_ipv4 127.0.0.1 udp const10:const1000:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_bidi_ipv6 ::1       udp const10:const1000:const10:const1000 plain)

# ====== 2. Upload-Only Flows (Active -> Passive) ===========================
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_upload_ipv4 127.0.0.1 tcp const10:const1000:const0:const0 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_upload_ipv6 ::1       tcp const10:const1000:const0:const0 plain)

ADD_NETPERFMETER_FLOW_TEST(flow_udp_upload_ipv4 127.0.0.1 udp const10:const1000:const0:const0 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_upload_ipv6 ::1       udp const10:const1000:const0:const0 plain)

# ====== 3. Download-Only Flows (Passive -> Active) =========================
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_download_ipv4 127.0.0.1 tcp const0:const0:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_download_ipv6 ::1       tcp const0:const0:const10:const1000 plain)

ADD_NETPERFMETER_FLOW_TEST(flow_udp_download_ipv4 127.0.0.1 udp const0:const0:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_download_ipv6 ::1       udp const0:const0:const10:const1000 plain)

# ====== 4. Default Flow Shorthand ("default") ==============================
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_default_shorthand
   127.0.0.1 tcp default plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_default_shorthand
   127.0.0.1 udp default:description=DefaultUDP plain 2 --expect-config "DefaultUDP")

# ====== 5. Compression Formats Matrix ======================================
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_gz  127.0.0.1 tcp const10:const1000:const10:const1000 gz)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_bz2 127.0.0.1 tcp const10:const1000:const10:const1000 bz2)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_xz  127.0.0.1 tcp const10:const1000:const10:const1000 xz)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_zst 127.0.0.1 tcp const10:const1000:const10:const1000 zst)

# ====== 6. Output Combinations (Without Scalars and/or Vectors) ============
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_no_scalar  127.0.0.1 tcp const10:const1000:const10:const1000 plain 2 --no-scalar)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_no_vector  127.0.0.1 tcp const10:const1000:const10:const1000 plain 2 --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_no_outputs 127.0.0.1 tcp const10:const1000:const10:const1000 plain 2 --no-scalar --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_no_outputs 127.0.0.1 udp const10:const1000:const10:const1000 plain 2 --no-scalar --no-vector)

# ====== 7. Framing and Defragmentation (maxmsgsize & defragtimeout) ========
# --- TCP Framing Tests ---
# Small frames (1,000 B) segmented into 250 B messages (4 msgs/frame)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_framing_small      127.0.0.1 tcp const10:const1000:const10:const1000:maxmsgsize=250  plain 2)
# Medium frames (4,000 B) segmented into 1,000 B messages (4 msgs/frame)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_framing_medium     127.0.0.1 tcp const10:const4000:const10:const4000:maxmsgsize=1000 plain 2)
# Large frames (8,000 B) segmented into 1,400 B messages (6 msgs/frame)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_framing_large      127.0.0.1 tcp const10:const8000:const10:const8000:maxmsgsize=1400 plain 2)
# Asymmetric bidirectional frame sizes (out: 4,000 B, in: 1,000 B, maxmsgsize: 500 B)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_framing_asymmetric 127.0.0.1 tcp const10:const4000:const10:const1000:maxmsgsize=500  plain 2)

# --- UDP Framing & Defragment Timeout Tests ---
# Small frames (1,000 B) segmented into 250 B messages
ADD_NETPERFMETER_FLOW_TEST(flow_udp_framing_small      127.0.0.1 udp const10:const1000:const10:const1000:maxmsgsize=250  plain 2)
# Medium frames (4,000 B) segmented into 1,000 B messages
ADD_NETPERFMETER_FLOW_TEST(flow_udp_framing_medium     127.0.0.1 udp const10:const4000:const10:const4000:maxmsgsize=1000 plain 2)
# Large frames (8,000 B) segmented into 1,400 B messages
ADD_NETPERFMETER_FLOW_TEST(flow_udp_framing_large      127.0.0.1 udp const10:const8000:const10:const8000:maxmsgsize=1400 plain 2)
# Asymmetric bidirectional frame sizes (out: 4,000 B, in: 1,000 B, maxmsgsize: 500 B)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_framing_asymmetric 127.0.0.1 udp const10:const4000:const10:const1000:maxmsgsize=500  plain 2)
# Explicit defragtimeout option on segmented UDP flow
ADD_NETPERFMETER_FLOW_TEST(flow_udp_defragtimeout      127.0.0.1 udp const10:const4000:const10:const4000:maxmsgsize=1000:defragtimeout=2500 plain 2)

# ====== 8. CLI Flags & Per-Flow Option Overrides ===========================
# Local data & control endpoint binding (-L / -l) over IPv4, IPv6, comma-separated list, and UDP
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_local_bind_ipv4
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 2
   -L 127.0.0.1 -l 127.0.0.1 --server-arg -L --server-arg 127.0.0.1 --server-arg -l --server-arg 127.0.0.1)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_local_bind_ipv6
   ::1 tcp const10:const1000:const10:const1000 plain 2
   -L ::1 -l ::1 --server-arg -L --server-arg ::1 --server-arg -l --server-arg ::1)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_multi_local_bind_ipv4
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 2
   -L 127.0.0.1,::1 -l 127.0.0.1,::1
   --server-arg -L --server-arg 127.0.0.1,::1
   --server-arg -l --server-arg 127.0.0.1,::1)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_local_bind_ipv4
   127.0.0.1 udp const10:const1000:const10:const1000 plain 2
   -L 127.0.0.1 -l 127.0.0.1 --server-arg -L --server-arg 127.0.0.1 --server-arg -l --server-arg 127.0.0.1)

# IPv6-only mode (-6 / --v6only) on CLI and per-flow v6only option
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_v6only_cli
   ::1 tcp const10:const1000:const10:const1000 plain 2
   -6 --server-arg -6)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_v6only_perflow
   ::1 tcp const10:const1000:const10:const1000:v6only plain 2)

# Flow multiplier (-N / --count)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_count_multiplier
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 3
   -N 3)

# Global (-o / -i) and per-flow (sndbuf= / rcvbuf=) socket buffer sizes
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_global_buffers
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 2
   -o 65536 -i 65536)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_perflow_buffers
   127.0.0.1 tcp const10:const1000:const10:const1000:sndbuf=65536:rcvbuf=65536 plain 2)

# Custom flow id=, description=, congestion control cc=, nodelay=off, and debug=on/off
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_custom_id_description
   127.0.0.1 tcp const10:const1000:const10:const1000:id=42:description=CustomFlow42 plain 2
   --expect-config "CustomFlow42")
IF (HAVE_TCP_CONGESTION)
   ADD_NETPERFMETER_FLOW_TEST(flow_tcp_cc_cubic
      127.0.0.1 tcp const10:const1000:const10:const1000:cc=cubic plain 2)
ENDIF()
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_nodelay_off
   127.0.0.1 tcp const10:const1000:const10:const1000:nodelay=off plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_debug_on_off
   127.0.0.1 tcp const10:const1000:const10:const1000:debug=on plain 2
   -tcp const10:const1000:const10:const1000:debug=off)

# Node names (-A / -P), control heartbeats, logging levels/colors/files, and display flags
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_node_names_and_heartbeats
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 2
   -A CustomClientNode --server-arg -P --server-arg CustomServerNode
   --control-hbinterval 1000 --control-hbpathmaxrtx 2 --control-hbassocmaxrtx 4
   --server-arg --control-hbinterval --server-arg 1000
   --server-arg --control-hbpathmaxrtx --server-arg 2
   --server-arg --control-hbassocmaxrtx --server-arg 4
   --expect-config "CustomClientNode")
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_quiet_nodisplay_nocolor
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 2
   -q --logcolor off --nodisplay --server-arg -q --server-arg --logcolor --server-arg off --server-arg --nodisplay)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_verbose_display_color
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 2
   -! --logcolor on --display --server-arg -! --server-arg --logcolor --server-arg on --server-arg --display)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_logfile_redirect
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 2 --verify-logfile)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_logappend_redirect
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 2 --verify-logappend)

# ====== 9. Traffic Class (DSCP/TOS), IPv6 Flow Label & Per-Flow Keep-Alives =
# Per-flow Traffic Class (DSCP name, hex/decimal with ECN-bit masking) and control TC/FL
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_tc_dscp_ef_ipv4
   127.0.0.1 tcp const10:const1000:const10:const1000:tc=EF plain 2
   --expect-config "FLOW0_TRAFFIC_CLASS=\"0xb8\"")
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_tc_dscp_af21_ipv6
   ::1 tcp const10:const1000:const10:const1000:tc=AF21 plain 2
   --expect-config "FLOW0_TRAFFIC_CLASS_NAME=\"AF21\"")
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_tc_ecn_bits_masked_ipv4
   127.0.0.1 tcp const10:const1000:const10:const1000:tc=186 plain 2
   --expect-config "FLOW0_TRAFFIC_CLASS=\"0xb8\"")
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_control_tc_and_fl_ipv6
   ::1 tcp const10:const1000:const10:const1000:tc=AF31:fl=auto plain 2
   -Q CS6 -F auto --server-arg -Q --server-arg CS6
   --expect-config "CONTROL_TRAFFIC_CLASS=\"0xc0\"")

# IPv6 Flow Label modes (explicit 20-bit label, explicit 0, reflect)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_fl_explicit_ipv6
   ::1 tcp const10:const1000:const10:const1000:fl=0x12345 plain 2
   --expect-config "FLOW0_FLOW_LABEL=\"0x12345\"")
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_fl_zero_ipv6
   ::1 tcp const10:const1000:const10:const1000:fl=0 plain 2
   --expect-config "FLOW0_FLOW_LABEL=\"0x00000\"")
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_fl_reflect_ipv6
   ::1 tcp const10:const1000:const10:const1000:fl=reflect plain 2
   --expect-config "FLOW0_FLOW_LABEL=\"reflect\"")

# Per-flow TCP keep-alive enabled and explicitly disabled (hbinterval=0)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_perflow_keepalive_ipv4
   127.0.0.1 tcp const10:const1000:const10:const1000:hbinterval=5000:hbpathmaxrtx=4 plain 2
   --expect-config "FLOW0_HBINTERVAL=\"5000\"")
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_perflow_keepalive_disabled_ipv4
   127.0.0.1 tcp const10:const1000:const10:const1000:hbinterval=0 plain 2
   --expect-config "FLOW0_HBINTERVAL=\"0\"")

# Single and concurrent UDP flows with distinct Traffic Classes and Flow Labels (passive sendmsg() cmsg)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_tc_dscp_ef_ipv4
   127.0.0.1 udp const10:const1000:const10:const1000:tc=EF plain 2
   --expect-config "FLOW0_TRAFFIC_CLASS=\"0xb8\"")
ADD_NETPERFMETER_FLOW_TEST(flow_udp_tc_and_fl_ipv6
   ::1 udp const10:const1000:const10:const1000:tc=AF41:fl=0x00100 plain 2
   --expect-config "FLOW0_FLOW_LABEL=\"0x00100\"")
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_different_tc_ipv4
   127.0.0.1 udp const10:const1000:const10:const1000:tc=EF plain 3
   -udp const10:const1000:const10:const1000:tc=AF11
   --expect-config "FLOW1_TRAFFIC_CLASS=\"0x28\"")
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_different_tc_fl_ipv6
   ::1 udp const10:const1000:const10:const1000:tc=EF:fl=0x00100 plain 3
   -udp const10:const1000:const10:const1000:tc=AF21:fl=0x00200
   --expect-config "FLOW1_FLOW_LABEL=\"0x00200\"")


#############################################################################
#### RANDOM DISTRIBUTION TESTS (VBR FLOWS - TCP & UDP)                   ####
#############################################################################

# 1. Uniform Distribution (Rate [15, 25) -> HM ~19.6, Size [800, 1200) -> AM 1000)
ADD_NETPERFMETER_FLOW_TEST(dist_tcp_uniform 127.0.0.1 tcp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_udp_uniform 127.0.0.1 udp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)

# 2. Exponential Frame Sizes with Constant Rate (Rate 20 frames/s, Exp Size mean 1000 B)
ADD_NETPERFMETER_FLOW_TEST(dist_tcp_exp     127.0.0.1 tcp const20:exp1000:const20:exp1000 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_udp_exp     127.0.0.1 udp const20:exp1000:const20:exp1000 plain 8)

# 3. Normal Distribution (Rate mean 20, stddev 2; Size mean 1000, stddev 100)
ADD_NETPERFMETER_FLOW_TEST(dist_tcp_normal  127.0.0.1 tcp normal20,2:normal1000,100:normal20,2:normal1000,100 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_udp_normal  127.0.0.1 udp normal20,2:normal1000,100:normal20,2:normal1000,100 plain 8)

# 4. Truncated Normal Distribution (Rate mean 20, stddev 2; Size mean 1000, stddev 100)
ADD_NETPERFMETER_FLOW_TEST(dist_tcp_truncnormal 127.0.0.1 tcp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_udp_truncnormal 127.0.0.1 udp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)

# 5. Pareto Distribution with Finite Variance (Shape alpha=3.0 -> HM = (4/3)*k = 20 for k=15; Size k=666.7 -> AM 1000)
ADD_NETPERFMETER_FLOW_TEST(dist_tcp_pareto  127.0.0.1 tcp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_udp_pareto  127.0.0.1 udp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)


#############################################################################
#### ON-OFF FLOW SCHEDULE TESTS (TCP & UDP)                              ####
#############################################################################

# ====== 1. Single On-Off Burst (Off 0..1s, On 1..3s, Off 3..5s) ============
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_single_burst_ipv4
   127.0.0.1 tcp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_single_burst_ipv6
   ::1       tcp const20:const1000:const0:const0:onoff=1,3 plain 5)

ADD_NETPERFMETER_FLOW_TEST(flow_udp_onoff_single_burst_ipv4
   127.0.0.1 udp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_onoff_single_burst_ipv6
   ::1       udp const20:const1000:const0:const0:onoff=1,3 plain 5)

# ====== 2. Multi-Burst On-Off (Alternating On/Off: 1..2s On, 3..4s On) =====
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_multiburst_ipv4
   127.0.0.1 tcp const20:const1000:const0:const0:onoff=1,2,3,4 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_onoff_multiburst_ipv4
   127.0.0.1 udp const20:const1000:const0:const0:onoff=1,2,3,4 plain 5)

# ====== 3. Delayed Start (Off 0..2s, On 2..5s until test completion) ======
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_delayed_start
   127.0.0.1 tcp const20:const1000:const0:const0:onoff=2 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_onoff_delayed_start
   127.0.0.1 udp const20:const1000:const0:const0:onoff=2 plain 5)

# ====== 4. On-Off Combined with Message Segmentation =======================
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_fragmented
   127.0.0.1 tcp const20:const3000:const0:const0:onoff=1,3:maxmsgsize=1000 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_onoff_fragmented
   127.0.0.1 udp const20:const3000:const0:const0:onoff=1,3:maxmsgsize=1000 plain 5)

# ====== 5. On-Off Combined with Stochastic Distribution (Exp Size) =========
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_exp_size
   127.0.0.1 tcp const20:exp1000:const0:const0:onoff=1,4 plain 6)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_onoff_exp_size
   127.0.0.1 udp const20:exp1000:const0:const0:onoff=1,4 plain 6)

# ====== 6. Repeating, Stochastic & Inbound On-Off Schedules ================
# Repeating relative cycle (1s OFF, 1s ON, repeat -> 3.0s active across 6s)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_repeat
   127.0.0.1 tcp const20:const1000:const0:const0:onoff=+1,+1,repeat plain 6)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_onoff_repeat
   127.0.0.1 udp const20:const1000:const0:const0:onoff=+1,+1,repeat plain 6)

# Random uniform durations inside repeating on-off schedule
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_random_repeat
   127.0.0.1 tcp const20:const1000:const0:const0:onoff=+uniform0.5,1.0,+uniform0.5,1.0,repeat plain 8)

# Inbound On-Off (passive -> active burst) & Bidirectional On-Off
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_inbound
   127.0.0.1 tcp const0:const0:const20:const1000:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_bidirectional
   127.0.0.1 tcp const20:const1000:const20:const1000:onoff=1,3 plain 5)


#############################################################################
#### MULTI-CONNECTION & CONCURRENT FLOW TESTS (TCP & UDP)                ####
#############################################################################

# ====== 1. Homogeneous Multi-Connection Tests (TCP) =======================
# 2 concurrent TCP connections over IPv4 (2x 10 fps, 1000 B)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_2connections_ipv4
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000)

# 4 concurrent TCP connections over IPv4 (4x 10 fps, 1000 B) - 5s for scheduler stability
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_4connections_ipv4
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 5
   -tcp const10:const1000:const10:const1000
   -tcp const10:const1000:const10:const1000
   -tcp const10:const1000:const10:const1000)

# 2 concurrent TCP connections over IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_2connections_ipv6
   ::1       tcp const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000)

# 4 concurrent TCP connections over IPv6 (4x 10 fps, 1000 B) - 5s for scheduler stability
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_4connections_ipv6
   ::1       tcp const10:const1000:const10:const1000 plain 5
   -tcp const10:const1000:const10:const1000
   -tcp const10:const1000:const10:const1000
   -tcp const10:const1000:const10:const1000)

# 2 competing TCP connections: interactive small frames (25 fps, 100 B, nodelay) vs bulk frames (10 fps, 1400 B)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_heterogeneous_rates_sizes
   127.0.0.1 tcp const25:const100:const25:const100:nodelay=on plain 5
   -tcp const10:const1400:const10:const1400)


# ====== 2. Homogeneous Multi-Connection Tests (UDP) =======================
# 2 concurrent UDP flows to verify clean passive server shutdown on SIGINT
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_2flows_shutdown_clean
   127.0.0.1 udp const5:const1000:const2:const500 plain 3
   -udp const20:const1000:const20:const500)

# 2 concurrent UDP flows over IPv4 (2x 10 fps, 1000 B)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_2flows_ipv4
   127.0.0.1 udp const10:const1000:const10:const1000 plain 3
   -udp const10:const1000:const10:const1000)

# 4 concurrent UDP flows over IPv4 (4x 10 fps, 1000 B) - 5s for scheduler stability
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_4flows_ipv4
   127.0.0.1 udp const10:const1000:const10:const1000 plain 5
   -udp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)

# 2 concurrent UDP flows over IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_2flows_ipv6
   ::1       udp const10:const1000:const10:const1000 plain 3
   -udp const10:const1000:const10:const1000)

# 4 concurrent UDP flows over IPv6 (4x 10 fps, 1000 B) - 5s for scheduler stability
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_4flows_ipv6
   ::1       udp const10:const1000:const10:const1000 plain 5
   -udp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)

# 4 UDP flows with distinct asymmetric frame rates and sizes - 5s for multi-flow stability
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_4flows_heterogeneous
   127.0.0.1 udp const40:const150:const40:const150 plain 5
   -udp const20:const500:const20:const500
   -udp const10:const1200:const10:const1200
   -udp const5:const1400:const5:const1400)


# ====== 3. Heterogeneous Cross-Protocol Concurrent Connections =============
# Concurrent TCP + UDP competing flows (1 TCP connection + 1 UDP flow)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_udp_cross_traffic_ipv4
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 3
   -udp const10:const1000:const10:const1000)

# Concurrent TCP + UDP competing flows over IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_udp_cross_traffic_ipv6
   ::1       tcp const10:const1000:const10:const1000 plain 3
   -udp const10:const1000:const10:const1000)

# 4-flow hybrid session: 2 concurrent TCP connections + 2 concurrent UDP flows - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_udp_4flows_cross
   127.0.0.1 tcp const10:const1000:const10:const1000 plain 5
   -tcp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)


# ====== 4. Asymmetric Split Concurrency (Upload + Download on Separate Sockets)
# Flow 0: Upload-only TCP, Flow 1: Download-only TCP running simultaneously
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_split_upload_download
   127.0.0.1 tcp const15:const1000:const0:const0 plain 3
   -tcp const0:const0:const15:const1000)

# Flow 0: Upload-only UDP, Flow 1: Download-only UDP running simultaneously
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_split_upload_download
   127.0.0.1 udp const15:const1000:const0:const0 plain 3
   -udp const0:const0:const15:const1000)

# Cross-protocol split: Flow 0 upload-only TCP competing with Flow 1 download-only UDP
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_upload_udp_download_split
   127.0.0.1 tcp const15:const1000:const0:const0 plain 3
   -udp const0:const0:const15:const1000)


# ====== 5. Multi-Connection Framing & Segmentation Tests ===================
# 2 concurrent TCP connections with message segmentation (maxmsgsize=250)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_2connections_fragmented
   127.0.0.1 tcp const10:const1000:const10:const1000:maxmsgsize=250 plain 3
   -tcp const10:const1000:const10:const1000:maxmsgsize=250)

# 2 concurrent UDP flows with datagram segmentation (maxmsgsize=250)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_2flows_fragmented
   127.0.0.1 udp const10:const1000:const10:const1000:maxmsgsize=250 plain 3
   -udp const10:const1000:const10:const1000:maxmsgsize=250)


# ====== 6. Interleaved & Staggered On-Off Dynamic Concurrency =============
# 2 TCP connections with alternating bursts
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tcp_interleaved_onoff
   127.0.0.1 tcp const20:const1000:const0:const0:onoff=1,2 plain 4
   -tcp const20:const1000:const0:const0:onoff=2,3)

# 2 UDP flows with staggered overlapping bursts
ADD_NETPERFMETER_FLOW_TEST(flow_multi_udp_staggered_onoff
   127.0.0.1 udp const20:const1000:const0:const0:onoff=1,3 plain 5
   -udp const20:const1000:const0:const0:onoff=2,4)
