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
   FOREACH(ARG ${EXTRA_TEST_ARGS})
      IF(ARG MATCHES "^--no-(.*)")
         LIST(APPEND TEST_LABELS "no-${CMAKE_MATCH_1}")
      ENDIF()
   ENDFOREACH()

   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:"
      LABELS "${TEST_LABELS}"
      TIMEOUT 30
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
