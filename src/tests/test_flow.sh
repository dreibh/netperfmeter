#!/bin/sh
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

# Usage:
#   test_flow.sh <netperfmeter_bin> <protocol> <spec> [runtime] [extra_args...]

set -eu

NETPERFMETER="${1:-netperfmeter}"
PROTOCOL="${2:-tcp}"
SPEC="${3:-const10:const1000:const10:const1000}"
RUNTIME="${4:-2}"

# Shift off the first 4 arguments; remaining arguments are passed to the client
if [ "$#" -ge 4 ]; then
    shift 4
    EXTRA_ARGS="$*"
else
    EXTRA_ARGS=""
fi

# Pick an unprivileged, even base port using PID to avoid collisions in parallel runs
PORT=$((20000 + ($$ % 10000) * 2))

# Temporary directory for output files and logs
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_test')

SERVER_PID=""
cleanup() {
    if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill -TERM "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT INT TERM

# 1. Start passive node (server) listening on PORT (control on PORT+1 via TCP)
"${NETPERFMETER}" "${PORT}" -no-control-over-sctp >"${TMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

# Allow listening sockets to bind and initialize
sleep 0.5

# 2. Start active node (client) with specified protocol and traffic specification
if ! "${NETPERFMETER}" "127.0.0.1:${PORT}" \
    -control-over-tcp \
    -runtime="${RUNTIME}" \
    "-${PROTOCOL}" "${SPEC}" \
    -scalar="${TMP_DIR}/test.sca" \
    -vector="${TMP_DIR}/test.vec" \
    ${EXTRA_ARGS} \
    >"${TMP_DIR}/client.log" 2>&1; then
    echo "FAIL: Active instance exited with an error." >&2
    echo "--- Client Log ---" >&2
    cat "${TMP_DIR}/client.log" >&2
    echo "--- Server Log ---" >&2
    cat "${TMP_DIR}/server.log" >&2
    exit 1
fi

# 3. Verify output files were generated and are non-empty
if [ ! -s "${TMP_DIR}/test-active.sca" ] || [ ! -s "${TMP_DIR}/test-passive.sca" ]; then
    echo "FAIL: Scalar output file(s) missing or empty." >&2
    exit 1
fi

# Verify at least one vector file was generated on each side
count_active_vec=$(ls -1 "${TMP_DIR}"/test-active-*.vec 2>/dev/null | wc -l)
count_passive_vec=$(ls -1 "${TMP_DIR}"/test-passive-*.vec 2>/dev/null | wc -l)
if [ "${count_active_vec}" -eq 0 ] || [ "${count_passive_vec}" -eq 0 ]; then
    echo "FAIL: Vector output file(s) missing." >&2
    exit 1
fi

# 4. Verify packet transmission and reception in the scalar summary
grep -q "Transmitted Packets" "${TMP_DIR}/test-active.sca"
grep -q "Received Packets" "${TMP_DIR}/test-passive.sca"

echo "PASS: ${PROTOCOL} flow test (${SPEC}, runtime: ${RUNTIME}s) completed successfully."
exit 0
