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

SET(TEST_LOCALE_DIR "${CMAKE_BINARY_DIR}/po")


# Configurable starting port; each test reserves a block of 10 ports
SET(TEST_BASE_PORT 20000 CACHE STRING "Initial base port number for automated flow tests")
SET_PROPERTY(GLOBAL PROPERTY NPM_TEST_PORT_COUNTER ${TEST_BASE_PORT})

# Helper to fetch and advance the port counter
FUNCTION(GET_NEXT_TEST_PORT OUT_PORT)
   GET_PROPERTY(CURRENT_PORT GLOBAL PROPERTY NPM_TEST_PORT_COUNTER)
   SET(${OUT_PORT} ${CURRENT_PORT} PARENT_SCOPE)
   MATH(EXPR NEXT_PORT "${CURRENT_PORT} + 10")
   SET_PROPERTY(GLOBAL PROPERTY NPM_TEST_PORT_COUNTER ${NEXT_PORT})
ENDFUNCTION()


# ###### Generic helper for compiled binary targets (expected code: 0) ######
FUNCTION(ADD_PROGRAM_TEST TEST_NAME TARGET_NAME)
   ADD_TEST(
      NAME ${TEST_NAME}
      COMMAND $<TARGET_FILE:${TARGET_NAME}> ${ARGN}
   )
   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:"
      LABELS "cli;${TARGET_NAME}"
   )
ENDFUNCTION()


# ###### Backward-compatible wrapper for NetPerfMeter binary ################
FUNCTION(ADD_NETPERFMETER_TEST TEST_NAME)
   ADD_PROGRAM_TEST(${TEST_NAME} netperfmeter ${ARGN})
ENDFUNCTION()


# ###### Helper for expected failures (expected return code: non-zero) ######
FUNCTION(ADD_NETPERFMETER_FAIL_TEST TEST_NAME)
   ADD_TEST(
      NAME ${TEST_NAME}
      COMMAND $<TARGET_FILE:netperfmeter> ${ARGN}
   )
   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      WILL_FAIL TRUE
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      LABELS "negative;error;netperfmeter"
   )
ENDFUNCTION()


# ###### Helper for standard compiled tool CLI tests ########################
MACRO(ADD_TOOL_CLI_TESTS TARGET_NAME)
   IF(TARGET ${TARGET_NAME})
      # Version checks (-v, --version)
      ADD_PROGRAM_TEST(cli_${TARGET_NAME}_version_short ${TARGET_NAME} -v)
      ADD_PROGRAM_TEST(cli_${TARGET_NAME}_version_long  ${TARGET_NAME} --version)
      SET_TESTS_PROPERTIES(cli_${TARGET_NAME}_version_short cli_${TARGET_NAME}_version_long PROPERTIES
         PASS_REGULAR_EXPRESSION "[0-9]+\\.[0-9]+\\.[0-9]+"
      )

      # Help / Usage checks (-h, --help)
      ADD_PROGRAM_TEST(cli_${TARGET_NAME}_help_short ${TARGET_NAME} -h)
      ADD_PROGRAM_TEST(cli_${TARGET_NAME}_help_long  ${TARGET_NAME} --help)
      SET_TESTS_PROPERTIES(cli_${TARGET_NAME}_help_short cli_${TARGET_NAME}_help_long PROPERTIES
         PASS_REGULAR_EXPRESSION "Usage:"
      )
   ENDIF()
ENDMACRO()


# ###### Helper for script CLI tests (testing configured .versioned files) ##
FUNCTION(ADD_SCRIPT_CLI_TESTS SCRIPT_NAME)
   SET(SCRIPT_PATH "${CMAKE_CURRENT_BINARY_DIR}/../${SCRIPT_NAME}.versioned")
   IF(NOT EXISTS "${SCRIPT_PATH}")
      SET(SCRIPT_PATH "${PROJECT_BINARY_DIR}/src/${SCRIPT_NAME}.versioned")
   ENDIF()

   EXECUTE_PROCESS(COMMAND chmod +x "${SCRIPT_PATH}")

   # Version checks (-v, --version)
   ADD_TEST(
      NAME cli_${SCRIPT_NAME}_version_short
      COMMAND "${SCRIPT_PATH}" -v
   )
   ADD_TEST(
      NAME cli_${SCRIPT_NAME}_version_long
      COMMAND "${SCRIPT_PATH}" --version
   )
   SET_TESTS_PROPERTIES(cli_${SCRIPT_NAME}_version_short cli_${SCRIPT_NAME}_version_long PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:@PROJECT_VERSION"
      PASS_REGULAR_EXPRESSION "[0-9]+\\.[0-9]+"
      LABELS "cli;script;${SCRIPT_NAME}"
   )

   # Help / Usage checks (-h, --help)
   ADD_TEST(
      NAME cli_${SCRIPT_NAME}_help_short
      COMMAND "${SCRIPT_PATH}" -h
   )
   ADD_TEST(
      NAME cli_${SCRIPT_NAME}_help_long
      COMMAND "${SCRIPT_PATH}" --help
   )
   SET_TESTS_PROPERTIES(cli_${SCRIPT_NAME}_help_short cli_${SCRIPT_NAME}_help_long PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:"
      PASS_REGULAR_EXPRESSION "Usage:"
      LABELS "cli;script;${SCRIPT_NAME}"
   )
ENDFUNCTION()


# ###### Helper for parameterized flow tests ################################
# Signature:
#   ADD_NETPERFMETER_FLOW_TEST(TEST_NAME ADDRESS PROTOCOL SPEC FORMAT [RUNTIME] [EXTRA_ARGS...])
FUNCTION(ADD_NETPERFMETER_FLOW_TEST TEST_NAME ADDRESS PROTOCOL SPEC FORMAT)
   SET(RUNTIME "2")
   SET(EXTRA_TEST_ARGS "")
   IF(${ARGC} GREATER 5)
      SET(RUNTIME "${ARGV5}")
      IF(${ARGC} GREATER 6)
         MATH(EXPR LAST_INDEX "${ARGC} - 1")
         FOREACH(IDX RANGE 6 ${LAST_INDEX})
            LIST(APPEND EXTRA_TEST_ARGS "${ARGV${IDX}}")
         ENDFOREACH()
      ENDIF()
   ENDIF()

   GET_NEXT_TEST_PORT(TEST_PORT)

   ADD_TEST(
      NAME ${TEST_NAME}
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_flow.sh
         $<TARGET_FILE:netperfmeter>
         ${TEST_PORT}
         ${ADDRESS}
         ${PROTOCOL}
         ${SPEC}
         ${FORMAT}
         ${RUNTIME}
         ${EXTRA_TEST_ARGS}
   )

   SET(TEST_LABELS "flow;${PROTOCOL};${FORMAT};integration")
   IF(ADDRESS STREQUAL "::1")
      LIST(APPEND TEST_LABELS "ipv6")
   ELSE()
      LIST(APPEND TEST_LABELS "ipv4")
   ENDIF()
   IF(SPEC MATCHES "maxmsgsize")
      LIST(APPEND TEST_LABELS "framing;defragmenter")
   ENDIF()
   IF(SPEC MATCHES "onoff")
      LIST(APPEND TEST_LABELS "onoff")
   ENDIF()
   FOREACH(ARG ${EXTRA_TEST_ARGS})
      IF(ARG MATCHES "^--no-(.*)")
         LIST(APPEND TEST_LABELS "no-${CMAKE_MATCH_1}")
      ENDIF()
   ENDFOREACH()

   # Dynamic timeout: accommodates up to 5 retries plus margin under high concurrency (-j64)
   MATH(EXPR TEST_TIMEOUT "(${RUNTIME} * 5) + 40")

   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:"
      LABELS "${TEST_LABELS}"
      TIMEOUT ${TEST_TIMEOUT}
   )
ENDFUNCTION()


#############################################################################
#### BASIC CLI OPTIONS                                                   ####
#############################################################################

# ====== 1. Compiled Binaries ===============================================
ADD_TOOL_CLI_TESTS(netperfmeter)
ADD_TOOL_CLI_TESTS(combinesummaries)
ADD_TOOL_CLI_TESTS(createsummary)
ADD_TOOL_CLI_TESTS(extractvectors)
ADD_TOOL_CLI_TESTS(getabstime)
ADD_TOOL_CLI_TESTS(runtimeestimator)

# ====== 2. Companion Scripts (.versioned) ==================================
ADD_SCRIPT_CLI_TESTS(netperfmeter-module-loader)
ADD_SCRIPT_CLI_TESTS(pdfembedfonts)
ADD_SCRIPT_CLI_TESTS(plot-netperfmeter-results)
ADD_SCRIPT_CLI_TESTS(setpdfmetadata)


#############################################################################
#### NEGATIVE / ERROR HANDLING TESTS                                     ####
#############################################################################

# Invocation without arguments must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_no_args)

# Unknown or malformed options must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_option    --non-existent-option)
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_short_opt -Z)


#############################################################################
#### FLOW INTEGRATION TESTS                                              ####
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

# ====== 4. Compression Formats Matrix ======================================
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_gz  127.0.0.1 tcp const10:const1000:const10:const1000 gz)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_bz2 127.0.0.1 tcp const10:const1000:const10:const1000 bz2)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_xz  127.0.0.1 tcp const10:const1000:const10:const1000 xz)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_zst 127.0.0.1 tcp const10:const1000:const10:const1000 zst)

# ====== 5. Output Combinations (Without Scalars and/or Vectors) ============
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_no_scalar  127.0.0.1 tcp const10:const1000:const10:const1000 plain 2 --no-scalar)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_no_vector  127.0.0.1 tcp const10:const1000:const10:const1000 plain 2 --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_no_outputs 127.0.0.1 tcp const10:const1000:const10:const1000 plain 2 --no-scalar --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_no_outputs 127.0.0.1 udp const10:const1000:const10:const1000 plain 2 --no-scalar --no-vector)

# ====== 6. Framing and Defragmentation (maxmsgsize & frame sizes) ==========
# --- TCP Framing Tests ---
# Small frames (1,000 B) segmented into 250 B messages (4 msgs/frame)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_framing_small      127.0.0.1 tcp const10:const1000:const10:const1000:maxmsgsize=250  plain 2)
# Medium frames (4,000 B) segmented into 1,000 B messages (4 msgs/frame)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_framing_medium     127.0.0.1 tcp const10:const4000:const10:const4000:maxmsgsize=1000 plain 2)
# Large frames (8,000 B) segmented into 1,400 B messages (6 msgs/frame)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_framing_large      127.0.0.1 tcp const10:const8000:const10:const8000:maxmsgsize=1400 plain 2)
# Asymmetric bidirectional frame sizes (out: 4,000 B, in: 1,000 B, maxmsgsize: 500 B)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_framing_asymmetric 127.0.0.1 tcp const10:const4000:const10:const1000:maxmsgsize=500  plain 2)

# --- UDP Framing Tests ---
# Small frames (1,000 B) segmented into 250 B messages
ADD_NETPERFMETER_FLOW_TEST(flow_udp_framing_small      127.0.0.1 udp const10:const1000:const10:const1000:maxmsgsize=250  plain 2)
# Medium frames (4,000 B) segmented into 1,000 B messages
ADD_NETPERFMETER_FLOW_TEST(flow_udp_framing_medium     127.0.0.1 udp const10:const4000:const10:const4000:maxmsgsize=1000 plain 2)
# Large frames (8,000 B) segmented into 1,400 B messages
ADD_NETPERFMETER_FLOW_TEST(flow_udp_framing_large      127.0.0.1 udp const10:const8000:const10:const8000:maxmsgsize=1400 plain 2)
# Asymmetric bidirectional frame sizes (out: 4,000 B, in: 1,000 B, maxmsgsize: 500 B)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_framing_asymmetric 127.0.0.1 udp const10:const4000:const10:const1000:maxmsgsize=500  plain 2)


#############################################################################
#### RANDOM DISTRIBUTION TESTS (VBR FLOWS)                               ####
#############################################################################

# 1. Uniform Distribution (Rate [15, 25) -> HM ~19.6, Size [800, 1200) -> AM 1000)
ADD_NETPERFMETER_FLOW_TEST(dist_tcp_uniform 127.0.0.1 tcp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_udp_uniform 127.0.0.1 udp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)

# 2. Exponential Frame Sizes with Constant Rate (Rate 20 frames/s, Exp Size mean 1000 B)
ADD_NETPERFMETER_FLOW_TEST(dist_tcp_exp     127.0.0.1 tcp const20:exp1000:const20:exp1000 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_udp_exp     127.0.0.1 udp const20:exp1000:const20:exp1000 plain 8)

# 3. Truncated Normal Distribution (Rate mean 20, stddev 2; Size mean 1000, stddev 100)
ADD_NETPERFMETER_FLOW_TEST(dist_tcp_truncnormal 127.0.0.1 tcp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_udp_truncnormal 127.0.0.1 udp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)

# 4. Pareto Distribution with Finite Variance (Shape alpha=3.0 -> HM = (4/3)*k = 20 for k=15; Size k=666.7 -> AM 1000)
ADD_NETPERFMETER_FLOW_TEST(dist_tcp_pareto  127.0.0.1 tcp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_udp_pareto  127.0.0.1 udp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)


#############################################################################
#### ON-OFF FLOW SCHEDULE TESTS (onoff=t1,t2,...)                        ####
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

# ====== 6. Negative Boundary: Events Scheduled Beyond Runtime ==============
# Flow scheduled to turn on at 10s, but runtime is only 3s (asserts 0 bytes transferred)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_onoff_never_active
   127.0.0.1 tcp const20:const1000:const0:const0:onoff=10,20 plain 3)
ADD_NETPERFMETER_FLOW_TEST(flow_udp_onoff_never_active
   127.0.0.1 udp const20:const1000:const0:const0:onoff=10,20 plain 3)


#############################################################################
#### 50 REALISTIC APPLICATION DISTRIBUTION PROFILES                      ####
#############################################################################

# ====== 1. Real-Time Audio, Voice & Interactive Speech =====================
# 1. VoIP G.711 Narrowband Speech (~50 fps, 200 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_voip_g711_narrowband
   127.0.0.1 udp uniform45,55:uniform160,240:uniform45,55:uniform160,240 plain 8)

# 2. VoIP G.729 Low-Bitrate Voice over IPv6 (~50 fps, 60 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_voip_g729_lowbitrate
   ::1       udp uniform45,55:uniform40,80:uniform45,55:uniform40,80 plain 8)

# 3. High-Fidelity Wideband Opus Voice (50 fps, 300 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_voip_opus_wideband
   127.0.0.1 udp uniform48,52:truncnormal300,30:uniform48,52:truncnormal300,30 plain 8)

# 4. Fullband Stereo Audio Stream (50 fps, 500 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_voip_opus_stereo_vbr
   127.0.0.1 udp const50:uniform400,600:const50:uniform400,600 plain 8)

# 5. Compressed Podcast Audio Stream (35 fps, 1100 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_audio_podcast_stream
   127.0.0.1 tcp uniform30,40:uniform1000,1200:uniform30,40:uniform1000,1200 plain 8)

# 6. Lossless Hi-Res Audio Stream (60 fps, 1200 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_audio_flac_lossless
   127.0.0.1 tcp uniform50,70:uniform1000,1400:uniform50,70:uniform1000,1400 plain 8)

# 7. SIP Call Control Signaling (5 fps, 600 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_audio_sip_signaling
   127.0.0.1 tcp const5:uniform400,800:const5:uniform400,800 plain 8)

# 8. Multi-Party Conference Bridge over IPv6 (50 fps, 450 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_voip_conference_bridge
   ::1       udp const50:truncnormal450,40:const50:truncnormal450,40 plain 8)


# ====== 2. Multiplayer Gaming & Virtual World Netcode =====================
# 9. 128-Tick Competitive Gaming State Updates (128 fps, 130 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_gaming_tick_128hz
   127.0.0.1 udp truncnormal128,4:uniform80,180:truncnormal128,4:uniform80,180 plain 8)

# 10. Standard 64-Tick Multiplayer Game Server (64 fps, 150 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_gaming_tick_64hz
   127.0.0.1 udp truncnormal64,3:uniform100,200:truncnormal64,3:uniform100,200 plain 8)

# 11. Console Cross-Platform State over IPv6 (30 fps, 180 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_gaming_tick_30hz_ipv6
   ::1       udp truncnormal30,2:uniform120,240:truncnormal30,2:uniform120,240 plain 8)

# 12. MMO Virtual World Entity Position Updates (20 fps, 400 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_gaming_mmo_worldsync
   127.0.0.1 udp uniform15,25:uniform200,600:uniform15,25:uniform200,600 plain 8)

# 13. Rigid-Body Physics Synchronization (60 fps, 150 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_gaming_physics_sync
   127.0.0.1 udp const60:truncnormal150,15:const60:truncnormal150,15 plain 8)

# 14. Proximity Spatial In-Game Voice Chat (50 fps, 175 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_gaming_voice_spatial
   127.0.0.1 udp uniform40,60:uniform100,250:uniform40,60:uniform100,250 plain 8)

# 15. Game Matchmaking Lobby Exchange (7.5 fps, 500 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_gaming_lobby_match
   127.0.0.1 tcp uniform5,10:uniform300,700:uniform5,10:uniform300,700 plain 8)

# 16. Leaderboard & Score Sync over IPv6 (4 fps, 750 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_gaming_leaderboard
   ::1       tcp const4:uniform500,1000:const4:uniform500,1000 plain 8)


# ====== 3. IoT, SCADA, Smart Grid & Transport Telematics ===================
# 17. Connected Vehicle Safety Beacons over IPv6 (10 fps, 350 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_v2x_cam_safety
   ::1       udp truncnormal10,1:uniform200,500:truncnormal10,1:uniform200,500 plain 8)

# 18. Road Hazard Decentralized Notification (8 fps, 450 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_v2x_denm_hazard
   127.0.0.1 udp const8:uniform300,600:const8:uniform300,600 plain 8)

# 19. Industrial SCADA / Modbus Polling (5 fps, 90 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_iot_scada_modbus
   127.0.0.1 udp const5:uniform60,120:const5:uniform60,120 plain 8)

# 20. Smart Grid Power Meter Telemetry Burst (4 fps, 350 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_iot_smartmeter_burst
   127.0.0.1 udp const4:truncnormal350,30:const4:truncnormal350,30 plain 8)

# 21. Environmental Weather Station over IPv6 (2 fps, 110 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_iot_weather_telemetry
   ::1       udp const2:uniform80,140:const2:uniform80,140 plain 8)

# 22. Factory Floor PLC Line Polling (20 fps, 192 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_iot_industrial_plc
   127.0.0.1 tcp const20:uniform128,256:const20:uniform128,256 plain 8)

# 23. CoAP Environmental Probe over IPv6 (10 fps, 120 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_iot_coap_sensor_node
   ::1       udp uniform5,15:uniform90,150:uniform5,15:uniform90,150 plain 8)

# 24. Fleet Vehicle GPS Tracking Stream (10 fps, 180 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_fleet_gps_tracking
   127.0.0.1 tcp uniform8,12:uniform120,240:uniform8,12:uniform120,240 plain 8)


# ====== 4. Video Streaming, Surveillance & Screen Conferencing =============
# 25. DASH/HLS Video Segment Stream (4 fps, Pareto mean 2250 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_video_dash_chunks
   127.0.0.1 tcp const4:pareto1500,3:const4:pareto1500,3 plain 8)

# 26. DASH Video Segments over IPv6 (4 fps, Pareto mean 2250 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_video_dash_ipv6
   ::1       tcp const4:pareto1500,3:const4:pareto1500,3 plain 8)

# 27. CCTV Security Camera RTSP Stream (25 fps, 1250 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_cctv_rtsp_stream
   127.0.0.1 udp truncnormal25,2:uniform1100,1400:truncnormal25,2:uniform1100,1400 plain 8)

# 28. H.265 IP Surveillance Feed (30 fps, Pareto mean 1200 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_cctv_h265_surveillance
   127.0.0.1 udp const30:pareto800,3:const30:pareto800,3 plain 8)

# 29. Remote Desktop VNC Screen Delta Tiles (20 fps, Pareto mean 600 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_screen_sharing_vbr
   127.0.0.1 tcp uniform15,25:pareto400,3:uniform15,25:pareto400,3 plain 8)

# 30. WebRTC Video Layer over IPv6 (30 fps, 1000 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_webrtc_simulcast
   ::1       udp uniform25,35:uniform800,1200:uniform25,35:uniform800,1200 plain 8)

# 31. Presentation Deck Screenshare Stream (15 fps, 1100 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_videoconf_screenshare
   127.0.0.1 tcp const15:truncnormal1100,100:const15:truncnormal1100,100 plain 8)

# 32. Drone FPV Low-Latency Stream (60 fps, 800 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_drone_fpv_video_feed
   127.0.0.1 udp truncnormal60,3:uniform700,900:truncnormal60,3:uniform700,900 plain 8)


# ====== 5. Web APIs, Microservices & Edge Gateways =========================
# 33. Asymmetric gRPC RPC (25 fps, 200 B request -> 1200 B Exp response, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_grpc_unary_asymmetric
   127.0.0.1 tcp uniform20,30:uniform100,300:uniform20,30:exp1200 plain 8)

# 34. Bidirectional gRPC Streaming Pipe (35 fps, 400 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_grpc_bidi_stream
   127.0.0.1 tcp uniform30,40:truncnormal400,40:uniform30,40:truncnormal400,40 plain 8)

# 35. Asymmetric GraphQL Queries over IPv6 (15 fps, 300 B query -> Pareto mean 900 B resp, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_graphql_api_ipv6
   ::1       tcp const15:uniform200,400:const15:pareto600,3 plain 8)

# 36. Encrypted DNS-over-TLS Pattern (15 fps, 100 B query / 260 B response, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_dns_dot_tls_queries
   127.0.0.1 tcp uniform10,20:uniform60,140:uniform10,20:uniform120,400 plain 8)

# 37. Encrypted DNS-over-HTTPS over IPv6 (20 fps, 120 B / 300 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_dns_doh_https_ipv6
   ::1       tcp uniform15,25:uniform80,160:uniform15,25:uniform180,420 plain 8)

# 38. Cluster Pub/Sub Event Notification Bus (40 fps, 500 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_microservice_eventbus
   127.0.0.1 udp uniform30,50:truncnormal500,50:uniform30,50:truncnormal500,50 plain 8)

# 39. API Gateway Ingress Proxy (30 fps, 350 B request -> Exp 1000 B response, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_api_gateway_proxy
   127.0.0.1 tcp uniform25,35:uniform200,500:uniform25,35:exp1000 plain 8)

# 40. Webhook Dispatch over IPv6 (10 fps, 600 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_webhook_delivery
   ::1       tcp const10:truncnormal600,60:const10:truncnormal600,60 plain 8)


# ====== 6. Databases, Caches & Distributed Consensus ======================
# 41. Redis In-Memory Key-Value Pipeline (80 fps, 100 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_db_redis_keyval
   127.0.0.1 tcp uniform70,90:uniform60,140:uniform70,90:uniform60,140 plain 8)

# 42. Memcached UDP Retrieval Batches (70 fps, 120 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_db_memcached_pipeline
   127.0.0.1 udp uniform60,80:uniform80,160:uniform60,80:uniform80,160 plain 8)

# 43. Distributed Database WAL Replication over IPv6 (20 fps, 1200 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_db_replicated_sync_v6
   ::1       tcp const20:truncnormal1200,100:const20:truncnormal1200,100 plain 8)

# 44. Time-Series Metric Ingest Batching (20 fps, 900 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_timeseries_ingest
   127.0.0.1 tcp uniform15,25:truncnormal900,80:uniform15,25:truncnormal900,80 plain 8)

# 45. Enterprise SIEM Syslog Pipeline (50 fps, 400 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_syslog_siem_pipeline
   127.0.0.1 udp uniform40,60:uniform250,550:uniform40,60:uniform250,550 plain 8)

# 46. Raft Consensus Heartbeat Synchronization (50 fps, 64 B small frames, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_raft_consensus_hb
   127.0.0.1 tcp const50:const64:const50:const64 plain 8)

# 47. etcd Distributed Lease Keepalive over IPv6 (20 fps, 100 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_etcd_lease_keepalive
   ::1       tcp const20:uniform80,120:const20:uniform80,120 plain 8)

# 48. Paxos Quorum Voting Exchange (25 fps, 300 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_paxos_election_sync
   127.0.0.1 tcp const25:truncnormal300,30:const25:truncnormal300,30 plain 8)


# ====== 7. Segmented / Defragmenter Stress Workloads =======================
# 49. Kafka Message Batching with Fragmentation (10 fps, Pareto mean 2400 B, maxmsgsize=1000, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_db_kafka_batch_frag
   127.0.0.1 tcp const10:pareto1600,3:const10:pareto1600,3:maxmsgsize=1000 plain 8)

# 50. Backup Catalog Sync with Fragmentation (12 fps, mean 3200 B, maxmsgsize=1200, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_backup_catalog_frag
   127.0.0.1 tcp const12:truncnormal3200,200:const12:truncnormal3200,200:maxmsgsize=1200 plain 8)


# ====== 8. Financial Systems, FinTech & Trading ===========================
# 51. FIX Protocol Market Depth Quotes (40 fps, 325 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_fix_market_data
   127.0.0.1 tcp uniform30,50:uniform200,450:uniform30,50:uniform200,450 plain 8)

# 52. ITCH/OUCH Low-Latency Market Depth Feed (80 fps, 96 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_itch_feed_udp
   127.0.0.1 udp truncnormal80,4:uniform64,128:truncnormal80,4:uniform64,128 plain 8)

# 53. High-Frequency Algorithmic Execution over IPv6 (100 fps, 120 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_hft_execution_v6
   ::1       tcp const100:truncnormal120,10:const100:truncnormal120,10 plain 8)

# 54. Payment Gateway Card Authorization (20 fps, asymmetric request/response, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_iso8583_auth
   127.0.0.1 tcp uniform15,25:uniform300,500:uniform15,25:exp900 plain 8)

# 55. WebSocket Crypto Order Book Level-2 Depth (25 fps, Pareto mean 750 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_crypto_l2_book
   127.0.0.1 tcp const25:pareto500,3:const25:pareto500,3 plain 8)

# 56. Inter-Bank Settlement Batching with Segmentation (10 fps, mean 2400 B, maxmsgsize=800, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_swift_batch_frag
   127.0.0.1 tcp const10:truncnormal2400,150:const10:truncnormal2400,150:maxmsgsize=800 plain 8)

# 57. Centralized Risk Calculation Heartbeat (60 fps, 64 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_risk_heartbeat
   127.0.0.1 udp const60:const64:const60:const64 plain 8)

# 58. Trade Allocation Drop Copy Reports over IPv6 (25 fps, 550 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_fin_drop_copy_reports
   ::1       udp uniform20,30:uniform400,700:uniform20,30:uniform400,700 plain 8)


# ====== 9. Industrial Robotics, Automation & Industry 4.0 =================
# 59. Robot Arm 6-DoF Kinematics Telemetry over IPv6 (100 fps, 150 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_ind_robot_kinematics_v6
   ::1       udp truncnormal100,5:uniform120,180:truncnormal100,5:uniform120,180 plain 8)

# 60. OPC UA Binary Machine Telemetry (20 fps, 400 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_ind_opcua_telemetry
   127.0.0.1 tcp const20:truncnormal400,30:const20:truncnormal400,30 plain 8)

# 61. Machine-Vision Quality Inspection (6 fps, Pareto mean 2250 B, maxmsgsize=1000, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_ind_vision_inspection_frag
   127.0.0.1 tcp const6:pareto1500,3:const6:pareto1500,3:maxmsgsize=1000 plain 8)

# 62. Automated Guided Vehicle (AGV) LIDAR Point Cloud (40 fps, 1250 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_ind_agv_lidar_stream
   127.0.0.1 udp uniform35,45:uniform1100,1400:uniform35,45:uniform1100,1400 plain 8)

# 63. CNC Spindle High-Frequency Vibration Telemetry (80 fps, 96 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_ind_cnc_spindle_vibe
   127.0.0.1 udp const80:uniform64,128:const80:uniform64,128 plain 8)

# 64. Digital Twin Spatial State Sync over IPv6 (25 fps, 1000 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_ind_digital_twin_v6
   ::1       tcp uniform20,30:uniform800,1200:uniform20,30:uniform800,1200 plain 8)

# 65. Emergency Stop (E-Stop) Bus Beacon (100 fps, 48 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_ind_safety_estop
   127.0.0.1 udp const100:const48:const100:const48 plain 8)

# 66. Predictive Maintenance Vibration FFT Spectra (10 fps, Pareto mean 1200 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_ind_predictive_maint
   127.0.0.1 tcp const10:pareto800,3:const10:pareto800,3 plain 8)


# ====== 10. Aerospace, Satellites & Defense ================================
# 67. Satellite CCSDS Telemetry Downlink (20 fps, 750 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_satellite_downlink
   127.0.0.1 udp const20:uniform500,1000:const20:uniform500,1000 plain 8)

# 68. LEO Inter-Satellite Link (ISL) Routing Beacon over IPv6 (15 fps, 150 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_isl_routing_v6
   ::1       udp uniform10,20:uniform100,200:uniform10,20:uniform100,200 plain 8)

# 69. ADS-B Mode-S Aircraft Position Transponder (15 fps, 170 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_adsb_transponder
   127.0.0.1 udp const15:uniform120,220:const15:uniform120,220 plain 8)

# 70. Marine AIS Commercial Vessel Tracking (7.5 fps, 120 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_ais_marine_track
   127.0.0.1 udp uniform5,10:uniform80,160:uniform5,10:uniform80,160 plain 8)

# 71. Autonomous Drone Waypoint Mission Control (12 fps, 250 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_drone_mission_ctrl
   127.0.0.1 tcp const12:truncnormal250,25:const12:truncnormal250,25 plain 8)

# 72. Primary Radar Target Plot Correlation Feed (50 fps, 400 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_radar_track_sync
   127.0.0.1 udp truncnormal50,4:uniform300,500:truncnormal50,4:uniform300,500 plain 8)

# 73. Tactical Data Link (Link-16 style) over IPv6 (25 fps, 180 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_tactical_tdl_v6
   ::1       udp uniform20,30:uniform120,240:uniform20,30:uniform120,240 plain 8)

# 74. Satellite Imagery Raster Tile Sync (5 fps, Pareto mean 3000 B, maxmsgsize=1200, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_aero_sat_imagery_frag
   127.0.0.1 tcp const5:pareto2000,3:const5:pareto2000,3:maxmsgsize=1200 plain 8)


# ====== 11. Healthcare, Biomedical & Clinical Telemetry ====================
# 75. Continuous Bedside Multi-Lead ECG Telemetry (50 fps, 200 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_med_ecg_telemetry
   127.0.0.1 udp const50:uniform150,250:const50:uniform150,250 plain 8)

# 76. PACS DICOM Radiological Imaging Transfer (8 fps, mean 4000 B, maxmsgsize=1000, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_med_pacs_dicom_frag
   127.0.0.1 tcp const8:truncnormal4000,300:const8:truncnormal4000,300:maxmsgsize=1000 plain 8)

# 77. Robotic Telesurgery Bilateral Haptic Feedback over IPv6 (100 fps, 100 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_med_surgical_haptics_v6
   ::1       udp truncnormal100,5:uniform80,120:truncnormal100,5:uniform80,120 plain 8)

# 78. Smart Infusion Pump Rate Telemetry (2 fps, 192 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_med_infusion_pump
   127.0.0.1 tcp const2:uniform128,256:const2:uniform128,256 plain 8)

# 79. Interactive Telehealth Video Consultation (30 fps, 1150 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_med_telehealth_consult
   127.0.0.1 udp const30:uniform1000,1300:const30:uniform1000,1300 plain 8)

# 80. HL7 FHIR Patient Record Sync over IPv6 (12 fps, Exp size mean 900 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_med_fhir_hl7_sync_v6
   ::1       tcp uniform10,15:exp900:uniform10,15:exp900 plain 8)

# 81. Neural Myoelectric Prosthesis Sensor Feedback Loop (50 fps, 80 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_med_prosthesis_feedback
   127.0.0.1 udp uniform40,60:uniform60,100:uniform40,60:uniform60,100 plain 8)

# 82. Critical Care ICU Central Alarm Broadcast over IPv6 (10 fps, 300 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_med_icu_alarm_broadcast
   ::1       udp const10:uniform200,400:const10:uniform200,400 plain 8)


# ====== 12. Cloud Infrastructure, Virtualization & Storage =================
# 83. NVMe-over-Fabrics Completion Queue Processing (50 fps, 512 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_cloud_nvme_of_queue
   127.0.0.1 tcp uniform40,60:truncnormal512,32:uniform40,60:truncnormal512,32 plain 8)

# 84. Distributed Object Storage (Ceph/Gluster) Heartbeat over IPv6 (25 fps, 128 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_cloud_ceph_heartbeat_v6
   ::1       tcp const25:const128:const25:const128 plain 8)

# 85. Kubernetes CNI Inter-Pod Service Mesh Ingress (40 fps, 450 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_cloud_cni_service_proxy
   127.0.0.1 tcp uniform35,45:uniform250,650:uniform35,45:uniform250,650 plain 8)

# 86. Linux eBPF Continuous Observability Trace Feed (65 fps, 300 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_cloud_ebpf_observability
   127.0.0.1 udp uniform50,80:uniform200,400:uniform50,80:uniform200,400 plain 8)

# 87. Hypervisor Live VM Migration Dirty Pages (16 fps, Pareto mean 3300 B, maxmsgsize=1400, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_cloud_vm_migration_frag
   127.0.0.1 tcp const16:pareto2200,3:const16:pareto2200,3:maxmsgsize=1400 plain 8)

# 88. Distributed Coordination (ZooKeeper/etcd) Leases over IPv6 (40 fps, 96 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_cloud_zk_lock_sync_v6
   ::1       tcp const40:uniform64,128:const40:uniform64,128 plain 8)

# 89. OpenSearch Index Shard Rebalancing (10 fps, mean 2800 B, maxmsgsize=1000, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_cloud_elastic_rebalance_frag
   127.0.0.1 tcp const10:truncnormal2800,200:const10:truncnormal2800,200:maxmsgsize=1000 plain 8)

# 90. Serverless FaaS Invocation Trigger (20 fps, asymmetric Exp response 1100 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_cloud_faas_cold_trigger
   127.0.0.1 tcp uniform15,25:uniform300,700:uniform15,25:exp1100 plain 8)


# ====== 13. Smart Home, Consumer IoT & AR/VR ===============================
# 91. Smart Speaker Voice Assistant Stream (45 fps, 400 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_home_voice_assistant
   127.0.0.1 udp uniform40,50:uniform300,500:uniform40,50:uniform300,500 plain 8)

# 92. Smart Thermostat Energy Telemetry Sync (1 fps, 96 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_home_thermostat_sync
   127.0.0.1 tcp const1:uniform64,128:const1:uniform64,128 plain 8)

# 93. Video Doorbell Bidirectional Audio/Video Stream (24 fps, 950 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_home_doorbell_intercom
   127.0.0.1 udp const24:uniform800,1100:const24:uniform800,1100 plain 8)

# 94. Wearable Smartwatch PPG Sensor Stream over IPv6 (5 fps, 80 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_home_wearable_ppg_v6
   ::1       udp const5:uniform60,100:const5:uniform60,100 plain 8)

# 95. Matter / Thread Border Router Discovery Beacon over IPv6 (10 fps, 140 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_home_matter_thread_v6
   ::1       udp uniform8,12:uniform100,180:uniform8,12:uniform100,180 plain 8)

# 96. Robotic Vacuum Indoor SLAM Grid Map Sync (4 fps, Pareto mean 1050 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_home_robovac_slam_map
   127.0.0.1 tcp const4:pareto700,3:const4:pareto700,3 plain 8)

# 97. Smart Lighting Group Multicast Mesh Control (15 fps, 120 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_home_lighting_mesh
   127.0.0.1 udp const15:uniform80,160:const15:uniform80,160 plain 8)

# 98. Augmented Reality Spatial Mesh Coordinate Stream over IPv6 (60 fps, 600 B, UDP)
ADD_NETPERFMETER_FLOW_TEST(profile_home_ar_spatial_mesh_v6
   ::1       udp truncnormal60,4:uniform400,800:truncnormal60,4:uniform400,800 plain 8)


# ====== 14. Edge AI & Distributed Machine Learning =========================
# 99. Edge Computer Vision Inferencing Pipeline (20 fps, Pareto mean 1500 B, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_ai_edge_inferencing
   127.0.0.1 tcp const20:pareto1000,3:const20:pareto1000,3 plain 8)

# 100. Federated Learning Localized Gradient Updates (5 fps, mean 3500 B, maxmsgsize=1200, TCP)
ADD_NETPERFMETER_FLOW_TEST(profile_ai_federated_weights_frag
   ::1       tcp const5:truncnormal3500,250:const5:truncnormal3500,250:maxmsgsize=1200 plain 8)
