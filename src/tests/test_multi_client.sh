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
#   test_multi_client.sh <bin> <port> <addr> <scenario> [extra_args...]
#
# Scenarios:
#   concurrent_2clients_udp_tcp   - 2 simultaneous clients (C1: TCP+UDP, C2: 2x UDP)
#   staggered_overlap_udp_tcp     - C1 runs 4s, C2 starts at +1s for 2s (tears down while C1 is active)
#   sequential_session_reuse      - C1, C2, C3 run back-to-back against the same server
#   concurrent_3clients_mixed     - 3 simultaneous clients (SCTP multistream, TCP+UDP, 2x UDP)
#   concurrent_2clients_ipv6      - 2 simultaneous clients over IPv6 [::1]

set -eu

NETPERFMETER="${1:-netperfmeter}"
PORT="${2:-26000}"
ADDRESS="${3:-127.0.0.1}"
SCENARIO="${4:-concurrent_2clients_udp_tcp}"
shift 4 2>/dev/null || true

CONTROL_PORT=$((PORT + 1))
case "${ADDRESS}" in
    *:*) TARGET="[${ADDRESS}]" ;;
    *)   TARGET="${ADDRESS}" ;;
esac

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_multi_test')
SERVER_PID=""

cleanup_exit() {
    if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill -INT "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    rm -rf "${TMP_DIR}"
}
trap cleanup_exit EXIT INT TERM

wait_for_listener() {
    _host="$1"
    _port="$2"
    _proto="${3:-tcp}"
    _timeout="${4:-5}"
    _elapsed=0

    while [ "$_elapsed" -lt "$_timeout" ]; do
        if [ -n "${SERVER_PID}" ] && ! kill -0 "${SERVER_PID}" 2>/dev/null; then
            echo "FAIL: Passive server process (PID ${SERVER_PID}) terminated early." >&2
            if [ -s "${TMP_DIR}/server.log" ]; then
                cat "${TMP_DIR}/server.log" >&2
            fi
            return 1
        fi

        if [ "${_proto}" = "sctp" ]; then
            if perl -e '
                use strict; use warnings;
                use Socket qw(AF_INET AF_INET6 SOCK_STREAM sockaddr_in sockaddr_in6 inet_aton inet_pton);
                my ($host, $port) = @ARGV;
                my $is_v6 = ($host =~ /:/);
                my $family = $is_v6 ? AF_INET6 : AF_INET;
                my $sock;
                if (socket($sock, $family, 1, 132)) {
                    my $addr = $is_v6 ? sockaddr_in6($port, inet_pton(AF_INET6, $host))
                                      : sockaddr_in($port, inet_aton($host));
                    if (connect($sock, $addr)) { close($sock); exit 0; }
                    close($sock);
                }
                exit 1;
            ' "$_host" "$_port" 2>/dev/null; then
                return 0
            fi
        else
            if perl -MIO::Socket::IP -e \
               'exit(!IO::Socket::IP->new(PeerHost => $ARGV[0], PeerPort => $ARGV[1], Proto => "tcp", Timeout => 1))' \
               "$_host" "$_port" 2>/dev/null; then
                return 0
            fi
        fi
        sleep 1
        _elapsed=$((_elapsed + 1))
    done

    echo "FAIL: Passive server failed to listen on ${_proto}://${_host}:${_port} within ${_timeout}s." >&2
    return 1
}

verify_scalar_file() {
    _file="$1"
    _label="$2"
    if [ ! -s "${_file}" ]; then
        echo "FAIL: ${_label} scalar file '${_file}' is missing or empty." >&2
        return 1
    fi
    if ! grep -q 'scalar "netPerfMeter\.' "${_file}"; then
        echo "FAIL: ${_label} scalar file '${_file}' contains invalid format." >&2
        return 1
    fi
}

stop_and_verify_server() {
    _srv_rc=0
    if [ -n "${SERVER_PID}" ]; then
        if kill -0 "${SERVER_PID}" 2>/dev/null; then
            kill -INT "${SERVER_PID}" 2>/dev/null || true
            wait "${SERVER_PID}" 2>/dev/null || _srv_rc=$?
        else
            wait "${SERVER_PID}" 2>/dev/null || _srv_rc=$?
        fi
        SERVER_PID=""

        if [ "${_srv_rc}" -ne 0 ]; then
            echo "FAIL: Passive server instance crashed on shutdown with exit code ${_srv_rc}." >&2
            if [ -s "${TMP_DIR}/server.log" ]; then
                echo "--- Server Log ---" >&2
                cat "${TMP_DIR}/server.log" >&2
            fi
            return 1
        fi

        if [ -s "${TMP_DIR}/server.log" ] && grep -qE "assure\(|assert\(|failed in|FATAL ERROR|Aborted|Avbrutt" "${TMP_DIR}/server.log"; then
            echo "FAIL: Assertion failure or crash message detected in server log." >&2
            echo "--- Server Log ---" >&2
            cat "${TMP_DIR}/server.log" >&2
            return 1
        fi
    fi
    return 0
}

# ====== 1. Start Server ====================================================
SERVER_CONTROL_ARGS="-control-over-tcp"
CTRL_PROTO="tcp"
if [ "${SCENARIO}" = "concurrent_3clients_mixed" ]; then
    SERVER_CONTROL_ARGS="-control-over-tcp -control-over-sctp"
fi

"${NETPERFMETER}" "${PORT}" ${SERVER_CONTROL_ARGS} >"${TMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "${CTRL_PROTO}" 5; then
    stop_and_verify_server || true
    exit 1
fi

# ====== 2. Execute Scenario ================================================
case "${SCENARIO}" in
    # -----------------------------------------------------------------------
    # Scenario A: 2 Clients running simultaneously (Mixed TCP + 3x UDP)
    # -----------------------------------------------------------------------
    concurrent_2clients_udp_tcp)
        echo "Running Scenario: 2 concurrent clients (TCP + UDP)"
        # Client 1: 1 TCP flow + 1 UDP flow
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 \
            -scalar="${TMP_DIR}/c1.sca" \
            -tcp const10:const1000:const10:const1000 \
            -udp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/c1.log" 2>&1 &
        C1_PID=$!

        # Client 2: 2 UDP flows with different rates and sizes
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 \
            -scalar="${TMP_DIR}/c2.sca" \
            -udp const20:const500:const20:const500 \
            -udp const5:const1200:const5:const1200 \
            >"${TMP_DIR}/c2.log" 2>&1 &
        C2_PID=$!

        C1_RC=0; C2_RC=0
        wait "${C1_PID}" || C1_RC=$?
        wait "${C2_PID}" || C2_RC=$?

        if [ "${C1_RC}" -ne 0 ] || [ "${C2_RC}" -ne 0 ]; then
            echo "FAIL: One or more clients exited with non-zero status (C1=${C1_RC}, C2=${C2_RC})" >&2
            cat "${TMP_DIR}/c1.log" "${TMP_DIR}/c2.log" >&2
            stop_and_verify_server || true
            exit 1
        fi

        verify_scalar_file "${TMP_DIR}/c1-active.sca" "Client 1 Active"
        verify_scalar_file "${TMP_DIR}/c1-passive.sca" "Client 1 Passive"
        verify_scalar_file "${TMP_DIR}/c2-active.sca" "Client 2 Active"
        verify_scalar_file "${TMP_DIR}/c2-passive.sca" "Client 2 Passive"
        ;;

    # -----------------------------------------------------------------------
    # Scenario B: Staggered Overlapping Lifetimes
    # C1 starts at 0s (runtime 4s). C2 starts at +1s (runtime 2s).
    # C2 terminates and tears down its session at 3s while C1 is actively running!
    # -----------------------------------------------------------------------
    staggered_overlap_udp_tcp)
        echo "Running Scenario: Staggered overlapping clients (mid-flight session teardown)"
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -scalar="${TMP_DIR}/c1.sca" \
            -tcp const10:const1000:const10:const1000 \
            -udp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/c1.log" 2>&1 &
        C1_PID=$!

        sleep 1

        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 \
            -scalar="${TMP_DIR}/c2.sca" \
            -udp const20:const500:const20:const500 \
            -udp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/c2.log" 2>&1 &
        C2_PID=$!

        C1_RC=0; C2_RC=0
        wait "${C2_PID}" || C2_RC=$?
        wait "${C1_PID}" || C1_RC=$?

        if [ "${C1_RC}" -ne 0 ] || [ "${C2_RC}" -ne 0 ]; then
            echo "FAIL: Staggered clients exited with error (C1=${C1_RC}, C2=${C2_RC})" >&2
            cat "${TMP_DIR}/c1.log" "${TMP_DIR}/c2.log" >&2
            stop_and_verify_server || true
            exit 1
        fi

        verify_scalar_file "${TMP_DIR}/c1-active.sca" "Client 1 Active"
        verify_scalar_file "${TMP_DIR}/c1-passive.sca" "Client 1 Passive"
        verify_scalar_file "${TMP_DIR}/c2-active.sca" "Client 2 Active"
        verify_scalar_file "${TMP_DIR}/c2-passive.sca" "Client 2 Passive"
        ;;

    # -----------------------------------------------------------------------
    # Scenario C: Sequential Session Reuse (C1 -> C2 -> C3 back-to-back)
    # Verifies server resets its state and reference counts cleanly between sessions
    # -----------------------------------------------------------------------
    sequential_session_reuse)
        echo "Running Scenario: Sequential session reuse (3 generations)"
        # Gen 1: TCP
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 \
            -scalar="${TMP_DIR}/c1.sca" \
            -tcp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/c1.log" 2>&1

        # Gen 2: 2x UDP
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 \
            -scalar="${TMP_DIR}/c2.sca" \
            -udp const15:const800:const15:const800 \
            -udp const15:const800:const15:const800 \
            >"${TMP_DIR}/c2.log" 2>&1

        # Gen 3: TCP + UDP
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 \
            -scalar="${TMP_DIR}/c3.sca" \
            -tcp const15:const1000:const15:const1000 \
            -udp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/c3.log" 2>&1

        verify_scalar_file "${TMP_DIR}/c1-active.sca" "Gen 1 Active"
        verify_scalar_file "${TMP_DIR}/c2-active.sca" "Gen 2 Active"
        verify_scalar_file "${TMP_DIR}/c3-active.sca" "Gen 3 Active"
        ;;

    # -----------------------------------------------------------------------
    # Scenario D: 3 Concurrent Clients with Tri-Protocol Stress (SCTP + TCP + UDP)
    # -----------------------------------------------------------------------
    concurrent_3clients_mixed)
        echo "Running Scenario: 3 concurrent clients (SCTP multistream, TCP+UDP, 2x UDP)"
        # Client 1: SCTP multistream (2 streams in 1 association)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 \
            -scalar="${TMP_DIR}/c1.sca" \
            -sctp const10:const1000:const10:const1000 const15:const500:const15:const500 \
            >"${TMP_DIR}/c1.log" 2>&1 &
        C1_PID=$!

        # Client 2: TCP + UDP
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 \
            -scalar="${TMP_DIR}/c2.sca" \
            -tcp const10:const1000:const10:const1000 \
            -udp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/c2.log" 2>&1 &
        C2_PID=$!

        # Client 3: 2x UDP
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 \
            -scalar="${TMP_DIR}/c3.sca" \
            -udp const20:const500:const20:const500 \
            -udp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/c3.log" 2>&1 &
        C3_PID=$!

        C1_RC=0; C2_RC=0; C3_RC=0
        wait "${C1_PID}" || C1_RC=$?
        wait "${C2_PID}" || C2_RC=$?
        wait "${C3_PID}" || C3_RC=$?

        if [ "${C1_RC}" -ne 0 ] || [ "${C2_RC}" -ne 0 ] || [ "${C3_RC}" -ne 0 ]; then
            echo "FAIL: Multi-client session exited with error (C1=${C1_RC}, C2=${C2_RC}, C3=${C3_RC})" >&2
            cat "${TMP_DIR}/c1.log" "${TMP_DIR}/c2.log" "${TMP_DIR}/c3.log" >&2
            stop_and_verify_server || true
            exit 1
        fi

        verify_scalar_file "${TMP_DIR}/c1-active.sca" "Client 1 Active"
        verify_scalar_file "${TMP_DIR}/c1-passive.sca" "Client 1 Passive"
        verify_scalar_file "${TMP_DIR}/c2-active.sca" "Client 2 Active"
        verify_scalar_file "${TMP_DIR}/c2-passive.sca" "Client 2 Passive"
        verify_scalar_file "${TMP_DIR}/c3-active.sca" "Client 3 Active"
        verify_scalar_file "${TMP_DIR}/c3-passive.sca" "Client 3 Passive"
        ;;

    # -----------------------------------------------------------------------
    # Scenario E: 2 Clients running simultaneously over IPv6
    # -----------------------------------------------------------------------
    concurrent_2clients_ipv6)
        echo "Running Scenario: 2 concurrent clients over IPv6"
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 \
            -scalar="${TMP_DIR}/c1.sca" \
            -tcp const10:const1000:const10:const1000 \
            -udp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/c1.log" 2>&1 &
        C1_PID=$!

        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 \
            -scalar="${TMP_DIR}/c2.sca" \
            -udp const20:const500:const20:const500 \
            -udp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/c2.log" 2>&1 &
        C2_PID=$!

        C1_RC=0; C2_RC=0
        wait "${C1_PID}" || C1_RC=$?
        wait "${C2_PID}" || C2_RC=$?

        if [ "${C1_RC}" -ne 0 ] || [ "${C2_RC}" -ne 0 ]; then
            echo "FAIL: IPv6 multi-clients exited with error (C1=${C1_RC}, C2=${C2_RC})" >&2
            cat "${TMP_DIR}/c1.log" "${TMP_DIR}/c2.log" >&2
            stop_and_verify_server || true
            exit 1
        fi

        verify_scalar_file "${TMP_DIR}/c1-active.sca" "Client 1 Active"
        verify_scalar_file "${TMP_DIR}/c2-active.sca" "Client 2 Active"
        ;;

    *)
        echo "FAIL: Unknown scenario '${SCENARIO}'" >&2
        stop_and_verify_server || true
        exit 1
        ;;
esac

# ====== 3. Stop Server with SIGINT and Verify Clean Exit ===================
if ! stop_and_verify_server; then
    exit 1
fi

echo "PASS: Multi-client scenario '${SCENARIO}' completed and verified successfully."
exit 0
