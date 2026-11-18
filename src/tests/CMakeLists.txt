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
SET(TEST_BASE_PORT 12000 CACHE STRING "Initial base port number for automated flow tests")
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
FUNCTION(ADD_PROGRAM_FAIL_TEST TEST_NAME TARGET_NAME)
   ADD_TEST(
      NAME ${TEST_NAME}
      COMMAND $<TARGET_FILE:${TARGET_NAME}> ${ARGN}
   )
   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      WILL_FAIL TRUE
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      LABELS "negative;error;${TARGET_NAME}"
   )
ENDFUNCTION()


# ###### Backward-compatible wrapper for NetPerfMeter failure tests #########
FUNCTION(ADD_NETPERFMETER_FAIL_TEST TEST_NAME)
   ADD_PROGRAM_FAIL_TEST(${TEST_NAME} netperfmeter ${ARGN})
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
      ELSEIF(ARG STREQUAL "--control-over-sctp")
         LIST(APPEND TEST_LABELS "control-sctp")
      ELSEIF(ARG STREQUAL "--control-over-mptcp")
         LIST(APPEND TEST_LABELS "control-mptcp")
      ELSEIF(ARG STREQUAL "--control-over-quic")
         LIST(APPEND TEST_LABELS "control-quic")
      ENDIF()
   ENDFOREACH()

   # Dynamic timeout: accommodates up to 5 retries plus margin under high concurrency (-j128)
   MATH(EXPR TEST_TIMEOUT "(${RUNTIME} * 5) + 40")

   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:"
      LABELS "${TEST_LABELS}"
      TIMEOUT ${TEST_TIMEOUT}
   )
ENDFUNCTION()


# ###### Helper for Multi-Client Session Tests ##############################
FUNCTION(ADD_NETPERFMETER_MULTI_CLIENT_TEST TEST_NAME ADDRESS SCENARIO)
   GET_NEXT_TEST_PORT(TEST_PORT)

   ADD_TEST(
      NAME ${TEST_NAME}
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_multi_client.sh
         $<TARGET_FILE:netperfmeter>
         ${TEST_PORT}
         ${ADDRESS}
         ${SCENARIO}
   )

   SET(TEST_LABELS "flow;multi-client;session-cleanup;integration")
   IF(ADDRESS STREQUAL "::1")
      LIST(APPEND TEST_LABELS "ipv6")
   ELSE()
      LIST(APPEND TEST_LABELS "ipv4")
   ENDIF()

   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:;Avbrutt;Aborted;assure"
      LABELS "${TEST_LABELS}"
      TIMEOUT 60
   )
ENDFUNCTION()


# ###### Helper for Mid-Session Server Abort Regression Tests ###############
FUNCTION(ADD_NETPERFMETER_SERVER_ABORT_TEST TEST_NAME ADDRESS MODE)
   GET_NEXT_TEST_PORT(TEST_PORT)

   ADD_TEST(
      NAME ${TEST_NAME}
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_server_abort.sh
         $<TARGET_FILE:netperfmeter>
         ${TEST_PORT}
         ${ADDRESS}
         ${MODE}
   )

   SET(TEST_LABELS "flow;abort-regression;integration")
   IF(ADDRESS STREQUAL "::1")
      LIST(APPEND TEST_LABELS "ipv6")
   ELSE()
      LIST(APPEND TEST_LABELS "ipv4")
   ENDIF()

   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;FAIL:;Avbrutt;Aborted;assure"
      LABELS "${TEST_LABELS}"
      TIMEOUT 30
   )
ENDFUNCTION()


# ###### Helper for Protocol & Runtime Edge Case Tests ######################
FUNCTION(ADD_NETPERFMETER_EDGE_CASE_TEST TEST_NAME ADDRESS SCENARIO)
   GET_NEXT_TEST_PORT(TEST_PORT)

   ADD_TEST(
      NAME ${TEST_NAME}
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_edge_cases.sh
         $<TARGET_FILE:netperfmeter>
         ${TEST_PORT}
         ${ADDRESS}
         ${SCENARIO}
   )

   SET(TEST_LABELS "flow;edge-case;integration")
   IF(ADDRESS STREQUAL "::1")
      LIST(APPEND TEST_LABELS "ipv6")
   ELSE()
      LIST(APPEND TEST_LABELS "ipv4")
   ENDIF()

   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;FAIL:;Avbrutt;Aborted;assure"
      LABELS "${TEST_LABELS}"
      TIMEOUT 120
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
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_short_opt -Q)

# Active mode without any flow specification must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_active_no_flows 127.0.0.1:9000)

# Passive mode with all control channel protocols disabled must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_all_control_disabled 9000 -X -Y -Z)

# Out-of-range flow multiplier (-N / --count) must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_count_zero      127.0.0.1:9000 -N 0   -tcp default)
ADD_NETPERFMETER_FAIL_TEST(fail_count_too_large 127.0.0.1:9000 -N 257 -tcp default)

# Invalid local / control-local bind addresses must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_local_addr         9000 -L 999.999.999.999)
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_control_local_addr 9000 -l 999.999.999.999)

# Invalid remote endpoint / port must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_remote_port 127.0.0.1:0 -tcp default)
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_remote_host invalid..hostname.example:9000 -tcp default)

# Unwritable log file paths must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_logfile_path   127.0.0.1:9000 --logfile /nonexistent_dir_xyz/test.log -tcp default)
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_logappend_path 127.0.0.1:9000 --logappend /nonexistent_dir_xyz/test.log -tcp default)

# Invalid FLOWSPEC syntax, unknown options, probabilities, onoff=...,repeat, and duplicate IDs
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_syntax          127.0.0.1:9000 -tcp invalid_spec)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_unknown_option        127.0.0.1:9000 -tcp const10:const1000:const10:const1000:unknownopt=1)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_ordered       127.0.0.1:9000 -tcp const10:const1000:const10:const1000:ordered=1.5)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_reliable      127.0.0.1:9000 -tcp const10:const1000:const10:const1000:reliable=-0.1)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_onoff_repeat    127.0.0.1:9000 -tcp const10:const1000:const10:const1000:onoff=1,2,3,repeat)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_onoff_repeat_nonrel     127.0.0.1:9000 -tcp const10:const1000:const10:const1000:onoff=1,2,repeat)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_duplicate_id          127.0.0.1:9000 -tcp const10:const1000:const10:const1000:id=1 -tcp const10:const1000:const10:const1000:id=1)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_cmt             127.0.0.1:9000 -tcp const10:const1000:const10:const1000:cmt=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_ccid            127.0.0.1:9000 -tcp const10:const1000:const10:const1000:ccid=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_nodelay         127.0.0.1:9000 -tcp const10:const1000:const10:const1000:nodelay=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_debug           127.0.0.1:9000 -tcp const10:const1000:const10:const1000:debug=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_error_on_abort  127.0.0.1:9000 -tcp const10:const1000:const10:const1000:error_on_abort=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_cc_too_long             127.0.0.1:9000 -tcp const10:const1000:const10:const1000:cc=abcdefghijklmnopqrstuvwxyz0123456789)

IF (HAVE_QUIC_FOR_TESTS)
   # Mismatched TLS private key / certificate options and unsupported QUIC CC must exit with code 1
   ADD_NETPERFMETER_FAIL_TEST(fail_tls_key_without_cert 9000 --tls-key nonexistent.key)
   ADD_NETPERFMETER_FAIL_TEST(fail_tls_cert_without_key 9000 --tls-cert nonexistent.crt)
   ADD_NETPERFMETER_FAIL_TEST(fail_quic_invalid_cc      127.0.0.1:9000 -quic default:cc=bbr)
ENDIF()


#############################################################################
#### UTILITY TOOL TESTS                                                  ####
#############################################################################

IF(WITH_PLOT_PROGRAMS)
   SET(PDFEMBEDFONTS_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/../pdfembedfonts.versioned")
   IF(NOT EXISTS "${PDFEMBEDFONTS_SCRIPT}")
      SET(PDFEMBEDFONTS_SCRIPT "${PROJECT_BINARY_DIR}/src/pdfembedfonts.versioned")
   ENDIF()

   ADD_TEST(
      NAME util_pdfembedfonts_embed
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_pdfembedfonts.sh
         "${PDFEMBEDFONTS_SCRIPT}"
   )
   SET_TESTS_PROPERTIES(util_pdfembedfonts_embed PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;script;utility;pdfembedfonts"
      TIMEOUT 30
   )

   SET(SETPDFMETADATA_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/../setpdfmetadata.versioned")
   IF(NOT EXISTS "${SETPDFMETADATA_SCRIPT}")
      SET(SETPDFMETADATA_SCRIPT "${PROJECT_BINARY_DIR}/src/setpdfmetadata.versioned")
   ENDIF()

   ADD_TEST(
      NAME util_setpdfmetadata_update
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_setpdfmetadata.sh
         "${SETPDFMETADATA_SCRIPT}"
   )
   SET_TESTS_PROPERTIES(util_setpdfmetadata_update PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;script;utility;setpdfmetadata"
      TIMEOUT 15
   )
ENDIF()


IF(TARGET combinesummaries AND TARGET netperfmeter)
   GET_NEXT_TEST_PORT(PLOT_TEST_PORT)
   SET(PLOT_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/../plot-netperfmeter-results.versioned")
   IF(NOT EXISTS "${PLOT_SCRIPT}")
      SET(PLOT_SCRIPT "${PROJECT_BINARY_DIR}/src/plot-netperfmeter-results.versioned")
   ENDIF()

   ADD_TEST(
      NAME util_plot_netperfmeter_results
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_plot_netperfmeter_results.sh
         "${PLOT_SCRIPT}"
         $<TARGET_FILE:combinesummaries>
         "${CMAKE_CURRENT_SOURCE_DIR}/.."
         $<TARGET_FILE:netperfmeter>
         ${PLOT_TEST_PORT}
   )
   SET_TESTS_PROPERTIES(util_plot_netperfmeter_results PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;script;utility;plot-netperfmeter-results"
      TIMEOUT 150
   )
ENDIF()


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
# ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_gz  127.0.0.1 tcp const10:const1000:const10:const1000 gz)
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_bz2 127.0.0.1 tcp const10:const1000:const10:const1000 bz2)
# ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_xz  127.0.0.1 tcp const10:const1000:const10:const1000 xz)
# ADD_NETPERFMETER_FLOW_TEST(flow_tcp_format_zst 127.0.0.1 tcp const10:const1000:const10:const1000 zst)

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
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_cc_cubic
   127.0.0.1 tcp const10:const1000:const10:const1000:cc=cubic plain 2)
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


#############################################################################
#### 300 REALISTIC APPLICATION DISTRIBUTION PROFILES                     ####
#### (Profiles 1-100: TCP & UDP, Profiles 101-150: SCTP,                 ####
####  Profiles 151-200: QUIC, Profiles 201-250: MPTCP,                   ####
####  Profiles 251-300: DCCP)                                            ####
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

# 65. Emergency Stop (E-Stop) Bus Beacon (~100 fps, ~48 B, realistic input jitter)
ADD_NETPERFMETER_FLOW_TEST(profile_ind_safety_estop
   127.0.0.1 udp truncnormal100,3:uniform40,56:truncnormal100,3:uniform40,56 plain 8)

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


#############################################################################
#### MPTCP FLOW INTEGRATION TESTS (RFC 8684)                             ####
#############################################################################

IF (HAVE_MPTCP_FOR_TESTS)

# ====== 1. Bidirectional Flows =============================================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_bidi_ipv4 127.0.0.1 mptcp const10:const1000:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_bidi_ipv6 ::1       mptcp const10:const1000:const10:const1000 plain)

# ====== 2. Upload-Only Flows (Active -> Passive) ===========================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_upload_ipv4 127.0.0.1 mptcp const10:const1000:const0:const0 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_upload_ipv6 ::1       mptcp const10:const1000:const0:const0 plain)

# ====== 3. Download-Only Flows (Passive -> Active) =========================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_download_ipv4 127.0.0.1 mptcp const0:const0:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_download_ipv6 ::1       mptcp const0:const0:const10:const1000 plain)

# ====== 4. Default Shorthand, Control Channel Over MPTCP & Local Bind ======
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_default_shorthand
   127.0.0.1 mptcp default plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_cc_cubic
   127.0.0.1 mptcp const10:const1000:const10:const1000:cc=cubic plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_local_bind_ipv4
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2
   -L 127.0.0.1 -l 127.0.0.1
   --server-arg -L --server-arg 127.0.0.1 --server-arg -l --server-arg 127.0.0.1)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_local_bind_ipv6
   ::1 mptcp const10:const1000:const10:const1000 plain 2
   -L ::1 -l ::1
   --server-arg -L --server-arg ::1 --server-arg -l --server-arg ::1)

# Control channel using MPTCP with MPTCP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_ctrl_mptcp_bidi_ipv4
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2 --control-over-mptcp)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_ctrl_mptcp_bidi_ipv6
   ::1       mptcp const10:const1000:const10:const1000 plain 2 --control-over-mptcp)

# Control channel using MPTCP with TCP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_ctrl_mptcp_bidi_ipv4
   127.0.0.1 tcp   const10:const1000:const10:const1000 plain 2 --control-over-mptcp)

# Control channel using MPTCP with UDP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_udp_ctrl_mptcp_bidi_ipv4
   127.0.0.1 udp   const10:const1000:const10:const1000 plain 2 --control-over-mptcp)

# Backwards compatibility alias: -tcp ...:cmt=mptcp creates an MPTCP connection
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_cmt_alias_ipv4
   127.0.0.1 tcp   const10:const1000:const10:const1000:cmt=mptcp plain 2)

# ====== 5. Compression Formats & Output Suppression ========================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_format_bz2 127.0.0.1 mptcp const10:const1000:const10:const1000 bz2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_no_scalar  127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2 --no-scalar)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_no_vector  127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2 --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_no_outputs 127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2 --no-scalar --no-vector)

# ====== 6. Framing and Defragmentation (maxmsgsize & frame sizes) ==========
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_framing_small      127.0.0.1 mptcp const10:const1000:const10:const1000:maxmsgsize=250  plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_framing_medium     127.0.0.1 mptcp const10:const4000:const10:const4000:maxmsgsize=1000 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_framing_large      127.0.0.1 mptcp const10:const8000:const10:const8000:maxmsgsize=1400 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_framing_asymmetric 127.0.0.1 mptcp const10:const4000:const10:const1000:maxmsgsize=500  plain 2)

# ====== 7. On-Off Flow Schedule Tests ======================================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_single_burst_ipv4
   127.0.0.1 mptcp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_single_burst_ipv6
   ::1       mptcp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_multiburst_ipv4
   127.0.0.1 mptcp const20:const1000:const0:const0:onoff=1,2,3,4 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_delayed_start
   127.0.0.1 mptcp const20:const1000:const0:const0:onoff=2 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_fragmented
   127.0.0.1 mptcp const20:const3000:const0:const0:onoff=1,3:maxmsgsize=1000 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_exp_size
   127.0.0.1 mptcp const20:exp1000:const0:const0:onoff=1,4 plain 6)

# ====== 8. Random Distribution Tests (VBR Flows) ===========================
ADD_NETPERFMETER_FLOW_TEST(dist_mptcp_uniform     127.0.0.1 mptcp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_mptcp_exp         127.0.0.1 mptcp const20:exp1000:const20:exp1000 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_mptcp_truncnormal 127.0.0.1 mptcp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_mptcp_pareto      127.0.0.1 mptcp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)

# ====== 9. MPTCP Multi-Connection & Cross-Protocol Concurrency =============
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_2connections_ipv4
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -mptcp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_4connections_ipv4
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 5
   -mptcp const10:const1000:const10:const1000
   -mptcp const10:const1000:const10:const1000
   -mptcp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_2connections_ipv6
   ::1       mptcp const10:const1000:const10:const1000 plain 3
   -mptcp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_4connections_ipv6
   ::1       mptcp const10:const1000:const10:const1000 plain 5
   -mptcp const10:const1000:const10:const1000
   -mptcp const10:const1000:const10:const1000
   -mptcp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_heterogeneous_rates_sizes
   127.0.0.1 mptcp const25:const100:const25:const100:nodelay=on plain 5
   -mptcp const10:const1400:const10:const1400)

# Asymmetric split: Flow 0 upload-only MPTCP, Flow 1 download-only MPTCP
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_split_upload_download
   127.0.0.1 mptcp const15:const1000:const0:const0 plain 3
   -mptcp const0:const0:const15:const1000)

# Interleaved On-Off bursts across 2 MPTCP connections
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_interleaved_onoff
   127.0.0.1 mptcp const20:const1000:const0:const0:onoff=1,2 plain 4
   -mptcp const20:const1000:const0:const0:onoff=2,3)

# Simultaneous MPTCP (port-1) and regular TCP (port) connections
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_tcp_cross_traffic
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000)

# Competing MPTCP + UDP flows
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_udp_cross_traffic
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -udp const10:const1000:const10:const1000)

# Concurrent MPTCP + TCP + UDP competing flows
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_tcp_udp_cross
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_2connections_ctrl_mptcp
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -mptcp const10:const1000:const10:const1000 --control-over-mptcp)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_2connections_fragmented
   127.0.0.1 mptcp const10:const1000:const10:const1000:maxmsgsize=250 plain 3
   -mptcp const10:const1000:const10:const1000:maxmsgsize=250)


# ===========================================================================
# 50 REALISTIC MPTCP APPLICATION DISTRIBUTION PROFILES (Profiles 201 - 250)
# (RFC 8684 Multipath TCP: Mobile Handover, Hybrid Access & Cloud Bonding)
# ===========================================================================

# ====== 30. Mobile 5G + Wi-Fi Handover & Interactive Voice/Assistants ======
# 201. Mobile Voice Assistant (Siri/Alexa style) Interactive Stream (45 fps, 380 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_voice_assistant
   127.0.0.1 mptcp uniform40,50:uniform280,480:uniform40,50:uniform280,480 plain 8)

# 202. 5G-to-Wi-Fi Seamless VoWiFi/VoNR Handover Stream over IPv6 (50 fps, 280 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_vowifi_handover_v6
   ::1       mptcp uniform48,52:truncnormal280,25:uniform48,52:truncnormal280,25 plain 8)

# 203. Lossless Hi-Res Music Streaming over Dual-Path Cellular+Wi-Fi (55 fps, 1200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_hires_audio_stream
   127.0.0.1 mptcp uniform50,60:uniform1000,1400:uniform50,60:uniform1000,1400 plain 8)

# 204. Mobile Push Notification & Presence Keepalive Channel (10 fps, 160 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_push_keepalive
   127.0.0.1 mptcp uniform8,12:uniform100,220:uniform8,12:uniform100,220 plain 8)

# 205. Dual-Radio Smartphone RCS Rich Messaging Sync over IPv6 (15 fps, Exp mean 700 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_rcs_messaging_v6
   ::1       mptcp uniform12,18:exp700:uniform12,18:exp700 plain 8)

# 206. Mobile Cloud Photo Backup Chunked Upload with Segmentation (12 fps, 3600 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_cloud_backup_frag
   127.0.0.1 mptcp const12:truncnormal3600,250:const12:truncnormal3600,250:maxmsgsize=1200 plain 8)

# 207. Connected Car Infotainment & OTA Map Tile Stream (20 fps, Pareto mean 1500 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_car_infotainment
   127.0.0.1 mptcp const20:pareto1000,3:const20:pareto1000,3 plain 8)

# 208. Mobile Banking Mutual-Auth API over IPv6 (20 fps, 300 B req -> Exp 950 B resp)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_banking_api_v6
   ::1       mptcp uniform15,25:uniform200,400:uniform15,25:exp950 plain 8)


# ====== 31. Hybrid Access Bonding (DSL/Fiber + 5G FWA CPE Gateways) ======
# 209. Hybrid CPE Residential Web & HTTP/2 Browsing Aggregate (35 fps, 350 B -> Exp 1100 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_hybrid_cpe_web_browsing
   127.0.0.1 mptcp uniform30,40:uniform200,500:uniform30,40:exp1100 plain 8)

# 210. Hybrid Access Adaptive ABR Video Streaming over IPv6 (16 fps, Pareto mean 2400 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_hybrid_cpe_abr_video_v6
   ::1       mptcp const16:pareto1600,3:const16:pareto1600,3:maxmsgsize=1200 plain 8)

# 211. SD-WAN Branch Office SaaS ERP/CRM Tunnel Bonding (40 fps, 600 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_sdwan_branch_saas_bond
   127.0.0.1 mptcp uniform35,45:truncnormal600,50:uniform35,45:truncnormal600,50 plain 8)

# 212. SD-WAN Site-to-Site Encrypted Overlay Keepalive & Control (50 fps, 96 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_sdwan_overlay_control
   127.0.0.1 mptcp const50:uniform64,128:const50:uniform64,128 plain 8)

# 213. Remote Work VDI / RDP Virtual Desktop Stream over IPv6 (30 fps, Pareto mean 900 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_hybrid_vdi_desktop_v6
   ::1       mptcp uniform25,35:pareto600,3:uniform25,35:pareto600,3 plain 8)

# 214. Enterprise Cloud Print & Document Spooler Transfer (10 fps, 3000 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_hybrid_cloud_print_frag
   127.0.0.1 mptcp const10:truncnormal3000,200:const10:truncnormal3000,200:maxmsgsize=1000 plain 8)

# 215. Multi-ISP SOCKS/HTTP Proxy Trunking Gateway (45 fps, 750 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_socks_proxy_trunk
   127.0.0.1 mptcp uniform40,50:uniform500,1000:uniform40,50:uniform500,1000 plain 8)


# ====== 32. Data Center Multi-NIC Aggregation & Cloud Storage ==============
# 216. Multi-NIC Distributed Object Storage Replication (25 fps, 3200 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_object_replication_frag
   127.0.0.1 mptcp const25:truncnormal3200,200:const25:truncnormal3200,200:maxmsgsize=1000 plain 8)

# 217. Dual-ToR Leaf-Spine East-West RPC Microservice Mesh over IPv6 (60 fps, 320 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_east_west_rpc_v6
   ::1       mptcp uniform50,70:truncnormal320,30:uniform50,70:truncnormal320,30 plain 8)

# 218. Distributed SQL Read-Replica Binlog Stream (30 fps, 1100 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_sql_binlog_sync
   127.0.0.1 mptcp const30:uniform900,1300:const30:uniform900,1300 plain 8)

# 219. Container Registry Layer Pull Distribution with Chunking (14 fps, Pareto mean 2700 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_container_image_frag
   127.0.0.1 mptcp const14:pareto1800,3:const14:pareto1800,3:maxmsgsize=1200 plain 8)

# 220. Multi-Path Distributed Cache (Redis/KeyDB) Cluster Sync over IPv6 (70 fps, 140 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_cache_cluster_v6
   ::1       mptcp uniform60,80:uniform90,190:uniform60,80:uniform90,190 plain 8)

# 221. Cross-AZ Kafka Partition MirrorMaker Stream (25 fps, 1250 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_kafka_mirrormaker
   127.0.0.1 mptcp uniform20,30:truncnormal1250,80:uniform20,30:truncnormal1250,80 plain 8)

# 222. Dual-Homed Load Balancer Health & Session State Sync (50 fps, 120 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_lb_state_sync
   127.0.0.1 mptcp const50:uniform80,160:const50:uniform80,160 plain 8)


# ====== 33. High-Speed Rail, Maritime & Connected Transport ================
# 223. High-Speed Train Multi-Modem Roof Antenna Wi-Fi Backhaul (30 fps, 900 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_rail_train_wifi_backhaul
   127.0.0.1 mptcp uniform25,35:uniform700,1100:uniform25,35:uniform700,1100 plain 8)

# 224. FRMCS / Railway Wayside Telemetry & Diagnostics over IPv6 (20 fps, 260 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_rail_wayside_telemetry_v6
   ::1       mptcp const20:truncnormal260,25:const20:truncnormal260,25 plain 8)

# 225. Maritime VSAT + 5G Coastal Vessel Hybrid Link (15 fps, 800 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_marine_vsat_5g_hybrid
   127.0.0.1 mptcp uniform12,18:uniform600,1000:uniform12,18:uniform600,1000 plain 8)

# 226. Connected Ambulance Dual-LTE/5G Live Ultrasound & Vitals Stream (25 fps, 1150 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ambulance_telemedicine
   127.0.0.1 mptcp const25:truncnormal1150,90:const25:truncnormal1150,90 plain 8)

# 227. Autonomous Shuttle Multi-Carrier Fleet Telemetry over IPv6 (30 fps, 300 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_shuttle_fleet_telemetry_v6
   ::1       mptcp uniform25,35:uniform200,400:uniform25,35:uniform200,400 plain 8)

# 228. Public Transit Bus CCTV & Passenger Counter Offload (12 fps, Pareto mean 2100 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_transit_cctv_offload_frag
   127.0.0.1 mptcp const12:pareto1400,3:const12:pareto1400,3:maxmsgsize=1000 plain 8)


# ====== 34. Financial Trading, Multi-Homed Exchanges & FinTech =============
# 229. Multi-Homed Algorithmic Order Routing with TCP_NODELAY (80 fps, 140 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_algo_order_routing
   127.0.0.1 mptcp truncnormal80,4:uniform100,180:truncnormal80,4:uniform100,180:nodelay=on plain 8)

# 230. Dual-Carrier FIX Execution Drop-Copy Feed over IPv6 (40 fps, 320 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_fix_drop_copy_v6
   ::1       mptcp uniform35,45:uniform240,400:uniform35,45:uniform240,400 plain 8)

# 231. Retail POS Terminal Dual-WAN (Ethernet + Cellular Failover) Auth (20 fps, 240 B -> Exp 750 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_pos_terminal_auth
   127.0.0.1 mptcp uniform15,25:uniform180,300:uniform15,25:exp750 plain 8)

# 232. ATM Fleet Dual-Path Encrypted Transaction Tunnel (15 fps, 300 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_atm_transaction_tunnel
   127.0.0.1 mptcp const15:truncnormal300,25:const15:truncnormal300,25 plain 8)

# 233. Multi-Exchange Market Data Replay Stream over IPv6 (50 fps, Pareto mean 600 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_market_replay_v6
   ::1       mptcp const50:pareto400,3:const50:pareto400,3 plain 8)


# ====== 35. Industrial Automation, Smart Grid & Mission-Critical IoT =======
# 234. Redundant Dual-LAN PRP/HSR-Style Factory PLC Supervision (40 fps, 200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ind_redundant_plc_bus
   127.0.0.1 mptcp const40:uniform140,260:const40:uniform140,260 plain 8)

# 235. Smart Grid Substation Dual-Uplink IEC 61850 MMS Control over IPv6 (25 fps, 280 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_grid_iec61850_mms_v6
   ::1       mptcp uniform20,30:truncnormal280,25:uniform20,30:truncnormal280,25 plain 8)

# 236. Wind Farm Offshore SCADA Multi-Link Telemetry (15 fps, 450 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_energy_windfarm_scada
   127.0.0.1 mptcp uniform12,18:uniform350,550:uniform12,18:uniform350,550 plain 8)

# 237. Autonomous Mining Haul Truck Dual-Wi-Fi/LTE Command Stream (30 fps, 220 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mining_haul_truck_ctrl
   127.0.0.1 mptcp const30:truncnormal220,20:const30:truncnormal220,20 plain 8)

# 238. High-Resolution Machine Vision Defect Image Upload (8 fps, 3600 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ind_defect_imaging_frag
   127.0.0.1 mptcp const8:truncnormal3600,250:const8:truncnormal3600,250:maxmsgsize=1200 plain 8)


# ====== 36. Edge AI, Cloud Gaming & Interactive Streaming ==================
# 239. Multi-Path Cloud Gaming Video & Input Stream (60 fps, 950 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_cloud_gaming_stream
   127.0.0.1 mptcp truncnormal60,3:uniform800,1100:truncnormal60,3:uniform800,1100:nodelay=on plain 8)

# 240. Mobile AR/VR Edge Offload Split-Rendering Stream over IPv6 (60 fps, 700 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_edge_ar_split_render_v6
   ::1       mptcp uniform55,65:uniform550,850:uniform55,65:uniform550,850 plain 8)

# 241. Distributed AI Model Checkpoint Shard Sync (10 fps, Pareto mean 3000 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ai_checkpoint_sync_frag
   127.0.0.1 mptcp const10:pareto2000,3:const10:pareto2000,3:maxmsgsize=1000 plain 8)

# 242. Edge LLM Token Streaming Inference Gateway over IPv6 (40 fps, 180 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_edge_llm_token_stream_v6
   ::1       mptcp uniform35,45:uniform120,240:uniform35,45:uniform120,240 plain 8)

# 243. Multi-Camera Live Broadcast Bonding Backpack Stream (30 fps, 1250 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_broadcast_bonding_uplink
   127.0.0.1 mptcp const30:truncnormal1250,90:const30:truncnormal1250,90 plain 8)


# ====== 37. Control-Over-MPTCP Realistic Application Profiles ==============
# 244. Control-Over-MPTCP: Mobile Interactive RPC Service (25 fps, 300 B -> Exp 900 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_mobile_rpc
   127.0.0.1 mptcp uniform20,30:uniform200,400:uniform20,30:exp900 plain 8 --control-over-mptcp)

# 245. Control-Over-MPTCP: Dual-Stack IPv6 Cloud Microservice Stream (35 fps, 450 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_cloud_svc_v6
   ::1       mptcp uniform30,40:truncnormal450,40:uniform30,40:truncnormal450,40 plain 8 --control-over-mptcp)

# 246. Control-Over-MPTCP: Hybrid Access Segmented Bulk Sync (12 fps, 3200 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_bulk_frag
   127.0.0.1 mptcp const12:truncnormal3200,200:const12:truncnormal3200,200:maxmsgsize=1000 plain 8 --control-over-mptcp)

# 247. Control-Over-MPTCP: Connected Vehicle Multi-Radio Telemetry (15 fps, 350 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_v2x_telemetry
   127.0.0.1 mptcp uniform12,18:uniform250,450:uniform12,18:uniform250,450 plain 8 --control-over-mptcp)

# 248. Control-Over-MPTCP: Financial Low-Latency Order Channel over IPv6 (60 fps, 140 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_fin_orders_v6
   ::1       mptcp truncnormal60,3:uniform100,180:truncnormal60,3:uniform100,180:nodelay=on plain 8 --control-over-mptcp)

# 249. Control-Over-MPTCP: Media Streaming Pareto Chunk Delivery (15 fps, Pareto mean 1500 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_media_pareto
   127.0.0.1 mptcp const15:pareto1000,3:const15:pareto1000,3 plain 8 --control-over-mptcp)

# 250. Control-Over-MPTCP: Distributed Consensus & Cluster Heartbeat (50 fps, 96 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_cluster_hb
   127.0.0.1 mptcp const50:uniform64,128:const50:uniform64,128 plain 8 --control-over-mptcp)

ENDIF()


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
# ADD_NETPERFMETER_FLOW_TEST(flow_sctp_format_gz  127.0.0.1 sctp const10:const1000:const10:const1000 gz)
ADD_NETPERFMETER_FLOW_TEST(flow_sctp_format_bz2 127.0.0.1 sctp const10:const1000:const10:const1000 bz2)
# ADD_NETPERFMETER_FLOW_TEST(flow_sctp_format_xz  127.0.0.1 sctp const10:const1000:const10:const1000 xz)
# ADD_NETPERFMETER_FLOW_TEST(flow_sctp_format_zst 127.0.0.1 sctp const10:const1000:const10:const1000 zst)

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
# ADD_NETPERFMETER_FLOW_TEST(flow_quic_format_gz     127.0.0.1 quic const10:const1000:const10:const1000 gz 3)
ADD_NETPERFMETER_FLOW_TEST(flow_quic_format_bz2    127.0.0.1 quic const10:const1000:const10:const1000 bz2 3)
# ADD_NETPERFMETER_FLOW_TEST(flow_quic_format_xz     127.0.0.1 quic const10:const1000:const10:const1000 xz 3)
# ADD_NETPERFMETER_FLOW_TEST(flow_quic_format_zst    127.0.0.1 quic const10:const1000:const10:const1000 zst 3)
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

IF (HAVE_SCTP_FOR_TESTS)
   # Control channel using QUIC with SCTP data flow
   ADD_NETPERFMETER_FLOW_TEST(flow_sctp_ctrl_quic_bidi_ipv4
      127.0.0.1 sctp const10:const1000:const10:const1000 plain 3 --control-over-quic)
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


#############################################################################
#### DCCP FLOW INTEGRATION TESTS (RFC 4340)                              ####
#############################################################################

IF (HAVE_DCCP_FOR_TESTS)

# ====== 1. Bidirectional Flows =============================================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_bidi_ipv4 127.0.0.1 dccp const10:const1000:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_bidi_ipv6 ::1       dccp const10:const1000:const10:const1000 plain)

# ====== 2. Upload-Only Flows (Active -> Passive) ===========================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_upload_ipv4 127.0.0.1 dccp const10:const1000:const0:const0 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_upload_ipv6 ::1       dccp const10:const1000:const0:const0 plain)

# ====== 3. Download-Only Flows (Passive -> Active) =========================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_download_ipv4 127.0.0.1 dccp const0:const0:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_download_ipv6 ::1       dccp const0:const0:const10:const1000 plain)

# ====== 4. Default Shorthand, CCID Options & Local Bind ====================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_default_shorthand
   127.0.0.1 dccp default plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ccid2
   127.0.0.1 dccp const10:const1000:const10:const1000:ccid=2 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ccid3
   127.0.0.1 dccp const10:const1000:const10:const1000:ccid=3 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_local_bind_ipv4
   127.0.0.1 dccp const10:const1000:const10:const1000 plain 2
   -L 127.0.0.1 --server-arg -L --server-arg 127.0.0.1)

IF (HAVE_SCTP_FOR_TESTS)
   ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ctrl_sctp_bidi_ipv4
      127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --control-over-sctp)
ENDIF()

IF (HAVE_MPTCP_FOR_TESTS)
   ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ctrl_mptcp_bidi_ipv4
      127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --control-over-mptcp)
ENDIF()

IF (HAVE_QUIC_FOR_TESTS)
   ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ctrl_quic_bidi_ipv4
      127.0.0.1 dccp const10:const1000:const10:const1000 plain 3 --control-over-quic)
ENDIF()

# ====== 5. Compression Formats & Output Suppression ========================
# ADD_NETPERFMETER_FLOW_TEST(flow_dccp_format_gz  127.0.0.1 dccp const10:const1000:const10:const1000 gz)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_format_bz2 127.0.0.1 dccp const10:const1000:const10:const1000 bz2)
# ADD_NETPERFMETER_FLOW_TEST(flow_dccp_format_xz  127.0.0.1 dccp const10:const1000:const10:const1000 xz)
# ADD_NETPERFMETER_FLOW_TEST(flow_dccp_format_zst 127.0.0.1 dccp const10:const1000:const10:const1000 zst)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_no_scalar  127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --no-scalar)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_no_vector  127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_no_outputs 127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --no-scalar --no-vector)

# ====== 6. Framing and Defragmentation (maxmsgsize & defragtimeout) ========
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_framing_small      127.0.0.1 dccp const10:const1000:const10:const1000:maxmsgsize=250  plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_framing_medium     127.0.0.1 dccp const10:const4000:const10:const4000:maxmsgsize=1000 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_framing_large      127.0.0.1 dccp const10:const8000:const10:const8000:maxmsgsize=1200 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_framing_asymmetric 127.0.0.1 dccp const10:const4000:const10:const1000:maxmsgsize=500  plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_defragtimeout      127.0.0.1 dccp const10:const4000:const10:const4000:maxmsgsize=1000:defragtimeout=2500 plain 2)

# ====== 7. On-Off Flow Schedule Tests ======================================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_single_burst_ipv4
   127.0.0.1 dccp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_single_burst_ipv6
   ::1       dccp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_multiburst_ipv4
   127.0.0.1 dccp const20:const1000:const0:const0:onoff=1,2,3,4 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_delayed_start
   127.0.0.1 dccp const20:const1000:const0:const0:onoff=2 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_fragmented
   127.0.0.1 dccp const20:const3000:const0:const0:onoff=1,3:maxmsgsize=1000 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_exp_size
   127.0.0.1 dccp const20:exp1000:const0:const0:onoff=1,4 plain 6)

# ====== 8. Random Distribution Tests (VBR Flows) ===========================
ADD_NETPERFMETER_FLOW_TEST(dist_dccp_uniform     127.0.0.1 dccp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_dccp_exp         127.0.0.1 dccp const20:exp1000:const20:exp1000 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_dccp_truncnormal 127.0.0.1 dccp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_dccp_pareto      127.0.0.1 dccp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)

# ====== 9. Multi-Connection & Cross-Protocol Concurrency ===================
ADD_NETPERFMETER_FLOW_TEST(flow_multi_dccp_2connections_ipv4
   127.0.0.1 dccp const10:const1000:const10:const1000 plain 3
   -dccp const10:const1000:const10:const1000)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_dccp_2connections_ipv6
   ::1       dccp const10:const1000:const10:const1000 plain 3
   -dccp const10:const1000:const10:const1000)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_dccp_tcp_udp_cross
   127.0.0.1 dccp const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)


# ===========================================================================
# 50 REALISTIC DCCP APPLICATION DISTRIBUTION PROFILES (Profiles 251 - 300)
# (RFC 4340 DCCP, RFC 4341 CCID-2 TCP-like, RFC 4342 CCID-3 TFRC)
# ===========================================================================

# ====== 38. Real-Time Audio, VoIP & Interactive Voice ======================
# 251. VoIP G.711 Narrowband Speech over DCCP CCID-3 (~50 fps, 200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_g711_narrowband
   127.0.0.1 dccp uniform45,55:uniform160,240:uniform45,55:uniform160,240:ccid=3 plain 8)

# 252. VoIP G.729 Low-Bitrate Voice over IPv6 (~50 fps, 60 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_g729_lowbitrate_v6
   ::1       dccp uniform45,55:uniform40,80:uniform45,55:uniform40,80:ccid=3 plain 8)

# 253. High-Fidelity Wideband Opus Voice with TFRC CCID-3 (50 fps, 300 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_opus_wideband
   127.0.0.1 dccp uniform48,52:truncnormal300,30:uniform48,52:truncnormal300,30:ccid=3 plain 8)

# 254. Fullband Stereo Audio Contribution Stream (50 fps, 500 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_audio_opus_stereo_vbr
   127.0.0.1 dccp const50:uniform400,600:const50:uniform400,600:ccid=3 plain 8)

# 255. Live Radio Broadcast AAC-ELD Audio Feed (40 fps, 850 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_audio_radio_aac_eld
   127.0.0.1 dccp uniform35,45:truncnormal850,70:uniform35,45:truncnormal850,70:ccid=3 plain 8)

# 256. Multi-Party Voice Conference Mixer Uplink over IPv6 (50 fps, 420 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_conference_mixer_v6
   ::1       dccp const50:truncnormal420,35:const50:truncnormal420,35:ccid=3 plain 8)

# 257. Push-to-Talk Tactical Dispatch Voice Channel (30 fps, 240 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_ptt_dispatch
   127.0.0.1 dccp uniform25,35:uniform180,300:uniform25,35:uniform180,300 plain 8)

# 258. Smart Speaker Cloud Voice Assistant Audio Stream over IPv6 (45 fps, 380 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voice_assistant_uplink_v6
   ::1       dccp uniform40,50:uniform280,480:uniform40,50:uniform280,480:ccid=3 plain 8)


# ====== 39. Live Video Streaming, Surveillance & Media Ingest ==============
# 259. Adaptive Low-Latency Live Video Stream CCID-3 (30 fps, Pareto mean 1500 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_video_live_tfrc_frag
   127.0.0.1 dccp const30:pareto1000,3:const30:pareto1000,3:ccid=3:maxmsgsize=1200 plain 8)

# 260. CCTV Security Camera RTSP/DCCP Stream (25 fps, 1150 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cctv_rtsp_stream
   127.0.0.1 dccp truncnormal25,2:uniform1000,1300:truncnormal25,2:uniform1000,1300:ccid=3 plain 8)

# 261. H.265 IP Surveillance Feed over IPv6 (30 fps, Pareto mean 1200 B)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cctv_h265_surveillance_v6
   ::1       dccp const30:pareto800,3:const30:pareto800,3:ccid=3 plain 8)

# 262. WebRTC Simulcast Video Layer over DCCP CCID-3 (30 fps, 950 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_webrtc_video_layer
   127.0.0.1 dccp uniform25,35:uniform750,1150:uniform25,35:uniform750,1150:ccid=3 plain 8)

# 263. Drone FPV Low-Latency Video Downlink (60 fps, 750 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_drone_fpv_video_feed
   127.0.0.1 dccp truncnormal60,3:uniform650,850:truncnormal60,3:uniform650,850:ccid=3 plain 8)

# 264. Interactive Telehealth Video Consultation over IPv6 (30 fps, 1100 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_med_telehealth_video_v6
   ::1       dccp const30:uniform950,1250:const30:uniform950,1250:ccid=3 plain 8)

# 265. Video Doorbell Bidirectional Media Stream (24 fps, 900 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_home_doorbell_media
   127.0.0.1 dccp const24:uniform750,1050:const24:uniform750,1050 plain 8)

# 266. Remote Desktop Screen Delta Tile Stream CCID-2 (20 fps, Pareto mean 750 B)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_screen_sharing_vbr
   127.0.0.1 dccp uniform15,25:pareto500,3:uniform15,25:pareto500,3:ccid=2 plain 8)


# ====== 40. Multiplayer Gaming, Cloud Gaming & AR/VR Pose Telemetry ========
# 267. 128-Tick Competitive FPS Game State Updates (128 fps, 130 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_tick_128hz
   127.0.0.1 dccp truncnormal128,4:uniform80,180:truncnormal128,4:uniform80,180:ccid=2 plain 8)

# 268. Standard 64-Tick Multiplayer Game Server (64 fps, 150 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_tick_64hz
   127.0.0.1 dccp truncnormal64,3:uniform100,200:truncnormal64,3:uniform100,200 plain 8)

# 269. Console Cross-Platform State Sync over IPv6 (30 fps, 180 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_tick_30hz_v6
   ::1       dccp truncnormal30,2:uniform120,240:truncnormal30,2:uniform120,240 plain 8)

# 270. MMO Virtual World Entity Position Updates (20 fps, 400 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_mmo_worldsync
   127.0.0.1 dccp uniform15,25:uniform200,600:uniform15,25:uniform200,600:ccid=2 plain 8)

# 271. Rigid-Body Physics Synchronization Stream (60 fps, 150 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_physics_sync
   127.0.0.1 dccp const60:truncnormal150,15:const60:truncnormal150,15 plain 8)

# 272. Cloud Gaming Frame & Controller Stream CCID-3 (60 fps, 900 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cloud_gaming_stream
   127.0.0.1 dccp truncnormal60,3:uniform750,1050:truncnormal60,3:uniform750,1050:ccid=3 plain 8)

# 273. AR/VR 6-DoF Head/Hand Spatial Pose Tracking over IPv6 (90 fps, 120 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ar_pose_tracking_v6
   ::1       dccp truncnormal90,4:uniform90,150:truncnormal90,4:uniform90,150:ccid=3 plain 8)

# 274. Bilateral Telepresence Haptic Force Feedback Loop (~100 fps, ~100 B, realistic input jitter)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_haptic_feedback_loop
   127.0.0.1 dccp truncnormal100,3:uniform90,110:truncnormal100,3:uniform90,110 plain 8)


# ====== 41. Connected Vehicles (C-V2X), Aerospace & Autonomous Drones ======
# 275. Connected Vehicle CAM Safety Beacons over IPv6 (10 fps, 350 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_v2x_cam_safety_v6
   ::1       dccp truncnormal10,1:uniform200,500:truncnormal10,1:uniform200,500 plain 8)

# 276. Road Hazard DENM Decentralized Notification (8 fps, 450 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_v2x_denm_hazard
   127.0.0.1 dccp const8:uniform300,600:const8:uniform300,600 plain 8)

# 277. Satellite CCSDS Telemetry Downlink CCID-3 (20 fps, 750 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_aero_satellite_downlink
   127.0.0.1 dccp const20:uniform500,1000:const20:uniform500,1000:ccid=3 plain 8)

# 278. LEO Inter-Satellite Link Routing Beacon over IPv6 (15 fps, 150 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_aero_isl_routing_v6
   ::1       dccp uniform10,20:uniform100,200:uniform10,20:uniform100,200 plain 8)

# 279. ADS-B Mode-S Aircraft Position Transponder (15 fps, 170 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_aero_adsb_transponder
   127.0.0.1 dccp const15:uniform120,220:const15:uniform120,220 plain 8)

# 280. Primary Radar Target Plot Correlation Feed (50 fps, 400 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_aero_radar_track_sync
   127.0.0.1 dccp truncnormal50,4:uniform300,500:truncnormal50,4:uniform300,500:ccid=2 plain 8)

# 281. Autonomous Drone Swarm Attitude & Telemetry Stream (40 fps, 240 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_uav_swarm_telemetry
   127.0.0.1 dccp uniform35,45:truncnormal240,25:uniform35,45:truncnormal240,25:ccid=3 plain 8)


# ====== 42. Financial Market Data Feeds, Order Books & Risk Telemetry ======
# 282. ITCH/OUCH Low-Latency Market Depth Feed CCID-2 (80 fps, 96 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_itch_feed
   127.0.0.1 dccp truncnormal80,4:uniform64,128:truncnormal80,4:uniform64,128:ccid=2 plain 8)

# 283. Crypto Perpetual Futures Order Book Stream over IPv6 (40 fps, Pareto mean 600 B)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_crypto_l2_book_v6
   ::1       dccp const40:pareto400,3:const40:pareto400,3:ccid=2 plain 8)

# 284. Multi-Bank FX ECN Spot Quotation Pipeline (70 fps, 130 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_forex_spot_quotes
   127.0.0.1 dccp truncnormal70,5:uniform100,160:truncnormal70,5:uniform100,160:ccid=2 plain 8)

# 285. Centralized Pre-Trade Risk Heartbeat (60 fps, 64 B, const)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_risk_heartbeat
   127.0.0.1 dccp const60:const64:const60:const64 plain 8)

# 286. Trade Allocation Drop Copy Execution Feed over IPv6 (25 fps, 500 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_drop_copy_feed_v6
   ::1       dccp uniform20,30:uniform350,650:uniform20,30:uniform350,650:ccid=2 plain 8)

# 287. Derivatives Volatility Surface & Greeks Feed (25 fps, Exp mean 550 B)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_options_greeks_feed
   127.0.0.1 dccp const25:exp550:const25:exp550:ccid=3 plain 8)

# 288. Exchange Market Data Replay Multicast-Replacement Feed (50 fps, 320 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_market_replay
   127.0.0.1 dccp const50:truncnormal320,30:const50:truncnormal320,30:ccid=3 plain 8)


# ====== 43. Industrial IoT, Smart Grid & Healthcare Telemetry ==============
# 289. Robot Arm 6-DoF Kinematics Telemetry over IPv6 (100 fps, 150 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ind_robot_kinematics_v6
   ::1       dccp truncnormal100,5:uniform120,180:truncnormal100,5:uniform120,180 plain 8)

# 290. Automated Guided Vehicle (AGV) LIDAR Point Cloud (40 fps, 1150 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ind_agv_lidar_stream
   127.0.0.1 dccp uniform35,45:uniform1000,1300:uniform35,45:uniform1000,1300:ccid=3 plain 8)

# 291. IEEE C37.118 Synchrophasor PMU Grid Frequency Stream (50 fps, 140 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_smartgrid_pmu_c37_118
   127.0.0.1 dccp const50:truncnormal140,10:const50:truncnormal140,10:ccid=3 plain 8)

# 292. Smart Grid Substation Telemetry over IPv6 (15 fps, 240 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_smartgrid_substation_v6
   ::1       dccp uniform12,18:uniform180,300:uniform12,18:uniform180,300 plain 8)

# 293. Continuous Bedside Multi-Lead ECG Waveform Telemetry (50 fps, 200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_med_ecg_telemetry
   127.0.0.1 dccp const50:uniform150,250:const50:uniform150,250:ccid=3 plain 8)

# 294. Critical Care ICU Central Alarm & Vitals Broadcast (15 fps, 300 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_med_icu_vitals_stream
   127.0.0.1 dccp const15:truncnormal300,25:const15:truncnormal300,25 plain 8)


# ====== 44. Cloud Observability, Edge AI & Segmented Datagram Framing ======
# 295. Linux eBPF Continuous Kernel Observability Trace Feed (65 fps, 300 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cloud_ebpf_observability
   127.0.0.1 dccp uniform50,80:uniform200,400:uniform50,80:uniform200,400:ccid=2 plain 8)

# 296. Enterprise SIEM Syslog Event Pipeline (50 fps, 400 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cloud_siem_syslog
   127.0.0.1 dccp uniform40,60:uniform250,550:uniform40,60:uniform250,550:ccid=2 plain 8)

# 297. Edge AI Object Detection Bounding-Box Metadata Stream over IPv6 (30 fps, 360 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ai_edge_detections_v6
   ::1       dccp const30:truncnormal360,30:const30:truncnormal360,30:ccid=3 plain 8)

# 298. Volumetric 3D Point-Cloud Frame Streaming with Segmentation (12 fps, 3000 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_volumetric_pointcloud_frag
   127.0.0.1 dccp const12:truncnormal3000,200:const12:truncnormal3000,200:ccid=3:maxmsgsize=1000 plain 8)

# 299. Predictive Maintenance FFT Vibration Spectra with Segmentation (10 fps, Pareto mean 2100 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ind_fft_spectra_frag
   127.0.0.1 dccp const10:pareto1400,3:const10:pareto1400,3:ccid=2:maxmsgsize=1000:defragtimeout=2500 plain 8)

# 300. Encrypted Datagram Tunnel Encapsulation Flow over IPv6 (45 fps, 550 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_datagram_tunnel_encap_v6
   ::1       dccp uniform40,50:uniform400,700:uniform40,50:uniform400,700:ccid=3 plain 8)

ENDIF()


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
