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
   IF(SPEC MATCHES "tc=")
      LIST(APPEND TEST_LABELS "qos;tc")
   ENDIF()
   IF(SPEC MATCHES "fl=")
      LIST(APPEND TEST_LABELS "qos;flowlabel")
   ENDIF()
   IF(SPEC MATCHES "hb(interval|pathmaxrtx|assocmaxrtx)=")
      LIST(APPEND TEST_LABELS "heartbeats")
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
      ELSEIF(ARG STREQUAL "-Q" OR ARG STREQUAL "--control-tc")
         LIST(APPEND TEST_LABELS "qos;tc")
      ELSEIF(ARG STREQUAL "-F" OR ARG STREQUAL "--control-fl")
         LIST(APPEND TEST_LABELS "qos;flowlabel")
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
   IF(SCENARIO MATCHES "sequential")
      LIST(APPEND TEST_LABELS "sequential-reuse")
   ENDIF()
   IF(SCENARIO MATCHES "multistream")
      LIST(APPEND TEST_LABELS "multistream")
   ENDIF()
   IF(SCENARIO MATCHES "sctp")
      LIST(APPEND TEST_LABELS "sctp")
   ENDIF()
   IF(SCENARIO MATCHES "quic")
      LIST(APPEND TEST_LABELS "quic")
   ENDIF()
   IF(SCENARIO MATCHES "mptcp")
      LIST(APPEND TEST_LABELS "mptcp")
   ENDIF()
   IF(SCENARIO MATCHES "udp")
      LIST(APPEND TEST_LABELS "udp")
   ENDIF()
   IF(SCENARIO MATCHES "qos")
      LIST(APPEND TEST_LABELS "qos;tc")
   ENDIF()
   IF(SCENARIO MATCHES "sigkill")
      LIST(APPEND TEST_LABELS "abort-regression")
   ENDIF()

   SET(TEST_TIMEOUT 60)
   IF(SCENARIO MATCHES "sequential|staggered")
      SET(TEST_TIMEOUT 90)
   ENDIF()

   SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:;Avbrutt;Aborted;assure;MessageReader already has a state"
      LABELS "${TEST_LABELS}"
      TIMEOUT ${TEST_TIMEOUT}
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
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;FAIL:;assure"
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
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;FAIL:;assure"
      LABELS "${TEST_LABELS}"
      TIMEOUT 120
   )
ENDFUNCTION()
