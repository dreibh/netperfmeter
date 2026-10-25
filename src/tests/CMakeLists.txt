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
