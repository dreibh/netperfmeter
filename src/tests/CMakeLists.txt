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

# Shared test helpers, port allocation, and CTest property macros
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_common.cmake")

# 1. CLI flags, negative/error handling, and utility tool tests
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_cli_and_utils.cmake")

# 2. TCP and UDP integration, QoS, distribution, on-off, and multi-flow tests
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_tcp_udp.cmake")
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_profiles_tcp_udp.cmake")

# 3. Protocol-specific integration tests and application profiles
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_mptcp.cmake")
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_sctp.cmake")
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_quic.cmake")
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_dccp.cmake")

# 4. Saturated bulk transfer tests with multi-message framing
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_saturated.cmake")

# 5. Server abort regressions, multi-client / sequential reuse, and edge cases
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/tests_sessions_and_edge_cases.cmake")
