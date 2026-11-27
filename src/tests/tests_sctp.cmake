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
#### SCTP FLOW INTEGRATION TESTS                                         ####
#############################################################################

IF (HAVE_SCTP_FOR_TESTS)

# ====== 1. Bidirectional Flows =============================================
# Symmetrical constant rate and size (10 fps, 1000 B) over IPv4 and IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_bidi_ipv4 127.0.0.1 sctp const10:const1000:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_bidi_ipv6 ::1       sctp const10:const1000:const10:const1000 plain)

# ====== 2. Upload-Only Flows (Active -> Passive) ===========================
# Unidirectional sender traffic (10 fps, 1000 B, no inbound)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_upload_ipv4 127.0.0.1 sctp const10:const1000:const0:const0 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_upload_ipv6 ::1       sctp const10:const1000:const0:const0 plain)

# ====== 3. Download-Only Flows (Passive -> Active) =========================
# Unidirectional receiver traffic (no outbound, 10 fps, 1000 B inbound)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_download_ipv4 127.0.0.1 sctp const0:const0:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_download_ipv6 ::1       sctp const0:const0:const10:const1000 plain)

# ====== 4. Default Shorthand, Nagle, Local Bind & Control Channel ==========
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_default_shorthand
   127.0.0.1 sctp default plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_nodelay_on
   127.0.0.1 sctp const10:const1000:const10:const1000:nodelay=on plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_local_bind_ipv4
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 2
   -L 127.0.0.1 -l 127.0.0.1
   --server-arg -L --server-arg 127.0.0.1 --server-arg -l --server-arg 127.0.0.1)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_local_bind_ipv6
   ::1 sctp const10:const1000:const10:const1000 plain 2
   -L ::1 -l ::1
   --server-arg -L --server-arg ::1 --server-arg -l --server-arg ::1)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multihomed_local_bind
   ::1 sctp const10:const1000:const10:const1000 plain 2
   -L ::1,127.0.0.1 -l ::1,127.0.0.1 --control-over-sctp
   --server-arg -L --server-arg ::1,127.0.0.1 --server-arg -l --server-arg ::1,127.0.0.1)

# Control channel using SCTP with SCTP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_ctrl_sctp_bidi_ipv4 127.0.0.1 sctp const10:const1000:const10:const1000 plain 2 --control-over-sctp)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_ctrl_sctp_bidi_ipv6 ::1       sctp const10:const1000:const10:const1000 plain 2 --control-over-sctp)

# Control channel using SCTP with TCP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_ctrl_sctp_bidi_ipv4  127.0.0.1 tcp  const10:const1000:const10:const1000 plain 2 --control-over-sctp)

# Control channel using SCTP with UDP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_udp_ctrl_sctp_bidi_ipv4  127.0.0.1 udp  const10:const1000:const10:const1000 plain 2 --control-over-sctp)

IF (HAVE_MPTCP_FOR_TESTS)
   # Control channel using SCTP with MPTCP data flow
   ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_ctrl_sctp_bidi_ipv4 127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2 --control-over-sctp)

   # Control channel using MPTCP with SCTP data flow
   ADD_NETPERFMETER_FLOW_TEST(flow_sctp_ctrl_mptcp_bidi_ipv4 127.0.0.1 sctp  const10:const1000:const10:const1000 plain 2 --control-over-mptcp)

   # Quad-protocol concurrency: MPTCP + SCTP + TCP + UDP - 5s
   ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_sctp_tcp_udp_cross
      127.0.0.1 mptcp const10:const1000:const10:const1000 plain 5
      -sctp const10:const1000:const10:const1000
      -tcp  const10:const1000:const10:const1000
      -udp  const10:const1000:const10:const1000)
ENDIF()

# ====== 5. Compression Formats Matrix ======================================
# Verify scalar/vector compressed stream logging (gzip, bzip2, xz, zstd)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_format_gz  127.0.0.1 sctp const10:const1000:const10:const1000 gz)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_format_bz2 127.0.0.1 sctp const10:const1000:const10:const1000 bz2)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_format_xz  127.0.0.1 sctp const10:const1000:const10:const1000 xz)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_format_zst 127.0.0.1 sctp const10:const1000:const10:const1000 zst)

# ====== 6. Output Combinations (Without Scalars and/or Vectors) ============
# Verify CLI suppression flags (--no-scalar, --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_no_scalar  127.0.0.1 sctp const10:const1000:const10:const1000 plain 2 --no-scalar)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_no_vector  127.0.0.1 sctp const10:const1000:const10:const1000 plain 2 --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_no_outputs 127.0.0.1 sctp const10:const1000:const10:const1000 plain 2 --no-scalar --no-vector)

# ====== 7. Framing and Defragmentation (maxmsgsize & frame sizes) ==========
# Small frames (1,000 B) segmented into 250 B chunks (4 chunks/frame)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_framing_small      127.0.0.1 sctp const10:const1000:const10:const1000:maxmsgsize=250  plain 2)
# Medium frames (4,000 B) segmented into 1,000 B chunks (4 chunks/frame)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_framing_medium     127.0.0.1 sctp const10:const4000:const10:const4000:maxmsgsize=1000 plain 2)
# Large frames (8,000 B) segmented into 1,400 B chunks (6 chunks/frame)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_framing_large      127.0.0.1 sctp const10:const8000:const10:const8000:maxmsgsize=1400 plain 2)
# Asymmetric bidirectional frame sizes (out: 4,000 B, in: 1,000 B, maxmsgsize: 500 B)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_framing_asymmetric 127.0.0.1 sctp const10:const4000:const10:const1000:maxmsgsize=500  plain 2)

# ====== 8. On-Off Flow Schedule Tests (onoff=t1,t2,...) ====================
# Single On-Off Burst (Off 0..1s, On 1..3s, Off 3..5s)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_onoff_single_burst_ipv4    127.0.0.1 sctp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_onoff_single_burst_ipv6    ::1       sctp const20:const1000:const0:const0:onoff=1,3 plain 5)
# Multi-Burst On-Off (Alternating On/Off: 1..2s On, 3..4s On)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_onoff_multiburst_ipv4      127.0.0.1 sctp const20:const1000:const0:const0:onoff=1,2,3,4 plain 5)
# Delayed Start (Off 0..2s, On 2..5s until test completion)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_onoff_delayed_start        127.0.0.1 sctp const20:const1000:const0:const0:onoff=2 plain 5)
# On-Off Combined with Message Segmentation
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_onoff_fragmented           127.0.0.1 sctp const20:const3000:const0:const0:onoff=1,3:maxmsgsize=1000 plain 5)
# On-Off Combined with Exponential Distribution (Mean 1000 B, active 1..4s)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_onoff_exp_size             127.0.0.1 sctp const20:exp1000:const0:const0:onoff=1,4 plain 6)

# ====== 9. Basic Stochastic Distribution Tests (VBR Flows) =================
# 1. Uniform Distribution (Rate [15, 25) -> HM ~19.6, Size [800, 1200) -> AM 1000)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_uniform      127.0.0.1 sctp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_uniform_ipv6 ::1       sctp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)

# 2. Exponential Frame Sizes with Constant Rate (Rate 20 frames/s, Exp Size mean 1000 B)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_exp          127.0.0.1 sctp const20:exp1000:const20:exp1000 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_exp_ipv6     ::1       sctp const20:exp1000:const20:exp1000 plain 8)

# 3. Truncated Normal Distribution (Rate mean 20, stddev 2; Size mean 1000, stddev 100)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_truncnormal      127.0.0.1 sctp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_truncnormal_ipv6 ::1       sctp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)

# 4. Pareto Distribution with Finite Variance (Shape alpha=3.0 -> HM = (4/3)*k = 20 for k=15; Size k=666.7 -> AM 1000)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_pareto      127.0.0.1 sctp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_pareto_ipv6 ::1       sctp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)

# ====== 10. Control-Over-SCTP with Stochastic Data Distributions ===========
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_ctrl_sctp_uniform
   127.0.0.1 sctp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8 --control-over-sctp)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_ctrl_sctp_exp
   127.0.0.1 sctp const20:exp1000:const20:exp1000 plain 8 --control-over-sctp)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_ctrl_sctp_truncnormal
   127.0.0.1 sctp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8 --control-over-sctp)
ADD_NETPERFMETER_FLOW_TEST(dist_sctp_ctrl_sctp_pareto
   127.0.0.1 sctp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8 --control-over-sctp)


# ====== 11. SCTP Multi-Connection & Multi-Stream Integration Tests ==========
# 2 concurrent SCTP flows over IPv4 (2x 10 fps, 1000 B)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_sctp_2flows_ipv4
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 3
   -sctp const10:const1000:const10:const1000)

# 4 concurrent SCTP flows over IPv4 (4x 10 fps, 1000 B) - 5s for scheduler stability
ADD_NETPERFMETER_FLOW_TEST(flow_multi_sctp_4flows_ipv4
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 5
   -sctp const10:const1000:const10:const1000
   -sctp const10:const1000:const10:const1000
   -sctp const10:const1000:const10:const1000)

# 2 concurrent SCTP flows over IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_multi_sctp_2flows_ipv6
   ::1       sctp const10:const1000:const10:const1000 plain 3
   -sctp const10:const1000:const10:const1000)

# Concurrent SCTP + TCP heterogeneous flows
ADD_NETPERFMETER_FLOW_TEST(flow_multi_sctp_tcp_cross_traffic
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000)

# Concurrent SCTP + UDP heterogeneous flows
ADD_NETPERFMETER_FLOW_TEST(flow_multi_sctp_udp_cross_traffic
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 3
   -udp const10:const1000:const10:const1000)

# Tri-Protocol Concurrency: SCTP + TCP + UDP all running concurrently in 1 session - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_multi_tri_protocol_concurrent
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 5
   -tcp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)

# 4-protocol concurrent stress session: 1 SCTP association (2 streams) + 1 TCP conn + 1 UDP flow - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_multi_quad_concurrent_stress
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 5
   --stream const10:const500:const10:const500
   -tcp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)

# 2 concurrent SCTP flows with control channel over SCTP
ADD_NETPERFMETER_FLOW_TEST(flow_multi_sctp_2flows_ctrl_sctp
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 3
   -sctp const10:const1000:const10:const1000 --control-over-sctp)

# 2 concurrent SCTP flows with message chunking (maxmsgsize=250)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_sctp_2flows_fragmented
   127.0.0.1 sctp const10:const1000:const10:const1000:maxmsgsize=250 plain 3
   -sctp const10:const1000:const10:const1000:maxmsgsize=250)


# ====== 12. Partial Reliability (PR-SCTP, RFC 3758) =========================
# Fully unreliable delivery (unreliable=1.0) over IPv4 - 5s for statistical smoothing
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_pr_unreliable_ipv4
   127.0.0.1 sctp const20:const1000:const20:const1000:unreliable=1.0 plain 5)

# Fully unreliable delivery over IPv6 - 5s for statistical smoothing
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_pr_unreliable_ipv6
   ::1       sctp const20:const1000:const20:const1000:unreliable=1.0 plain 5)

# 50% partial reliability (unreliable=0.5: 50% reliable, 50% unreliable) - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_pr_partial_50percent
   127.0.0.1 sctp const20:const1000:const20:const1000:unreliable=0.5 plain 5)

# PR-SCTP with limited retransmission trials (rtx_trials=0: drop upon first loss) - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_pr_rtx_trials_zero
   127.0.0.1 sctp const20:const1000:const20:const1000:unreliable=1.0:rtx_trials=0 plain 5)

# PR-SCTP with up to 3 retransmission trials (rtx_trials=3) - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_pr_rtx_trials_three
   127.0.0.1 sctp const20:const1000:const20:const1000:unreliable=1.0:rtx_trials=3 plain 5)

# PR-SCTP with timed lifetime / buffer expiration (rtx_timeout=200ms) - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_pr_rtx_timeout_timed
   127.0.0.1 sctp const20:const1000:const20:const1000:unreliable=1.0:rtx_timeout=200 plain 5)

# PR-SCTP unreliable flow combined with message segmentation - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_pr_unreliable_fragmented
   127.0.0.1 sctp const20:const3000:const20:const3000:unreliable=1.0:maxmsgsize=1000 plain 5)


# ====== 13. Ordered vs. Unordered Delivery (RFC 9260) =======================
# Fully unordered delivery (unordered=1.0) over IPv4
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_unordered_delivery_ipv4
   127.0.0.1 sctp const20:const1000:const20:const1000:unordered=1.0 plain 3)

# Fully unordered delivery over IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_unordered_delivery_ipv6
   ::1       sctp const20:const1000:const20:const1000:unordered=1.0 plain 3)

# Explicit 100% ordered delivery (ordered=1.0)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_ordered_explicit
   127.0.0.1 sctp const20:const1000:const20:const1000:ordered=1.0 plain 3)

# Mixed 50% ordered / 50% unordered delivery (unordered=0.5)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_mixed_ordering_50percent
   127.0.0.1 sctp const20:const1000:const20:const1000:unordered=0.5 plain 3)

# Unordered delivery with message segmentation
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_unordered_fragmented
   127.0.0.1 sctp const20:const3000:const20:const3000:unordered=1.0:maxmsgsize=1000 plain 3)

# Unordered + Unreliable combined (RFC 9260 unordered + RFC 3758 PR-SCTP) - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_unordered_unreliable_pr
   127.0.0.1 sctp const20:const1000:const20:const1000:unordered=1.0:unreliable=1.0:rtx_trials=0 plain 5)


# ====== 14. SCTP Multi-Streaming (Single Association, Multiple Streams) =====
# 2 streams within 1 association: Stream 0 (10 fps, 1000 B) and Stream 1 (20 fps, 500 B)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_2streams_ipv4
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 3
   --stream const20:const500:const20:const500)

# 2 streams within 1 association over IPv6
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_2streams_ipv6
   ::1       sctp const10:const1000:const10:const1000 plain 3
   --stream const20:const500:const20:const500)

# 4 streams within 1 association (5, 10, 15, and 20 fps) - 5s for multi-stream stability
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_4streams
   127.0.0.1 sctp const5:const500:const5:const500 plain 5
   --stream const10:const500:const10:const500
   --stream const15:const500:const15:const500
   --stream const20:const500:const20:const500)

# Asymmetric multi-stream: Stream 0 upload-only, Stream 1 download-only in 1 association
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_asymmetric_split
   127.0.0.1 sctp const15:const1000:const0:const0 plain 3
   --stream const0:const0:const15:const1000)

# Multi-stream combining ordered and unordered streams in 1 association
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_ordered_and_unordered
   127.0.0.1 sctp const10:const1000:const10:const1000:ordered=1.0 plain 3
   --stream const20:const500:const20:const500:unordered=1.0)

# Multi-stream combining reliable and unreliable streams in 1 association
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_reliable_and_unreliable
   127.0.0.1 sctp const10:const1000:const10:const1000:reliable=1.0 plain 3
   --stream const20:const500:const20:const500:unreliable=1.0:rtx_trials=0)

# 4 streams in 1 association combining all 4 RFC permutations of reliability and ordering - 5s
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_all_rfc_permutations
   127.0.0.1 sctp const10:const500:const10:const500:reliable=1.0:ordered=1.0 plain 5
   --stream const10:const500:const10:const500:unreliable=1.0:rtx_trials=3:ordered=1.0
   --stream const10:const500:const10:const500:reliable=1.0:unordered=1.0
   --stream const10:const500:const10:const500:unreliable=1.0:rtx_trials=0:unordered=1.0)

# 2 streams in 1 association with heterogeneous frame sizes and segmentation
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_mixed_message_sizes
   127.0.0.1 sctp const20:const250:const20:const250 plain 3
   --stream const10:const2000:const10:const2000:maxmsgsize=500)

# 2 streams in 1 association with staggered On-Off schedules
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_staggered_onoff
   127.0.0.1 sctp const20:const1000:const0:const0:onoff=1,3 plain 5
   --stream const20:const1000:const0:const0:onoff=2,4)

# 2 streams within 1 association with control channel over SCTP
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_ctrl_sctp
   127.0.0.1 sctp const10:const1000:const10:const1000 plain 3
   --stream const20:const500:const20:const500 --control-over-sctp)

# 2 streams within 1 association with message chunking (maxmsgsize=500)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_fragmented
   127.0.0.1 sctp const10:const2000:const10:const2000:maxmsgsize=500 plain 3
   --stream const10:const2000:const10:const2000:maxmsgsize=500)

# Traffic Class, IPv6 Flow Label, and per-flow SCTP heartbeats (enabled, disabled, multi-stream matching)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_tc_and_heartbeats_ipv4
   127.0.0.1 sctp const10:const1000:const10:const1000:tc=EF:hbinterval=5000:hbpathmaxrtx=3:hbassocmaxrtx=6 plain 2
   --control-over-sctp -Q CS6 --server-arg -Q --server-arg CS6
   --expect-config "FLOW0_HBINTERVAL=\"5000\"")
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_tc_fl_hb_disabled_ipv6
   ::1       sctp const10:const1000:const10:const1000:tc=AF31:fl=0x12345:hbinterval=0 plain 2
   --expect-config "FLOW0_HBINTERVAL=\"0\"")
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_multistream_shared_tc_hb
   127.0.0.1 sctp const10:const1000:const10:const1000:tc=AF21:hbinterval=5000:hbpathmaxrtx=3:hbassocmaxrtx=6 plain 3
   --stream const10:const500:const10:const500:tc=AF21:hbinterval=5000:hbpathmaxrtx=3:hbassocmaxrtx=6
   --expect-config "FLOW1_TRAFFIC_CLASS=\"0x48\"")


# ===========================================================================
# 50 REALISTIC SCTP APPLICATION DISTRIBUTION PROFILES (Profiles 101 - 150)
# ===========================================================================

# ====== 15. 3GPP 4G LTE & 5G Cellular Core Signaling =======================
# 101. 4G LTE S1AP / NAS Signaling Transport (20 fps, ~200 B / ~230 B, uniform request/response)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_s1ap_nas_transport
   127.0.0.1 sctp uniform18,22:uniform120,280:uniform18,22:uniform140,320 plain 8)

# 102. 5G NGAP gNodeB Inter-gNB Handover Coordination over IPv6 (15 fps, 350 B / 420 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_ngap_handover_v6
   ::1       sctp const15:truncnormal350,30:const15:truncnormal420,40 plain 8)

# 103. Diameter S6a HSS Authentication Vectors (30 fps, 320 B request -> Exp 600 B reply)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_diameter_s6a_auth
   127.0.0.1 sctp uniform25,35:uniform240,400:uniform25,35:exp600 plain 8)

# 104. Diameter Gx/Gy Real-Time Credit-Control Policy & Charging (40 fps, 280 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_diameter_gx_gy_charging
   127.0.0.1 sctp const40:truncnormal280,25:const40:truncnormal280,25 plain 8)

# 105. LTE X2AP Neighbor Cell Load Indication over IPv6 (15 fps, 240 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_x2ap_load_indication_v6
   ::1       sctp uniform10,20:uniform180,300:uniform10,20:uniform180,300 plain 8)

# 106. SIGTRAN M3UA SS7 ISUP Call Setup Routing (25 fps, 128 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_m3ua_ss7_isup
   127.0.0.1 sctp const25:uniform96,160:const25:uniform96,160 plain 8)

# 107. SIGTRAN SUA SCCP Subsystem Routing Management (15 fps, 210 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_sua_sccp_routing
   127.0.0.1 sctp uniform12,18:truncnormal210,20:uniform12,18:truncnormal210,20 plain 8)

# 108. SIGTRAN M2PA Link Alignment State Sync over IPv6 (50 fps, 64 B small frames, const)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_m2pa_link_alignment_v6
   ::1       sctp const50:const64:const50:const64 plain 8)

# 109. 5G N2 AMF-SMF PDU Session Establishment (20 fps, Pareto mean 450 B, pareto)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_5g_n2_n3_setup
   127.0.0.1 sctp uniform15,25:pareto300,3:uniform15,25:pareto300,3 plain 8)

# 110. CAMEL CAP Prepaid Call Interrogation (10 fps, 325 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_telco_camel_cap_billing
   127.0.0.1 sctp const10:uniform200,450:const10:uniform200,450 plain 8)


# ====== 16. WebRTC SCTP Data Channels (RFC 8831) ===========================
# 111. WebRTC Data Channel Low-Latency Game Controller Input (60 fps, 90 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_webrtc_dc_game_input
   127.0.0.1 sctp truncnormal60,3:uniform70,110:truncnormal60,3:uniform70,110 plain 8)

# 112. WebRTC In-Conference Text Chat Stream over IPv6 (10 fps, 200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_webrtc_dc_chat_messages_v6
   ::1       sctp uniform5,15:uniform80,320:uniform5,15:uniform80,320 plain 8)

# 113. WebRTC Collaborative Canvas Vector Stroke Sync (30 fps, 180 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_webrtc_dc_whiteboard_sync
   127.0.0.1 sctp const30:truncnormal180,20:const30:truncnormal180,20 plain 8)

# 114. WebRTC P2P Chunked File Sharing with Framing (20 fps, 4000 B frame, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_webrtc_dc_file_chunk_transfer
   127.0.0.1 sctp const20:const4000:const0:const0:maxmsgsize=1000 plain 8)

# 115. WebRTC Client-Side WebVitals & QoS Telemetry over IPv6 (15 fps, 200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_webrtc_dc_telemetry_metrics_v6
   ::1       sctp uniform10,20:uniform150,250:uniform10,20:uniform150,250 plain 8)

# 116. WebRTC High-Rate Virtual Mouse / Laser Pointer Stream (~100 fps, ~48 B, realistic input jitter)
ADD_NETPERFMETER_FLOW_TEST(profile_webrtc_dc_pointer_teleoperation
   127.0.0.1 sctp truncnormal100,4:uniform40,56:truncnormal100,4:uniform40,56 plain 8)

# 117. WebRTC Metaverse 3D Spatial Audio Orientation Metadata (50 fps, 120 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_webrtc_dc_spatial_audio_meta
   127.0.0.1 sctp uniform45,55:truncnormal120,15:uniform45,55:truncnormal120,15 plain 8)

# 118. WebRTC MLS End-to-End Encryption Key Ratchet Exchange over IPv6 (4 fps, 400 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_webrtc_dc_e2ee_key_ratchet_v6
   ::1       sctp const4:uniform300,500:const4:uniform300,500 plain 8)


# ====== 17. Aviation, Air Traffic Management & Maritime ====================
# 119. Eurocontrol ASTERIX Category 048 Monoradar Target Reports (40 fps, 380 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_asterix_cat048_radar
   127.0.0.1 sctp uniform35,45:uniform280,480:uniform35,45:uniform280,480 plain 8)

# 120. ASTERIX CAT 062 Track Position Exchange over IPv6 (50 fps, 320 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_asterix_cat062_system_track
   ::1       sctp const50:truncnormal320,30:const50:truncnormal320,30 plain 8)

# 121. ICAO ATN/IPS Controller-Pilot Data Link Communications (8 fps, 250 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_atn_ips_cpdlc
   127.0.0.1 sctp uniform6,10:uniform150,350:uniform6,10:uniform150,350 plain 8)

# 122. Aeronautical Message Handling System (AMHS) Flight Plans (6 fps, Pareto mean 1200 B)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_aftn_amhs_flight_plans
   127.0.0.1 sctp const6:pareto800,3:const6:pareto800,3 plain 8)

# 123. FAA SWIM Digital NOTAM / Weather Bulletin Stream over IPv6 (11 fps, Exp mean 800 B)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_swim_notam_dissemination_v6
   ::1       sctp uniform8,14:exp800:uniform8,14:exp800 plain 8)

# 124. Coastal Vessel Traffic Service (VTS) Radar/AIS Fusion (25 fps, 300 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_marine_vts_ais_radar_fusion
   127.0.0.1 sctp const25:uniform200,400:const25:uniform200,400 plain 8)

# 125. Oceanic Airspace ADS-C Position & Meteorological Reporting (5 fps, 220 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_ads_c_periodic_reports
   127.0.0.1 sctp const5:uniform180,260:const5:uniform180,260 plain 8)


# ====== 18. Railway Signaling, Power Grid & Smart Utilities ================
# 126. ERTMS / ETCS Level 2 Radio Block Centre Movement Authority (20 fps, 220 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_rail_etcs_rbc_movement_auth
   127.0.0.1 sctp const20:truncnormal220,20:const20:truncnormal220,20 plain 8)

# 127. 5G-Rail (FRMCS) Train-to-Ground Critical Control Data over IPv6 (25 fps, 225 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_rail_frmcs_mission_critical_data_v6
   ::1       sctp uniform20,30:uniform150,300:uniform20,30:uniform150,300 plain 8)

# 128. Automated Train Operation (ATO) Trajectory & Speed Profiles (10 fps, 400 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_rail_ato_speed_profile_sync
   127.0.0.1 sctp const10:uniform300,500:const10:uniform300,500 plain 8)

# 129. IEEE C37.118 Synchrophasor PMU High-Speed Grid Frequency Data (50 fps, 140 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_smartgrid_pmu_c37_118
   127.0.0.1 sctp const50:truncnormal140,10:const50:truncnormal140,10 plain 8)

# 130. High-Voltage Electrical Substation IEC 60870-5-104 Telecontrol (12 fps, 130 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_substation_iec_60870_5_104_v6
   ::1       sctp uniform8,16:uniform90,170:uniform8,16:uniform90,170 plain 8)

# 131. Urban Metro CBTC Interlocking Vital Beacon Protocol (40 fps, 80 B, const)
ADD_NETPERFMETER_FLOW_TEST(profile_metro_cbct_interlocking_beacons
   127.0.0.1 sctp const40:const80:const40:const80 plain 8)

# 132. Municipal Water Treatment & Pump Telemetry Ring (8 fps, 180 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_water_scada_distribution_ring
   127.0.0.1 sctp uniform5,11:uniform120,240:uniform5,11:uniform120,240 plain 8)


# ====== 19. Financial Systems & Exchange Trading ===========================
# 133. FIX Protocol Multi-Stream Direct Market Access Execution (50 fps, 300 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_fix_multistream_orders
   127.0.0.1 sctp uniform40,60:uniform220,380:uniform40,60:uniform220,380 plain 8)

# 134. FAST Protocol Compressed Equities Order Book Deltas over IPv6 (80 fps, 110 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_fast_market_data_delta_v6
   ::1       sctp const80:truncnormal110,12:const80:truncnormal110,12 plain 8)

# 135. Derivatives Volatility Surface & Greeks Real-Time Stream (25 fps, Pareto mean 600 B)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_options_greeks_feed
   127.0.0.1 sctp const25:pareto400,3:const25:pareto400,3 plain 8)

# 136. Multi-Bank FX ECN Spot Quotation Pipeline (70 fps, 130 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_forex_aggregator_quote
   127.0.0.1 sctp truncnormal70,5:uniform100,160:truncnormal70,5:uniform100,160 plain 8)

# 137. Central Counterparty (CCP) Trade Allocation with Framing (8 fps, 1800 B, maxmsgsize=800)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_clearing_settlement_batch_v6
   ::1       sctp const8:truncnormal1800,150:const8:truncnormal1800,150:maxmsgsize=800 plain 8)

# 138. Crypto Exchange Cross-Margin Collateral Synchronization (40 fps, 240 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_crypto_spot_perpetual_sync
   127.0.0.1 sctp uniform30,50:uniform160,320:uniform30,50:uniform160,320 plain 8)


# ====== 20. High-Availability Clustering & Distributed Systems =============
# 139. Corosync Totem Single-Ring Token Passing Heartbeat (50 fps, 72 B, const)
ADD_NETPERFMETER_FLOW_TEST(profile_cluster_corosync_totem_ring
   127.0.0.1 sctp const50:const72:const50:const72 plain 8)

# 140. Distributed Lock Manager (DLM) Resource Queue Arbitration (30 fps, 160 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_cluster_dlm_lock_arbitration_v6
   ::1       sctp uniform25,35:truncnormal160,20:uniform25,35:truncnormal160,20 plain 8)

# 141. DRBD Synchronous Storage Block Replication Stream (20 fps, 4096 B block, maxmsgsize=1400)
ADD_NETPERFMETER_FLOW_TEST(profile_cluster_drbd_block_replication
   127.0.0.1 sctp const20:const4096:const0:const0:maxmsgsize=1400 plain 8)

# 142. NewSQL Multi-Stream Raft Log Replicated Entries (40 fps, 350 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_cluster_database_raft_multistream
   127.0.0.1 sctp uniform30,50:uniform200,500:uniform30,50:uniform200,500 plain 8)

# 143. Ceph Storage OSD Heartbeat & Cluster Map Peering over IPv6 (15 fps, 600 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_cluster_ceph_osd_peering_v6
   ::1       sctp const15:truncnormal600,50:const15:truncnormal600,50 plain 8)

# 144. In-Memory Distributed Cache Key Invalidation Feed (20 fps, Exp mean 500 B)
ADD_NETPERFMETER_FLOW_TEST(profile_cluster_distributed_cache_invalidation
   127.0.0.1 sctp uniform15,25:exp500:uniform15,25:exp500 plain 8)


# ====== 21. Defense, Healthcare & Intelligent Robotics =====================
# 145. NATO C2 Joint Friendly Force Situational Awareness Stream (25 fps, 400 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_defense_c2_situational_awareness
   127.0.0.1 sctp uniform20,30:uniform250,550:uniform20,30:uniform250,550 plain 8)

# 146. Unmanned Military Drone Flight Data & Waypoint Uplink over IPv6 (30 fps, 220 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_defense_uav_telemetry_uplink_v6
   ::1       sctp const30:truncnormal220,25:const30:truncnormal220,25 plain 8)

# 147. Laparoscopic Surgical Robot Force Feedback Master-Slave Stream (~100 fps, ~64 B, realistic input jitter)
ADD_NETPERFMETER_FLOW_TEST(profile_med_teleoperation_surgical_force
   127.0.0.1 sctp truncnormal100,3:uniform56,72:truncnormal100,3:uniform56,72 plain 8)

# 148. ICU Multi-Channel Continuous EEG Neuromonitoring (50 fps, 480 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_med_multilead_eeg_waveform_v6
   ::1       sctp const50:truncnormal480,40:const50:truncnormal480,40 plain 8)

# 149. Autonomous Warehouse AMR SLAM Grid Occupancy Updates (8 fps, Pareto mean 1800 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_robotics_fleet_slam_occupancy
   127.0.0.1 sctp const8:pareto1200,3:const8:pareto1200,3:maxmsgsize=1000 plain 8)

# 150. Transmission Grid Substation Wide-Area Phasor Concentrator over IPv6 (35 fps, 350 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_smartgrid_substation_phasor_gateway_v6
   ::1       sctp uniform30,40:truncnormal350,30:uniform30,40:truncnormal350,30 plain 8)

ENDIF()
