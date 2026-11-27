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
#### MID-SESSION SERVER ABORT REGRESSION TESTS                           ####
#############################################################################

# Active client must exit cleanly without SIGABRT when server stops mid-transfer (error_on_abort=on)
ADD_NETPERFMETER_SERVER_ABORT_TEST(session_abort_error_on_abort_on  127.0.0.1 error_on_abort_on)

# Active client must continue uninterrupted until runtime expires when error_on_abort=off
ADD_NETPERFMETER_SERVER_ABORT_TEST(session_abort_error_on_abort_off 127.0.0.1 error_on_abort_off)


#############################################################################
#### MULTI-CLIENT SESSION CLEANUP & SERVER LIFECYCLE TESTS               ####
#############################################################################

# 1. Two clients simultaneously running TCP and UDP flows
ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_2clients_udp_tcp
   127.0.0.1 concurrent_2clients_udp_tcp)

# 2. Staggered overlapping clients: Client 2 finishes and tears down session while Client 1 is mid-flight
ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_staggered_overlap_udp_tcp
   127.0.0.1 staggered_overlap_udp_tcp)

# 3. Sequential session reuse: 3 client generations run back-to-back against the same server
ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_sequential_session_reuse
   127.0.0.1 sequential_session_reuse)

# 4. Two clients running simultaneously over IPv6
ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_2clients_ipv6
   ::1 concurrent_2clients_ipv6)

# 5. MPTCP Multi-Client Scenarios
IF (HAVE_MPTCP_FOR_TESTS)
   # Two clients simultaneously running MPTCP flows over IPv4
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_2clients_mptcp
      127.0.0.1 concurrent_2clients_mptcp)

   # Two clients simultaneously running MPTCP flows over IPv6
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_2clients_mptcp_ipv6
      ::1 concurrent_2clients_mptcp)

   # Staggered overlapping clients over MPTCP
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_staggered_overlap_mptcp
      127.0.0.1 staggered_overlap_mptcp)

   # Tri-protocol concurrent stress: 3 clients (MPTCP + TCP + UDP)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_3clients_mptcp_tcp_udp
      127.0.0.1 concurrent_3clients_mptcp_tcp_udp)
ENDIF()

# 6. Tri-protocol concurrent stress: 3 clients (SCTP multistream + TCP/UDP + 2x UDP)
IF (HAVE_SCTP_FOR_TESTS)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_3clients_mixed
      127.0.0.1 concurrent_3clients_mixed)
ENDIF()

# 7. Quad-protocol concurrent stress: 4 clients (MPTCP + SCTP + TCP + UDP)
IF (HAVE_MPTCP_FOR_TESTS AND HAVE_SCTP_FOR_TESTS)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_4clients_mptcp_sctp_tcp_udp
      127.0.0.1 concurrent_4clients_mptcp_sctp_tcp_udp)
ENDIF()

# 8. QUIC Multi-Client Scenarios
IF (HAVE_QUIC_FOR_TESTS)
   # Two clients simultaneously running QUIC flows
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_2clients_quic
      127.0.0.1 concurrent_2clients_quic)

   # Staggered overlapping clients over QUIC
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_staggered_overlap_quic
      127.0.0.1 staggered_overlap_quic)
ENDIF()

# 9. Quad-protocol concurrent stress: 4 clients (QUIC + SCTP + TCP + UDP)
IF (HAVE_SCTP_FOR_TESTS AND HAVE_QUIC_FOR_TESTS)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_4clients_all_protocols
      127.0.0.1 concurrent_4clients_all_protocols)
ENDIF()

# 10. Penta-protocol concurrent stress: 5 clients (QUIC + MPTCP + SCTP + TCP + UDP)
IF (HAVE_MPTCP_FOR_TESTS AND HAVE_SCTP_FOR_TESTS AND HAVE_QUIC_FOR_TESTS)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_cleanup_concurrent_5clients_all_protocols
      127.0.0.1 concurrent_5clients_all_protocols)
ENDIF()


# 11. Sequential Multi-Stream & Multi-Flow FD Reuse Regression Tests
# 3 consecutive 3-flow UDP sessions with distinct QoS (tc=/fl=) markings
ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_multiflow_udp_qos_ipv4
   127.0.0.1 sequential_multiflow_udp_qos)
ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_multiflow_udp_qos_ipv6
   ::1 sequential_multiflow_udp_qos)

IF (HAVE_SCTP_FOR_TESTS)
   # 3 consecutive 3-stream SCTP sessions over SCTP control (IPv4 & IPv6, bz2 outputs)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_multistream_sctp_ipv4
      127.0.0.1 sequential_multistream_sctp)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_multistream_sctp_ipv6
      ::1 sequential_multistream_sctp)

   # 2 consecutive generations, each with 2 independent 3-stream SCTP associations
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_multi_assoc_sctp
      127.0.0.1 sequential_multi_assoc_multistream_sctp)

   # Mid-transfer SIGKILL on 3-stream SCTP client followed by a full 3-stream SCTP session
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sctp_multistream_after_sigkill
      127.0.0.1 sequential_multistream_sctp_after_sigkill)

   # Staggered overlap + immediate fd reuse across 3-stream SCTP clients
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_staggered_overlap_multistream_sctp
      127.0.0.1 staggered_overlap_multistream_sctp)

   # Alternating mixed protocols across sequential generations (3-stream SCTP -> TCP+2xUDP -> 4-stream SCTP)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_mixed_protocol_multistream
      127.0.0.1 sequential_mixed_protocol_multistream)
ENDIF()

IF (HAVE_QUIC_FOR_TESTS)
   # 3 consecutive 3-stream QUIC sessions (TCP control on IPv4 & IPv6, and QUIC control on IPv4)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_multistream_quic_ipv4
      127.0.0.1 sequential_multistream_quic)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_multistream_quic_ipv6
      ::1 sequential_multistream_quic)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_multistream_quic_ctrl_quic
      127.0.0.1 sequential_multistream_quic_ctrl_quic)

   # 2 consecutive generations, each with 2 independent 3-stream QUIC connections
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_multi_assoc_quic
      127.0.0.1 sequential_multi_assoc_multistream_quic)

   # Mid-transfer SIGKILL on 3-stream QUIC client followed by a full 3-stream QUIC session
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_quic_multistream_after_sigkill
      127.0.0.1 sequential_multistream_quic_after_sigkill)

   # Staggered overlap + immediate fd reuse across 3-stream QUIC clients
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_staggered_overlap_multistream_quic
      127.0.0.1 staggered_overlap_multistream_quic)
ENDIF()

IF (HAVE_SCTP_FOR_TESTS AND HAVE_QUIC_FOR_TESTS)
   # Alternating mixed protocols across sequential generations (3-stream SCTP -> TCP+2xUDP -> 3-stream QUIC -> 4-stream SCTP)
   ADD_NETPERFMETER_MULTI_CLIENT_TEST(session_reuse_sequential_mixed_protocol_all
      127.0.0.1 sequential_mixed_protocol_all)
ENDIF()


#############################################################################
#### PROTOCOL & RUNTIME EDGE CASE TESTS                                  ####
#############################################################################

# 1. Passive server automatic shutdown upon -runtime expiration
ADD_NETPERFMETER_EDGE_CASE_TEST(edge_passive_runtime_expiration
   127.0.0.1 passive_runtime_expiration)

# 2. Graceful SIGINT interruption of active client mid-measurement
ADD_NETPERFMETER_EDGE_CASE_TEST(edge_active_sigint_graceful_stop
   127.0.0.1 active_sigint_graceful_stop)

# 3. Control-channel mid-measurement abort / dead-peer timeout tests
ADD_NETPERFMETER_EDGE_CASE_TEST(edge_control_tcp_abort
   127.0.0.1 control_tcp_abort)

IF (HAVE_SCTP_FOR_TESTS)
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_control_heartbeat_timeout
      127.0.0.1 control_heartbeat_timeout)
ENDIF()

IF (HAVE_QUIC_FOR_TESTS)
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_control_quic_abort
      127.0.0.1 control_quic_abort)
ENDIF()

# 4. Passive control-protocol disable flags (-Y, -X, -W, -Z)
IF (HAVE_SCTP_FOR_TESTS)
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_passive_disable_ctrl_tcp
      127.0.0.1 passive_disable_ctrl_tcp)
ENDIF()

IF (HAVE_MPTCP_FOR_TESTS AND HAVE_SCTP_FOR_TESTS)
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_passive_disable_ctrl_mptcp
      127.0.0.1 passive_disable_ctrl_mptcp)
ENDIF()

IF (HAVE_SCTP_FOR_TESTS)
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_passive_disable_ctrl_sctp
      127.0.0.1 passive_disable_ctrl_sctp)
ENDIF()

IF (HAVE_QUIC_FOR_TESTS)
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_passive_disable_ctrl_quic
      127.0.0.1 passive_disable_ctrl_quic)

   # 5. QUIC TLS certificate, CA, hostname & PEM error cases
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_quic_tls_corrupt_pem
      127.0.0.1 quic_tls_corrupt_pem)
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_quic_tls_untrusted_ca
      127.0.0.1 quic_tls_untrusted_ca)
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_quic_tls_untrusted_client_cert
      127.0.0.1 quic_tls_untrusted_client_cert)
   ADD_NETPERFMETER_EDGE_CASE_TEST(edge_quic_tls_hostname_mismatch
      127.0.0.1 quic_tls_hostname_mismatch)
ENDIF()
