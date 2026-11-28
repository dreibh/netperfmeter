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
CLIENT1_PID=""
CLIENT2_PID=""
CLIENT3_PID=""
CLIENT4_PID=""
CLIENT5_PID=""

cleanup_processes() {
    for _pid in ${CLIENT1_PID} ${CLIENT2_PID} ${CLIENT3_PID} ${CLIENT4_PID} ${CLIENT5_PID} ${SERVER_PID}; do
        if [ -n "${_pid}" ] && kill -0 "${_pid}" 2>/dev/null; then
            kill -TERM "${_pid}" 2>/dev/null || true
        fi
    done
    for _pid in ${CLIENT1_PID} ${CLIENT2_PID} ${CLIENT3_PID} ${CLIENT4_PID} ${CLIENT5_PID} ${SERVER_PID}; do
        if [ -n "${_pid}" ]; then
            wait "${_pid}" 2>/dev/null || true
        fi
    done
    CLIENT1_PID=""
    CLIENT2_PID=""
    CLIENT3_PID=""
    CLIENT4_PID=""
    CLIENT5_PID=""
    SERVER_PID=""
    if [ -n "${TMP_DIR}" ] && [ -d "${TMP_DIR}" ]; then
        rm -rf "${TMP_DIR}"
    fi
}

cleanup_exit() {
    trap - EXIT INT TERM
    cleanup_processes
}

cleanup_signal() {
    _sig="$1"
    trap - EXIT INT TERM
    cleanup_processes
    if [ "${_sig}" = "INT" ]; then
        exit 130
    else
        exit 143
    fi
}

trap cleanup_exit EXIT
trap 'cleanup_signal INT' INT
trap 'cleanup_signal TERM' TERM

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

# ====== TLS Setup for QUIC Scenarios (Mutual TLS: CA + Server + Client) ====
SERVER_TLS_ARGS=""
CLIENT_TLS_ARGS=""

case "${SCENARIO}" in
    *quic*|*all_protocols*|*5clients*|*mixed_protocol_all*)
        CA_KEY="${TMP_DIR}/test_ca.key"
        CA_CRT="${TMP_DIR}/test_ca.crt"
        SRV_KEY="${TMP_DIR}/test_server.key"
        SRV_CSR="${TMP_DIR}/test_server.csr"
        SRV_CRT="${TMP_DIR}/test_server.crt"
        SRV_EXT="${TMP_DIR}/server.cnf"
        CLI_KEY="${TMP_DIR}/test_client.key"
        CLI_CSR="${TMP_DIR}/test_client.csr"
        CLI_CRT="${TMP_DIR}/test_client.crt"
        CLI_EXT="${TMP_DIR}/client.cnf"

        # 1. Root CA
        openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
            -keyout "${CA_KEY}" -out "${CA_CRT}" \
            -subj "/CN=NetPerfMeterTestCA" 2>/dev/null

        # 2. Server Certificate (signed by CA)
        openssl req -newkey rsa:2048 -nodes \
            -keyout "${SRV_KEY}" -out "${SRV_CSR}" \
            -subj "/CN=localhost" 2>/dev/null

        cat <<EOF >"${SRV_EXT}"
subjectAltName = DNS:localhost,IP:127.0.0.1,IP:::1
extendedKeyUsage = serverAuth
EOF

        openssl x509 -req -in "${SRV_CSR}" -CA "${CA_CRT}" -CAkey "${CA_KEY}" \
            -CAcreateserial -out "${SRV_CRT}" -days 1 \
            -extfile "${SRV_EXT}" 2>/dev/null

        # 3. Client Certificate (signed by CA)
        openssl req -newkey rsa:2048 -nodes \
            -keyout "${CLI_KEY}" -out "${CLI_CSR}" \
            -subj "/CN=NetPerfMeterTestClient" 2>/dev/null

        cat <<EOF >"${CLI_EXT}"
extendedKeyUsage = clientAuth
EOF

        openssl x509 -req -in "${CLI_CSR}" -CA "${CA_CRT}" -CAkey "${CA_KEY}" \
            -CAcreateserial -out "${CLI_CRT}" -days 1 \
            -extfile "${CLI_EXT}" 2>/dev/null

        SERVER_TLS_ARGS="--tls-key ${SRV_KEY} --tls-cert ${SRV_CRT} --tls-ca ${CA_CRT}"
        CLIENT_TLS_ARGS="--tls-hostname localhost --tls-ca ${CA_CRT} --tls-key ${CLI_KEY} --tls-cert ${CLI_CRT}"
        ;;
esac

# ====== Verification Helpers ===============================================
verify_server_alive() {
    _stage="$1"
    if [ -n "${SERVER_PID}" ] && ! kill -0 "${SERVER_PID}" 2>/dev/null; then
        echo "FAIL: Passive server died during ${_stage}!" >&2
        if [ -s "${TMP_DIR}/server.log" ]; then
            cat "${TMP_DIR}/server.log" >&2
        fi
        exit 1
    fi
}

verify_generation_outputs() {
    _gen="$1"
    _ext="$2"
    _num_flows="$3"
    _prefix="${TMP_DIR}/gen${_gen}"

    if [ ! -s "${_prefix}.config" ]; then
        echo "FAIL: Generation ${_gen} missing config file ${_prefix}.config." >&2
        exit 1
    fi
    for _side in active passive; do
        if [ ! -s "${_prefix}-${_side}.sca${_ext}" ]; then
            echo "FAIL: Generation ${_gen} missing ${_side} scalar file." >&2
            exit 1
        fi
        if [ ! -s "${_prefix}-${_side}.vec${_ext}" ]; then
            echo "FAIL: Generation ${_gen} missing ${_side} vector file." >&2
            exit 1
        fi
    done

    _vec_count=$(find "${TMP_DIR}" -maxdepth 1 -name "gen${_gen}-passive-*-*.vec${_ext}" -size +0c | wc -l)
    if [ "${_vec_count}" -lt "${_num_flows}" ]; then
        echo "FAIL: Generation ${_gen} expected at least ${_num_flows} non-empty passive per-flow vector files, found ${_vec_count}." >&2
        ls -la "${TMP_DIR}" >&2
        exit 1
    fi
}

wait_for_client_started() {
    _log="$1"
    _pid="$2"
    _waited=0
    while [ "${_waited}" -lt 8 ]; do
        if ! kill -0 "${_pid}" 2>/dev/null; then
            echo "FAIL: Client exited prematurely before starting measurement." >&2
            if [ -s "${_log}" ]; then
                cat "${_log}" >&2
            fi
            exit 1
        fi
        if [ -s "${_log}" ] && grep -q "Starting measurement" "${_log}"; then
            return 0
        fi
        sleep 1
        _waited=$((_waited + 1))
    done
    return 0
}

# ====== Start Server =======================================================
SERVER_CONTROL_ARGS="-control-over-tcp"
SERVER_EXTRA_ARGS=""
case "${SCENARIO}" in
    *mptcp*|*5clients*) SERVER_CONTROL_ARGS="-control-over-tcp -control-over-mptcp" ;;
esac
case "${SCENARIO}" in
    *after_sigkill*) SERVER_EXTRA_ARGS="--loglevel 1 --control-hbinterval 5000 --control-hbassocmaxrtx 1" ;;
esac
"${NETPERFMETER}" "${PORT}" ${SERVER_CONTROL_ARGS} ${SERVER_TLS_ARGS} ${SERVER_EXTRA_ARGS} >"${TMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" 5; then
    exit 1
fi

CLIENT1_RC=0
CLIENT2_RC=0
CLIENT3_RC=0
CLIENT4_RC=0
CLIENT5_RC=0

# ====== Execute Scenario ===================================================
case "${SCENARIO}" in
    # 1. Two clients simultaneously running TCP and UDP flows
    concurrent_2clients_udp_tcp)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        ;;

    # 2. Staggered overlapping clients: Client 2 finishes and tears down while Client 1 is mid-flight
    staggered_overlap_udp_tcp)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=5 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        sleep 1
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        ;;

    # 3. Sequential session reuse: 3 client generations run back-to-back against the same server
    sequential_session_reuse)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""

        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""

        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=2 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client3.log" 2>&1 &
        CLIENT3_PID=$!
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        ;;

    # 4. Two clients running simultaneously over IPv6
    concurrent_2clients_ipv6)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=3 -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
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
        CLIENT1_PID=""
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        ;;

    # 6. Two clients simultaneously running MPTCP flows
    concurrent_2clients_mptcp)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-mptcp -runtime=3 \
            -mptcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-mptcp -runtime=3 \
            -mptcp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        ;;

    # 7. Staggered overlapping clients over MPTCP
    staggered_overlap_mptcp)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-mptcp -runtime=5 \
            -mptcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        sleep 1
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-mptcp -runtime=2 \
            -mptcp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        ;;

    # 8. Tri-protocol concurrent stress: 3 clients (MPTCP + TCP + UDP)
    concurrent_3clients_mptcp_tcp_udp)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-mptcp -runtime=4 \
            -mptcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client3.log" 2>&1 &
        CLIENT3_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        ;;

    # 9. Quad-protocol concurrent stress: 4 clients (MPTCP + SCTP + TCP + UDP)
    concurrent_4clients_mptcp_sctp_tcp_udp)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-mptcp -runtime=4 \
            -mptcp const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
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
        CLIENT1_PID=""
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        wait "${CLIENT4_PID}" || CLIENT4_RC=$?
        CLIENT4_PID=""
        ;;

    # 10. Two clients simultaneously running QUIC flows
    concurrent_2clients_quic)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=3 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=3 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        ;;

    # 11. Staggered overlapping clients over QUIC
    staggered_overlap_quic)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=5 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        sleep 1
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=2 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!

        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        ;;

    # 12. Quad-protocol concurrent stress: 4 clients (QUIC + SCTP + TCP + UDP)
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
        CLIENT1_PID=""
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        wait "${CLIENT4_PID}" || CLIENT4_RC=$?
        CLIENT4_PID=""
        ;;

    # 13. Penta-protocol concurrent stress: 5 clients (QUIC + MPTCP + SCTP + TCP + UDP)
    concurrent_5clients_all_protocols)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} -runtime=4 \
            -quic const10:const1000:const10:const1000 >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-mptcp -runtime=4 \
            -mptcp const10:const1000:const10:const1000 >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -sctp const10:const1000:const10:const1000 >"${TMP_DIR}/client3.log" 2>&1 &
        CLIENT3_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -tcp const10:const1000:const10:const1000 >"${TMP_DIR}/client4.log" 2>&1 &
        CLIENT4_PID=$!
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=4 \
            -udp const10:const1000:const10:const1000 >"${TMP_DIR}/client5.log" 2>&1 &
        CLIENT5_PID=$!

        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        wait "${CLIENT4_PID}" || CLIENT4_RC=$?
        CLIENT4_PID=""
        wait "${CLIENT5_PID}" || CLIENT5_RC=$?
        CLIENT5_PID=""
        ;;

    # 14. Sequential 3-stream SCTP reuse (3 generations, SCTP control, bz2 outputs)
    sequential_multistream_sctp)
        for _gen in 1 2 3; do
            _prefix="${TMP_DIR}/gen${_gen}"
            "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-sctp --runtime 1 \
                --config "${_prefix}.config" \
                --scalar "${_prefix}.sca.bz2" \
                --vector "${_prefix}.vec.bz2" \
                --sctp const5:const1000:const5:const1000 const5:const1000:const5:const1000 const5:const1000:const5:const1000 \
                >"${TMP_DIR}/client${_gen}.log" 2>&1 &
            eval "CLIENT${_gen}_PID=\$!"
            eval "wait \"\${CLIENT${_gen}_PID}\" || CLIENT${_gen}_RC=\$?"
            eval "CLIENT${_gen}_PID=\"\""
            eval "_rc=\$CLIENT${_gen}_RC"
            if [ "${_rc}" -ne 0 ]; then
                break
            fi
            verify_server_alive "sequential_multistream_sctp gen ${_gen}"
            verify_generation_outputs "${_gen}" ".bz2" 3
        done
        ;;

    # 15. Sequential 3-stream QUIC reuse (3 generations, TCP control)
    sequential_multistream_quic)
        for _gen in 1 2 3; do
            _prefix="${TMP_DIR}/gen${_gen}"
            "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} --runtime 2 \
                --config "${_prefix}.config" \
                --scalar "${_prefix}.sca" \
                --vector "${_prefix}.vec" \
                --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
                >"${TMP_DIR}/client${_gen}.log" 2>&1 &
            eval "CLIENT${_gen}_PID=\$!"
            eval "wait \"\${CLIENT${_gen}_PID}\" || CLIENT${_gen}_RC=\$?"
            eval "CLIENT${_gen}_PID=\"\""
            eval "_rc=\$CLIENT${_gen}_RC"
            if [ "${_rc}" -ne 0 ]; then
                break
            fi
            verify_server_alive "sequential_multistream_quic gen ${_gen}"
            verify_generation_outputs "${_gen}" "" 3
        done
        ;;

    # 16. Sequential 3-stream QUIC reuse over QUIC control channel (3 generations)
    sequential_multistream_quic_ctrl_quic)
        for _gen in 1 2 3; do
            _prefix="${TMP_DIR}/gen${_gen}"
            "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-quic ${CLIENT_TLS_ARGS} --runtime 2 \
                --config "${_prefix}.config" \
                --scalar "${_prefix}.sca" \
                --vector "${_prefix}.vec" \
                --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
                >"${TMP_DIR}/client${_gen}.log" 2>&1 &
            eval "CLIENT${_gen}_PID=\$!"
            eval "wait \"\${CLIENT${_gen}_PID}\" || CLIENT${_gen}_RC=\$?"
            eval "CLIENT${_gen}_PID=\"\""
            eval "_rc=\$CLIENT${_gen}_RC"
            if [ "${_rc}" -ne 0 ]; then
                break
            fi
            verify_server_alive "sequential_multistream_quic_ctrl_quic gen ${_gen}"
            verify_generation_outputs "${_gen}" "" 3
        done
        ;;

    # 17. Multiple multi-stream SCTP associations per client in sequence (2 gens x 2 assocs x 3 streams)
    sequential_multi_assoc_multistream_sctp)
        for _gen in 1 2; do
            _prefix="${TMP_DIR}/gen${_gen}"
            "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-sctp --runtime 2 \
                --config "${_prefix}.config" \
                --scalar "${_prefix}.sca" \
                --vector "${_prefix}.vec" \
                --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
                --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
                >"${TMP_DIR}/client${_gen}.log" 2>&1 &
            eval "CLIENT${_gen}_PID=\$!"
            eval "wait \"\${CLIENT${_gen}_PID}\" || CLIENT${_gen}_RC=\$?"
            eval "CLIENT${_gen}_PID=\"\""
            eval "_rc=\$CLIENT${_gen}_RC"
            if [ "${_rc}" -ne 0 ]; then
                break
            fi
            verify_server_alive "sequential_multi_assoc_multistream_sctp gen ${_gen}"
            verify_generation_outputs "${_gen}" "" 6
        done
        ;;

    # 18. Multiple multi-stream QUIC connections per client in sequence (2 gens x 2 conns x 3 streams)
    sequential_multi_assoc_multistream_quic)
        for _gen in 1 2; do
            _prefix="${TMP_DIR}/gen${_gen}"
            "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} --runtime 2 \
                --config "${_prefix}.config" \
                --scalar "${_prefix}.sca" \
                --vector "${_prefix}.vec" \
                --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
                --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
                >"${TMP_DIR}/client${_gen}.log" 2>&1 &
            eval "CLIENT${_gen}_PID=\$!"
            eval "wait \"\${CLIENT${_gen}_PID}\" || CLIENT${_gen}_RC=\$?"
            eval "CLIENT${_gen}_PID=\"\""
            eval "_rc=\$CLIENT${_gen}_RC"
            if [ "${_rc}" -ne 0 ]; then
                break
            fi
            verify_server_alive "sequential_multi_assoc_multistream_quic gen ${_gen}"
            verify_generation_outputs "${_gen}" "" 6
        done
        ;;

    # 19. Abrupt SIGKILL of 3-stream SCTP client mid-transfer followed by clean 3-stream SCTP session
    sequential_multistream_sctp_after_sigkill)
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-sctp --runtime 10 \
            --sctp const10:const1000:const10:const1000 const10:const500:const10:const500 const10:const500:const10:const500 \
            >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        wait_for_client_started "${TMP_DIR}/client1.log" "${CLIENT1_PID}"
        sleep 1
        kill -9 "${CLIENT1_PID}" 2>/dev/null || true
        wait "${CLIENT1_PID}" 2>/dev/null || true
        CLIENT1_PID=""
        sleep 1
        verify_server_alive "after client1 SIGKILL (SCTP)"

        _prefix="${TMP_DIR}/gen2"
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-sctp --runtime 2 \
            --config "${_prefix}.config" \
            --scalar "${_prefix}.sca" \
            --vector "${_prefix}.vec" \
            --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        if [ "${CLIENT2_RC}" -eq 0 ]; then
            verify_server_alive "after client2 (SCTP)"
            verify_generation_outputs "2" "" 3
        fi
        ;;

    # 20. Abrupt SIGKILL of 3-stream QUIC client mid-transfer followed by clean 3-stream QUIC session
    sequential_multistream_quic_after_sigkill)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} --runtime 10 \
            --quic const10:const1000:const10:const1000 const10:const500:const10:const500 const10:const500:const10:const500 \
            >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        wait_for_client_started "${TMP_DIR}/client1.log" "${CLIENT1_PID}"
        sleep 1
        kill -9 "${CLIENT1_PID}" 2>/dev/null || true
        wait "${CLIENT1_PID}" 2>/dev/null || true
        CLIENT1_PID=""
        sleep 1
        verify_server_alive "after client1 SIGKILL (QUIC)"

        _prefix="${TMP_DIR}/gen2"
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} --runtime 2 \
            --config "${_prefix}.config" \
            --scalar "${_prefix}.sca" \
            --vector "${_prefix}.vec" \
            --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        if [ "${CLIENT2_RC}" -eq 0 ]; then
            verify_server_alive "after client2 (QUIC)"
            verify_generation_outputs "2" "" 3
        fi
        ;;

    # 20b. Abrupt SIGKILL of 3-stream QUIC client over QUIC control followed by clean 3-stream QUIC session
    sequential_multistream_quic_ctrl_quic_after_sigkill)
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-quic ${CLIENT_TLS_ARGS} \
            --control-hbinterval 5000 --control-hbassocmaxrtx 1 --runtime 15 \
            --quic const10:const1000:const10:const1000 \
                   const10:const500:const10:const500 \
                   const10:const500:const10:const500 \
            >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        wait_for_client_started "${TMP_DIR}/client1.log" "${CLIENT1_PID}"
        sleep 1
        kill -9 "${CLIENT1_PID}" 2>/dev/null || true
        wait "${CLIENT1_PID}" 2>/dev/null || true
        CLIENT1_PID=""
        sleep 6
        verify_server_alive "after client1 SIGKILL (QUIC control + 3-stream QUIC)"

        _prefix="${TMP_DIR}/gen2"
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-quic ${CLIENT_TLS_ARGS} \
            --control-hbinterval 5000 --control-hbassocmaxrtx 2 --runtime 2 \
            --config "${_prefix}.config" \
            --scalar "${_prefix}.sca" \
            --vector "${_prefix}.vec" \
            --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        if [ "${CLIENT2_RC}" -eq 0 ]; then
            verify_server_alive "after client2 (QUIC control + 3-stream QUIC)"
            verify_generation_outputs "2" "" 3
        fi
        ;;

    # 21. Staggered overlap + sequential fd reuse on 3-stream SCTP
    staggered_overlap_multistream_sctp)
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-sctp --runtime 6 \
            --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        sleep 1
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-sctp --runtime 1 \
            --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        verify_server_alive "staggered_overlap_multistream_sctp after client2"

        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-sctp --runtime 1 \
            --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client3.log" 2>&1 &
        CLIENT3_PID=$!
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        ;;

    # 22. Staggered overlap + sequential fd reuse on 3-stream QUIC
    staggered_overlap_multistream_quic)
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} --runtime 6 \
            --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        sleep 1
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} --runtime 1 \
            --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        verify_server_alive "staggered_overlap_multistream_quic after client2"

        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp ${CLIENT_TLS_ARGS} --runtime 1 \
            --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client3.log" 2>&1 &
        CLIENT3_PID=$!
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        ;;

    # 23. Sequential 3-flow UDP reuse with distinct per-flow Traffic Class (and IPv6 Flow Labels)
    sequential_multiflow_udp_qos)
        case "${ADDRESS}" in
            *:*)
                _f1="const5:const1000:const5:const1000"
                _f2="const5:const500:const5:const500"
                _f3="const5:const500:const5:const500"
                ;;
            *)
                _f1="const5:const1000:const5:const1000"
                _f2="const5:const500:const5:const500"
                _f3="const5:const500:const5:const500"
                ;;
        esac
        for _gen in 1 2 3; do
            _prefix="${TMP_DIR}/gen${_gen}"
            "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp --runtime 2 \
                --config "${_prefix}.config" \
                --scalar "${_prefix}.sca" \
                --vector "${_prefix}.vec" \
                --udp "${_f1}" --udp "${_f2}" --udp "${_f3}" \
                >"${TMP_DIR}/client${_gen}.log" 2>&1 &
            eval "CLIENT${_gen}_PID=\$!"
            eval "wait \"\${CLIENT${_gen}_PID}\" || CLIENT${_gen}_RC=\$?"
            eval "CLIENT${_gen}_PID=\"\""
            eval "_rc=\$CLIENT${_gen}_RC"
            if [ "${_rc}" -ne 0 ]; then
                break
            fi
            verify_server_alive "sequential_multiflow_udp_qos gen ${_gen}"
            verify_generation_outputs "${_gen}" "" 3
        done
        ;;

    # 24. Alternating mixed-protocol sequential generations (SCTP -> TCP+2xUDP -> 4-stream SCTP)
    sequential_mixed_protocol_multistream)
        _prefix="${TMP_DIR}/gen1"
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-sctp --runtime 1 \
            --config "${_prefix}.config" --scalar "${_prefix}.sca" --vector "${_prefix}.vec" \
            --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        if [ "${CLIENT1_RC}" -eq 0 ]; then
            verify_server_alive "mixed gen 1"
            verify_generation_outputs "1" "" 3
        fi

        _prefix="${TMP_DIR}/gen2"
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp --runtime 1 \
            --config "${_prefix}.config" --scalar "${_prefix}.sca" --vector "${_prefix}.vec" \
            --tcp const5:const1000:const5:const1000 \
            --udp const5:const500:const5:const500 \
            --udp const5:const500:const5:const500 \
            >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        if [ "${CLIENT2_RC}" -eq 0 ]; then
            verify_server_alive "mixed gen 2"
            verify_generation_outputs "2" "" 3
        fi

        _prefix="${TMP_DIR}/gen3"
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp --runtime 1 \
            --config "${_prefix}.config" --scalar "${_prefix}.sca" --vector "${_prefix}.vec" \
            --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client3.log" 2>&1 &
        CLIENT3_PID=$!
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        if [ "${CLIENT3_RC}" -eq 0 ]; then
            verify_server_alive "mixed gen 3"
            verify_generation_outputs "3" "" 4
        fi
        ;;

    # 25. Alternating mixed-protocol sequential generations including QUIC
    sequential_mixed_protocol_all)
        _prefix="${TMP_DIR}/gen1"
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-sctp --runtime 1 \
            --config "${_prefix}.config" --scalar "${_prefix}.sca" --vector "${_prefix}.vec" \
            --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT1_PID=$!
        wait "${CLIENT1_PID}" || CLIENT1_RC=$?
        CLIENT1_PID=""
        if [ "${CLIENT1_RC}" -eq 0 ]; then
            verify_server_alive "mixed_all gen 1"
            verify_generation_outputs "1" "" 3
        fi

        _prefix="${TMP_DIR}/gen2"
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp --runtime 1 \
            --config "${_prefix}.config" --scalar "${_prefix}.sca" --vector "${_prefix}.vec" \
            --tcp const5:const1000:const5:const1000 \
            --udp const5:const500:const5:const500 \
            --udp const5:const500:const5:const500 \
            >"${TMP_DIR}/client2.log" 2>&1 &
        CLIENT2_PID=$!
        wait "${CLIENT2_PID}" || CLIENT2_RC=$?
        CLIENT2_PID=""
        if [ "${CLIENT2_RC}" -eq 0 ]; then
            verify_server_alive "mixed_all gen 2"
            verify_generation_outputs "2" "" 3
        fi

        _prefix="${TMP_DIR}/gen3"
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-quic ${CLIENT_TLS_ARGS} --runtime 2 \
            --config "${_prefix}.config" --scalar "${_prefix}.sca" --vector "${_prefix}.vec" \
            --quic const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client3.log" 2>&1 &
        CLIENT3_PID=$!
        wait "${CLIENT3_PID}" || CLIENT3_RC=$?
        CLIENT3_PID=""
        if [ "${CLIENT3_RC}" -eq 0 ]; then
            verify_server_alive "mixed_all gen 3"
            verify_generation_outputs "3" "" 3
        fi

        _prefix="${TMP_DIR}/gen4"
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp --runtime 1 \
            --config "${_prefix}.config" --scalar "${_prefix}.sca" --vector "${_prefix}.vec" \
            --sctp const5:const1000:const5:const1000 const5:const500:const5:const500 const5:const500:const5:const500 const5:const500:const5:const500 \
            >"${TMP_DIR}/client4.log" 2>&1 &
        CLIENT4_PID=$!
        wait "${CLIENT4_PID}" || CLIENT4_RC=$?
        CLIENT4_PID=""
        if [ "${CLIENT4_RC}" -eq 0 ]; then
            verify_server_alive "mixed_all gen 4"
            verify_generation_outputs "4" "" 4
        fi
        ;;

    *)
        echo "FAIL: Unknown scenario '${SCENARIO}'" >&2
        exit 1
        ;;
esac

# ====== Check Client Results ==============================================
for _c in 1 2 3 4 5; do
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

    if [ -s "${TMP_DIR}/server.log" ] && grep -qE "failed in|FATAL ERROR|terminate called|(^|[^* ])Aborted|Avbrutt|SIGABRT|Segmentation fault|assure|MessageReader already has a state" "${TMP_DIR}/server.log"; then
        echo "FAIL: Assertion failure or crash detected in server log." >&2
        cat "${TMP_DIR}/server.log" >&2
        exit 1
    fi

    if [ -s "${TMP_DIR}/server.log" ]; then
        _eoi_cnt=$(perl -ne '$c++ if /End of input for /; END { print $c+0 }' "${TMP_DIR}/server.log")
        if [ "${_eoi_cnt}" -gt 32 ]; then
            echo "FAIL: Poll-loop spin detected in server log (${_eoi_cnt} 'End of input' messages)!" >&2
            head -n 60 "${TMP_DIR}/server.log" >&2
            exit 1
        fi
    fi
fi

echo "PASS: Multi-client session cleanup test (${SCENARIO}) passed."
exit 0
