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
#   test_multi_client.sh <netperfmeter_bin> <port> <address> <scenario>

set -eu

NETPERFMETER="${1:-netperfmeter}"
PORT="${2:-25000}"
ADDRESS="${3:-127.0.0.1}"
SCENARIO="${4:-concurrent_2clients_udp_tcp}"

CONTROL_PORT=$((PORT + 1))

case "${ADDRESS}" in
    *:*) TARGET="[${ADDRESS}]" ;;
    *)   TARGET="${ADDRESS}" ;;
esac

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_multi_test')
SERVER_PID=""

cleanup() {
    if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill -INT "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT INT TERM

# ====== Readiness Probe ====================================================
wait_for_listener() {
    _host="$1"
    _port="$2"
    _timeout="${3:-5}"
    _elapsed=0

    while [ "$_elapsed" -lt "$_timeout" ]; do
        if [ -n "${SERVER_PID}" ] && ! kill -0 "${SERVER_PID}" 2>/dev/null; then
            echo "FAIL: Passive server process (PID ${SERVER_PID}) terminated early." >&2
            if [ -s "${TMP_DIR}/server.log" ]; then
                cat "${TMP_DIR}/server.log" >&2
            fi
            return 1
        fi

        if perl -MIO::Socket::IP -e \
           'exit(!IO::Socket::IP->new(PeerHost => $ARGV[0], PeerPort => $ARGV[1], Proto => "tcp", GetAddrInfoFlags => 0, Timeout => 1))' \
           "$_host" "$_port" 2>/dev/null; then
            return 0
        fi

        sleep 1
        _elapsed=$((_elapsed + 1))
    done

    echo "FAIL: Passive server failed to listen on tcp://${_host}:${_port} within ${_timeout}s." >&2
    if [ -s "${TMP_DIR}/server.log" ]; then
        cat "${TMP_DIR}/server.log" >&2
    fi
    return 1
}

# ====== TLS Setup for QUIC Scenarios =======================================
SERVER_TLS_ARGS=""
CLIENT_TLS_ARGS=""

case "${SCENARIO}" in
    *quic*|*all_protocols*)
        CA_KEY="${TMP_DIR}/test_ca.key"
        CA_CRT="${TMP_DIR}/test_ca.crt"
        SRV_KEY="${TMP_DIR}/test_server.key"
        SRV_CSR="${TMP_DIR}/test_server.csr"
        SRV_CRT="${TMP_DIR}/test_server.crt"
        SAN_EXT="${TMP_DIR}/san.cnf"

        openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
            -keyout "${CA_KEY}" -out "${CA_CRT}" \
            -subj "/CN=NetPerfMeterTestCA" 2>/dev/null

        openssl req -newkey rsa:2048 -nodes \
            -keyout "${SRV_KEY}" -out "${SRV_CSR}" \
            -subj "/CN=localhost" 2>/dev/null

        cat <<EOF >"${SAN_EXT}"
subjectAltName = DNS:localhost,IP:127.0.0.1,IP:::1
EOF

        openssl x509 -req -in "${SRV_CSR}" -CA "${CA_CRT}" -CAkey "${CA_KEY}" \
            -CAcreateserial -out "${SRV_CRT}" -days 1 \
            -extfile "${SAN_EXT}" 2>/dev/null

        SERVER_TLS_ARGS="--tls-key ${SRV_KEY} --tls-cert ${SRV_CRT} --tls-ca ${CA_CRT}"
        CLIENT_TLS_ARGS="--tls-hostname localhost --tls-ca ${CA_CRT}"
        ;;
esac

# ====== Start Server =======================================================
SERVER_CONTROL_ARGS="-control-over-tcp"
"${NETPERFMETER}" "${PORT}" ${SERVER_CONTROL_ARGS} ${SERVER_TLS_ARGS} >"${TMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" 5; then
    exit 1
fi

CLIENT1_RC=0
CLIENT2_RC=0
CLIENT3_RC=0
CLIENT4_RC=0

# ====== Execute Scenario ===================================================
case "${SCENARIO}" in
    # 1. Two clients simultaneously running TCP and UDP flows
    concurrent_2clients_udp_tcp)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        ;;

    # 2. Staggered overlapping clients: Client 2 finishes and tears down while Client 1 is mid-flight
    staggered_overlap_udp_tcp)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=5 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        sleep 1
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        ;;

    # 3. Sequential session reuse: 3 client generations run back-to-back against the same server
    sequential_session_reuse)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 || CLIENT1_RC=$?
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 || CLIENT2_RC=$?
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client3.log" 2>&1 || CLIENT3_RC=$?
        ;;

    # 4. Two clients running simultaneously over IPv6
    concurrent_2clients_ipv6)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        ;;

    # 5. Tri-protocol concurrent stress: 3 clients (SCTP multistream + TCP/UDP + 2x UDP)
    concurrent_3clients_mixed)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -sctp const10:const1000:const10:const1000 const10:const500:const10:const500 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -tcp const10:const1000:const10:const1000 -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -udp const10:const1000:const10:const1000 -udp const10:const500:const10:const500 >"${TMP_DIR}/client3.log" 2>&1 &
        CLIENT3_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        ;;

    # 6. Two clients simultaneously running QUIC flows
    concurrent_2clients_quic)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=3 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=3 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        ;;

    # 7. Staggered overlapping clients over QUIC
    staggered_overlap_quic)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=5 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        sleep 1
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=2 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        ;;

    # 8. Quad-protocol concurrent stress: 4 clients (QUIC + SCTP + TCP + UDP)
    concurrent_4clients_all_protocols)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=4 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -sctp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client3.log" 2>&1 &
        CLIENT3_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client4.log" 2>&1 &
        CLIENT4_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        wait "${CLIENT4_PID}" || CLIENT4_RC=$?
        ;;

    *)
        echo "FAIL: Unknown scenario '${SCENARIO}'" >&2
        exit 1
        ;;
esac

# ====== Check Client Results ==============================================
for _c in 1 2 3 4; do
    eval "_rc=\$CLIENT${_c}_RC"
    if [ "${_rc}" -ne 0 ]; then
        echo "FAIL: Client ${_c} exited with error code ${_rc}." >&2
        if [ -s "${TMP_DIR}/client${_c}.log" ]; then
            cat "${TMP_DIR}/client${_c}.log" >&2
        fi
        exit 1
    fi
done

# ====== Stop Server and Validate Log =======================================
SERVER_RC=0
if [ -n "${SERVER_PID}" ]; then
    if kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill -INT "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || SERVER_RC=$?
    else
        wait "${SERVER_PID}" 2>/dev/null || SERVER_RC=$?
    fi
    SERVER_PID=""

    if [ "${SERVER_RC}" -ne 0 ]; then
        echo "FAIL: Passive server instance crashed with exit code ${SERVER_RC}." >&2
        if [ -s "${TMP_DIR}/server.log" ]; then
            cat "${TMP_DIR}/server.log" >&2
        fi
        exit 1
    fi

    if [ -s "${TMP_DIR}/server.log" ] && grep -qE "failed in|FATAL ERROR|Aborted|Avbrutt" "${TMP_DIR}/server.log"; then
        echo "FAIL: Assertion failure or crash detected in server log." >&2
        cat "${TMP_DIR}/server.log" >&2
        exit 1
    fi
fi

echo "PASS: Multi-client session cleanup test (${SCENARIO}) passed."
exit 0
