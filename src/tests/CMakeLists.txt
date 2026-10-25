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
SET(TEST_BASE_PORT 10000 CACHE STRING "Initial base port number for automated flow tests")
SET_PROPERTY(GLOBAL PROPERTY NPM_TEST_PORT_COUNTER ${TEST_BASE_PORT})

# Helper to fetch and advance the port counter
FUNCTION(GET_NEXT_TEST_PORT OUT_PORT)
   GET_PROPERTY(CURRENT_PORT GLOBAL PROPERTY NPM_TEST_PORT_COUNTER)
   SET(${OUT_PORT} ${CURRENT_PORT} PARENT_SCOPE)
   MATH(EXPR NEXT_PORT "${CURRENT_PORT} + 10")
   SET_PROPERTY(GLOBAL PROPERTY NPM_TEST_PORT_COUNTER ${NEXT_PORT})
ENDFUNCTION()


# ###### Helper for deterministic commands (expected return code: 0) #######
FUNCTION(ADD_NETPERFMETER_TEST TEST_NAME)
   ADD_TEST(
      NAME ${TEST_NAME}
      COMMAND $<TARGET_FILE:netperfmeter> ${ARGN}
   )
   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:"
      LABELS "cli"
   )
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
      LABELS "negative;error"
   )
ENDFUNCTION()


# ###### Helper for parameterized flow tests ################################
# Signature:
#   ADD_NETPERFMETER_FLOW_TEST(TEST_NAME ADDRESS PROTOCOL SPEC FORMAT [RUNTIME] [EXTRA_ARGS...])
FUNCTION(ADD_NETPERFMETER_FLOW_TEST TEST_NAME ADDRESS PROTOCOL SPEC FORMAT)
   SET(RUNTIME "2")
   IF(${ARGC} GREATER 5)
      SET(RUNTIME "${ARGV5}")
   ENDIF()

   # Allocate a dedicated base port window for this test
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
         ${ARGN}
   )

   SET(TEST_LABELS "flow;${PROTOCOL};${FORMAT};integration")
   IF(ADDRESS STREQUAL "::1")
      LIST(APPEND TEST_LABELS "ipv6")
   ELSE()
      LIST(APPEND TEST_LABELS "ipv4")
   ENDIF()

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

# --- Version Checks (-v, --version) ----------------------------------------
ADD_NETPERFMETER_TEST(cli_version_short -v)
ADD_NETPERFMETER_TEST(cli_version_long  --version)

SET_TESTS_PROPERTIES(cli_version_short cli_version_long PROPERTIES
   PASS_REGULAR_EXPRESSION "NetPerfMeter [0-9]+\\.[0-9]+\\.[0-9]+"
)

# --- Help / Usage Checks (-h, --help) --------------------------------------
ADD_NETPERFMETER_TEST(cli_help_short -h)
ADD_NETPERFMETER_TEST(cli_help_long  --help)

SET_TESTS_PROPERTIES(cli_help_short cli_help_long PROPERTIES
   PASS_REGULAR_EXPRESSION "Usage:"
)


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
