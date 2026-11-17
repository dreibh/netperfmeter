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
#   test_edge_cases.sh <netperfmeter_bin> <port> <address> <scenario>

set -eu

LC_ALL=C.UTF-8
export LC_ALL

NETPERFMETER="${1:-netperfmeter}"
PORT="${2:-25000}"
ADDRESS="${3:-127.0.0.1}"
SCENARIO="${4:-passive_runtime_expiration}"

CONTROL_PORT=$((PORT + 1))

case "${ADDRESS}" in
    *:*) TARGET="[${ADDRESS}]" ;;
    *)   TARGET="${ADDRESS}" ;;
esac

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_edge_test')
SERVER_PID=""
CLIENT_PID=""
RELAY_PID=""

cleanup_processes() {
    for _pid in ${CLIENT_PID} ${RELAY_PID} ${SERVER_PID}; do
        if [ -n "${_pid}" ] && kill -0 "${_pid}" 2>/dev/null; then
            kill -TERM "${_pid}" 2>/dev/null || true
        fi
    done
    for _pid in ${CLIENT_PID} ${RELAY_PID} ${SERVER_PID}; do
        if [ -n "${_pid}" ]; then
            wait "${_pid}" 2>/dev/null || true
        fi
    done
    CLIENT_PID=""
    RELAY_PID=""
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

wait_for_listener() {
    _host="$1"
    _port="$2"
    _proto="${3:-tcp}"
    _timeout="${4:-5}"
    _elapsed=0

    while [ "$_elapsed" -lt "$_timeout" ]; do
        if [ -n "${SERVER_PID}" ] && ! kill -0 "${SERVER_PID}" 2>/dev/null; then
            echo "FAIL: Passive server process terminated early." >&2
            if [ -s "${TMP_DIR}/server.log" ]; then
                cat "${TMP_DIR}/server.log" >&2
            fi
            return 1
        fi

        if [ "${_proto}" = "quic" ]; then
            if [ -s "${TMP_DIR}/server.log" ] && grep -q "Control Channel = " "${TMP_DIR}/server.log"; then
                return 0
            fi
        elif [ "${_proto}" = "sctp" ]; then
            if perl -e '
                use strict;
                use warnings;
                use Socket qw(AF_INET AF_INET6 SOCK_STREAM sockaddr_in sockaddr_in6 inet_aton inet_pton);

                my ($host, $port) = @ARGV;
                my $is_v6 = ($host =~ /:/);
                my $family = $is_v6 ? AF_INET6 : AF_INET;
                my $proto = 132; # IPPROTO_SCTP

                my $sock;
                if (socket($sock, $family, SOCK_STREAM, $proto)) {
                    my $addr = $is_v6 ? sockaddr_in6($port, inet_pton(AF_INET6, $host))
                                      : sockaddr_in($port, inet_aton($host));
                    if (connect($sock, $addr)) {
                        close($sock);
                        exit 0;
                    }
                    close($sock);
                }
                exit 1;
            ' "$_host" "$_port" 2>/dev/null; then
                return 0
            fi
        elif perl -MIO::Socket::IP -e \
           'exit(!IO::Socket::IP->new(PeerHost => $ARGV[0], PeerPort => $ARGV[1], Proto => "tcp", GetAddrInfoFlags => 0, Timeout => 1))' \
           "$_host" "$_port" 2>/dev/null; then
            return 0
        fi

        sleep 1
        _elapsed=$((_elapsed + 1))
    done

    echo "FAIL: Passive server failed to listen on ${_proto}://${_host}:${_port} within ${_timeout}s." >&2
    if [ -s "${TMP_DIR}/server.log" ]; then
        cat "${TMP_DIR}/server.log" >&2
    fi
    return 1
}

stop_and_check_server() {
    if [ -n "${SERVER_PID}" ]; then
        _srv_rc=0
        if kill -0 "${SERVER_PID}" 2>/dev/null; then
            kill -INT "${SERVER_PID}" 2>/dev/null || true
            wait "${SERVER_PID}" 2>/dev/null || _srv_rc=$?
        else
            wait "${SERVER_PID}" 2>/dev/null || _srv_rc=$?
        fi
        SERVER_PID=""

        if [ "${_srv_rc}" -ne 0 ]; then
            echo "FAIL: Passive server exited with non-zero code ${_srv_rc}." >&2
            if [ -s "${TMP_DIR}/server.log" ]; then
                cat "${TMP_DIR}/server.log" >&2
            fi
            exit 1
        fi
        if [ -s "${TMP_DIR}/server.log" ] && grep -qE "failed in|FATAL ERROR|terminate called|(^|[^* ])Aborted|Avbrutt|SIGABRT|Segmentation fault|assure" "${TMP_DIR}/server.log"; then
            echo "FAIL: Crash or assertion failure detected in passive server log." >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
    fi
}

check_clean_client_failure() {
    _rc="$1"
    if [ "${_rc}" -eq 0 ]; then
        echo "FAIL: Expected active client to fail (non-zero exit), but it succeeded (exit code 0)." >&2
        cat "${TMP_DIR}/client.log" >&2
        exit 1
    fi
    if [ "${_rc}" -ge 128 ]; then
        echo "FAIL: Active client terminated via signal (exit code ${_rc})." >&2
        cat "${TMP_DIR}/client.log" >&2
        exit 1
    fi
    if [ -s "${TMP_DIR}/client.log" ] && grep -qE "failed in|terminate called|(^|[^* ])Aborted|Avbrutt|SIGABRT|Segmentation fault|assure" "${TMP_DIR}/client.log"; then
        echo "FAIL: Crash or assertion failure detected in active client log." >&2
        cat "${TMP_DIR}/client.log" >&2
        exit 1
    fi
}

generate_ca_and_certs() {
    _prefix="$1"
    _ca_key="${TMP_DIR}/${_prefix}_ca.key"
    _ca_crt="${TMP_DIR}/${_prefix}_ca.crt"
    _srv_key="${TMP_DIR}/${_prefix}_srv.key"
    _srv_csr="${TMP_DIR}/${_prefix}_srv.csr"
    _srv_crt="${TMP_DIR}/${_prefix}_srv.crt"
    _srv_ext="${TMP_DIR}/${_prefix}_srv.cnf"
    _cli_key="${TMP_DIR}/${_prefix}_cli.key"
    _cli_csr="${TMP_DIR}/${_prefix}_cli.csr"
    _cli_crt="${TMP_DIR}/${_prefix}_cli.crt"
    _cli_ext="${TMP_DIR}/${_prefix}_cli.cnf"

    openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
        -keyout "${_ca_key}" -out "${_ca_crt}" \
        -subj "/CN=NetPerfMeterTestCA_${_prefix}" 2>/dev/null

    openssl req -newkey rsa:2048 -nodes \
        -keyout "${_srv_key}" -out "${_srv_csr}" \
        -subj "/CN=localhost" 2>/dev/null

    cat <<EOF >"${_srv_ext}"
subjectAltName = DNS:localhost,IP:127.0.0.1,IP:::1
extendedKeyUsage = serverAuth
EOF

    openssl x509 -req -in "${_srv_csr}" -CA "${_ca_crt}" -CAkey "${_ca_key}" \
        -CAcreateserial -out "${_srv_crt}" -days 1 \
        -extfile "${_srv_ext}" 2>/dev/null

    openssl req -newkey rsa:2048 -nodes \
        -keyout "${_cli_key}" -out "${_cli_csr}" \
        -subj "/CN=NetPerfMeterTestClient_${_prefix}" 2>/dev/null

    cat <<EOF >"${_cli_ext}"
extendedKeyUsage = clientAuth
EOF

    openssl x509 -req -in "${_cli_csr}" -CA "${_ca_crt}" -CAkey "${_ca_key}" \
        -CAcreateserial -out "${_cli_crt}" -days 1 \
        -extfile "${_cli_ext}" 2>/dev/null
}

case "${SCENARIO}" in
    # 1. Passive server automatic shutdown upon -runtime expiration
    passive_runtime_expiration)
        "${NETPERFMETER}" "${PORT}" -control-over-tcp -no-control-over-sctp \
            -runtime=3 \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi
        if ! wait_for_listener "${ADDRESS}" "${PORT}" "tcp" 5; then
            exit 1
        fi

        # Run a short 1s active measurement while passive server has -runtime=3
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=1 \
            -scalar="${TMP_DIR}/passive.sca" \
            -vector="${TMP_DIR}/passive.vec" \
            -tcp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        CLIENT_RC=0
        wait "${CLIENT_PID}" || CLIENT_RC=$?
        CLIENT_PID=""
        if [ "${CLIENT_RC}" -ne 0 ]; then
            echo "FAIL: Active client exited with error ${CLIENT_RC}." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi

        # Wait for passive server to exit on its own when -runtime=3 expires (no SIGINT)
        _waited=0
        while kill -0 "${SERVER_PID}" 2>/dev/null; do
            if [ "${_waited}" -ge 8 ]; then
                echo "FAIL: Passive server did not terminate automatically after -runtime=3." >&2
                cat "${TMP_DIR}/server.log" >&2
                exit 1
            fi
            sleep 1
            _waited=$((_waited + 1))
        done

        SERVER_RC=0
        wait "${SERVER_PID}" || SERVER_RC=$?
        SERVER_PID=""

        if [ "${SERVER_RC}" -ne 0 ]; then
            echo "FAIL: Passive server exited with non-zero code ${SERVER_RC} on -runtime expiration." >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
        if grep -qE "failed in|FATAL ERROR|terminate called|(^|[^* ])Aborted|Avbrutt|SIGABRT|Segmentation fault|assure" "${TMP_DIR}/server.log"; then
            echo "FAIL: Crash or assertion failure in passive server log." >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
        if [ ! -s "${TMP_DIR}/passive-passive.sca" ] || [ ! -s "${TMP_DIR}/passive-passive.vec" ]; then
            echo "FAIL: Passive server did not write expected scalar/vector output files." >&2
            exit 1
        fi
        ;;

    # 2a. Control-channel abort (SCTP ABORT / SO_LINGER=0) mid-measurement
    control_heartbeat_timeout)
        "${NETPERFMETER}" "${PORT}" -no-control-over-tcp -control-over-sctp \
            --control-hbinterval 200 --control-hbpathmaxrtx 1 --control-hbassocmaxrtx 2 \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "sctp" 5; then
            exit 1
        fi

        PROXY_DATA_PORT=$((PORT + 2))
        PROXY_CTRL_PORT=$((PORT + 3))

        # Relay 1-to-1 SCTP control & UDP data during initial handshake (0.6s after
        # first payload byte), then abort the SCTP control association via SO_LINGER=0
        # so the active client detects control channel loss mid-measurement.
        perl -MIO::Socket::IP -MIO::Select -MSocket=AF_INET,SOCK_STREAM,SOL_SOCKET,SO_REUSEADDR,SO_LINGER,sockaddr_in,inet_aton -MTime::HiRes=time -e '
            use strict;
            use warnings;

            my ($host, $udp_lport, $udp_tport, $sctp_lport, $sctp_tport) = @ARGV;
            my $proto_sctp = 132; # IPPROTO_SCTP

            socket(my $sctp_srv, AF_INET, SOCK_STREAM, $proto_sctp) or die "sctp socket: $!";
            setsockopt($sctp_srv, SOL_SOCKET, SO_REUSEADDR, pack("l", 1));
            bind($sctp_srv, sockaddr_in($sctp_lport, inet_aton($host))) or die "sctp bind: $!";
            listen($sctp_srv, 5) or die "sctp listen: $!";

            my $udp_srv = IO::Socket::IP->new(
                LocalHost => $host, LocalPort => $udp_lport,
                Proto => "udp", ReuseAddr => 1
            ) or die "udp listen: $!";

            my $udp_dst = IO::Socket::IP->new(
                PeerHost => $host, PeerPort => $udp_tport,
                Proto => "udp"
            ) or die "udp connect: $!";

            $sctp_srv->blocking(0);
            $udp_srv->blocking(0);
            $udp_dst->blocking(0);

            my $sel = IO::Select->new($sctp_srv, $udp_srv, $udp_dst);
            my %peer;
            my $abort_at;

            while (1) {
                my @ready = $sel->can_read(0.1);
                my $now = time();
                if (defined($abort_at) && $now >= $abort_at) {
                    for my $fh (keys %peer) {
                        my $sock = $peer{$fh};
                        next unless $sock;
                        setsockopt($sock, SOL_SOCKET, SO_LINGER, pack("ii", 1, 0));
                        close($sock);
                    }
                    exit 0;
                }

                for my $fh (@ready) {
                    if ($fh == $sctp_srv) {
                        my $cli;
                        next unless accept($cli, $sctp_srv);
                        socket(my $dst, AF_INET, SOCK_STREAM, $proto_sctp) or do { close($cli); next; };
                        if (!connect($dst, sockaddr_in($sctp_tport, inet_aton($host)))) {
                            close($cli);
                            close($dst);
                            next;
                        }
                        $cli->blocking(0);
                        $dst->blocking(0);
                        $peer{$cli} = $dst;
                        $peer{$dst} = $cli;
                        $sel->add($cli, $dst);
                    }
                    elsif ($fh == $udp_srv) {
                        my $buf = "";
                        my $sender = recv($udp_srv, $buf, 65536, 0);
                        send($udp_dst, $buf, 0) if defined($sender);
                    }
                    elsif ($fh == $udp_dst) {
                        my $buf = "";
                        recv($udp_dst, $buf, 65536, 0);
                    }
                    elsif (exists $peer{$fh}) {
                        my $dst = $peer{$fh};
                        my $buf = "";
                        my $n = sysread($fh, $buf, 65536);
                        if (!defined($n) || $n <= 0) {
                            $sel->remove($fh, $dst);
                            delete $peer{$fh};
                            delete $peer{$dst};
                            close($fh);
                            close($dst);
                            next;
                        }
                        $abort_at = $now + 0.6 unless defined $abort_at;
                        my $off = 0;
                        while ($off < length($buf)) {
                            my $w = syswrite($dst, $buf, length($buf) - $off, $off);
                            last unless defined $w;
                            $off += $w;
                        }
                    }
                }
            }
        ' "${ADDRESS}" "${PROXY_DATA_PORT}" "${PORT}" "${PROXY_CTRL_PORT}" "${CONTROL_PORT}" \
            >"${TMP_DIR}/relay.log" 2>&1 &
        RELAY_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${PROXY_CTRL_PORT}" "sctp" 5; then
            exit 1
        fi

        "${NETPERFMETER}" "${TARGET}:${PROXY_DATA_PORT}" -control-over-sctp \
            --control-hbinterval 200 --control-hbpathmaxrtx 1 --control-hbassocmaxrtx 2 \
            -runtime=15 -udp const10:const1000:const0:const0 \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        # Client must abort promptly when the control association is reset mid-measurement
        _waited=0
        while kill -0 "${CLIENT_PID}" 2>/dev/null; do
            if [ "${_waited}" -ge 8 ]; then
                echo "FAIL: Active client did not abort on lost SCTP control association within 8s." >&2
                cat "${TMP_DIR}/client.log" >&2
                exit 1
            fi
            sleep 1
            _waited=$((_waited + 1))
        done

        CLIENT_RC=0
        wait "${CLIENT_PID}" || CLIENT_RC=$?
        CLIENT_PID=""
        check_clean_client_failure "${CLIENT_RC}"

        stop_and_check_server
        ;;

    # 2b. Control-channel abort (TCP RST / SO_LINGER=0) mid-measurement
    control_tcp_abort)
        "${NETPERFMETER}" "${PORT}" -control-over-tcp -no-control-over-sctp -no-control-over-quic \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi

        PROXY_DATA_PORT=$((PORT + 2))
        PROXY_CTRL_PORT=$((PORT + 3))

        perl -MIO::Socket::IP -MIO::Select -MSocket=SOL_SOCKET,SO_LINGER -MTime::HiRes=time -e '
            use strict;
            use warnings;

            my ($host, $udp_lport, $udp_tport, $tcp_lport, $tcp_tport) = @ARGV;

            my $tcp_srv = IO::Socket::IP->new(
                LocalHost => $host, LocalPort => $tcp_lport,
                Proto => "tcp", Listen => 5, ReuseAddr => 1
            ) or die "tcp listen: $!";

            my $udp_srv = IO::Socket::IP->new(
                LocalHost => $host, LocalPort => $udp_lport,
                Proto => "udp", ReuseAddr => 1
            ) or die "udp listen: $!";

            my $udp_dst = IO::Socket::IP->new(
                PeerHost => $host, PeerPort => $udp_tport,
                Proto => "udp"
            ) or die "udp connect: $!";

            $tcp_srv->blocking(0);
            $udp_srv->blocking(0);
            $udp_dst->blocking(0);

            my $sel = IO::Select->new($tcp_srv, $udp_srv, $udp_dst);
            my %peer;
            my $abort_at;

            while (1) {
                my @ready = $sel->can_read(0.1);
                my $now = time();
                if (defined($abort_at) && $now >= $abort_at) {
                    for my $fh (keys %peer) {
                        my $sock = $peer{$fh};
                        next unless $sock;
                        setsockopt($sock, SOL_SOCKET, SO_LINGER, pack("ii", 1, 0));
                        close($sock);
                    }
                    exit 0;
                }

                for my $fh (@ready) {
                    if ($fh == $tcp_srv) {
                        my $cli = $tcp_srv->accept();
                        next unless $cli;
                        my $dst = IO::Socket::IP->new(
                            PeerHost => $host, PeerPort => $tcp_tport,
                            Proto => "tcp"
                        );
                        if (!$dst) {
                            close($cli);
                            next;
                        }
                        $cli->blocking(0);
                        $dst->blocking(0);
                        $peer{$cli} = $dst;
                        $peer{$dst} = $cli;
                        $sel->add($cli, $dst);
                    }
                    elsif ($fh == $udp_srv) {
                        my $buf = "";
                        my $sender = recv($udp_srv, $buf, 65536, 0);
                        send($udp_dst, $buf, 0) if defined($sender);
                    }
                    elsif ($fh == $udp_dst) {
                        my $buf = "";
                        recv($udp_dst, $buf, 65536, 0);
                    }
                    elsif (exists $peer{$fh}) {
                        my $dst = $peer{$fh};
                        my $buf = "";
                        my $n = sysread($fh, $buf, 65536);
                        if (!defined($n) || $n <= 0) {
                            $sel->remove($fh, $dst);
                            delete $peer{$fh};
                            delete $peer{$dst};
                            close($fh);
                            close($dst);
                            next;
                        }
                        $abort_at = $now + 0.6 unless defined $abort_at;
                        my $off = 0;
                        while ($off < length($buf)) {
                            my $w = syswrite($dst, $buf, length($buf) - $off, $off);
                            last unless defined $w;
                            $off += $w;
                        }
                    }
                }
            }
        ' "${ADDRESS}" "${PROXY_DATA_PORT}" "${PORT}" "${PROXY_CTRL_PORT}" "${CONTROL_PORT}" \
            >"${TMP_DIR}/relay.log" 2>&1 &
        RELAY_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${PROXY_CTRL_PORT}" "tcp" 5; then
            exit 1
        fi

        "${NETPERFMETER}" "${TARGET}:${PROXY_DATA_PORT}" -control-over-tcp \
            -runtime=15 -udp const10:const1000:const0:const0 \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        _waited=0
        while kill -0 "${CLIENT_PID}" 2>/dev/null; do
            if [ "${_waited}" -ge 8 ]; then
                echo "FAIL: Active client did not abort on lost TCP control connection within 8s." >&2
                cat "${TMP_DIR}/client.log" >&2
                exit 1
            fi
            sleep 1
            _waited=$((_waited + 1))
        done

        CLIENT_RC=0
        wait "${CLIENT_PID}" || CLIENT_RC=$?
        CLIENT_PID=""
        check_clean_client_failure "${CLIENT_RC}"

        stop_and_check_server
        ;;

    # 2c. Control-channel loss when QUIC passive server terminates mid-measurement
    control_quic_abort)
        generate_ca_and_certs "qhb"

        "${NETPERFMETER}" "${PORT}" -no-control-over-tcp -no-control-over-sctp -control-over-quic \
            --tls-key "${TMP_DIR}/qhb_srv.key" \
            --tls-cert "${TMP_DIR}/qhb_srv.crt" \
            --tls-ca "${TMP_DIR}/qhb_ca.crt" \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "quic" 5; then
            exit 1
        fi

        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-quic \
            --tls-hostname localhost \
            --tls-ca "${TMP_DIR}/qhb_ca.crt" \
            --tls-key "${TMP_DIR}/qhb_cli.key" \
            --tls-cert "${TMP_DIR}/qhb_cli.crt" \
            -runtime=2 -udp const10:const1000:const0:const0 \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        # Wait for measurement to start, then kill passive server mid-transfer
        _waited=0
        while [ "${_waited}" -lt 5 ]; do
            if [ -s "${TMP_DIR}/client.log" ] && grep -q "Starting measurement" "${TMP_DIR}/client.log"; then
                break
            fi
            sleep 1
            _waited=$((_waited + 1))
        done
        sleep 1

        if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
            kill -9 "${SERVER_PID}" 2>/dev/null || true
            wait "${SERVER_PID}" 2>/dev/null || true
        fi
        SERVER_PID=""

        _waited=0
        while kill -0 "${CLIENT_PID}" 2>/dev/null; do
            if [ "${_waited}" -ge 10 ]; then
                echo "FAIL: Active client did not abort on lost QUIC control connection within 10s." >&2
                cat "${TMP_DIR}/client.log" >&2
                exit 1
            fi
            sleep 1
            _waited=$((_waited + 1))
        done

        CLIENT_RC=0
        wait "${CLIENT_PID}" || CLIENT_RC=$?
        CLIENT_PID=""
        check_clean_client_failure "${CLIENT_RC}"
        ;;

    # 2d. Graceful SIGINT interruption of active client mid-measurement
    active_sigint_graceful_stop)
        "${NETPERFMETER}" "${PORT}" -control-over-tcp -no-control-over-sctp -no-control-over-quic \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi
        if ! wait_for_listener "${ADDRESS}" "${PORT}" "tcp" 5; then
            exit 1
        fi

        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=15 \
            -scalar="${TMP_DIR}/sigint.sca" \
            -vector="${TMP_DIR}/sigint.vec" \
            -tcp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        # Wait for measurement to start and transfer data
        _waited=0
        while [ "${_waited}" -lt 5 ]; do
            if [ -s "${TMP_DIR}/client.log" ] && grep -q "Starting measurement" "${TMP_DIR}/client.log"; then
                break
            fi
            sleep 1
            _waited=$((_waited + 1))
        done
        sleep 1

        if ! kill -0 "${CLIENT_PID}" 2>/dev/null; then
            echo "FAIL: Active client exited before SIGINT was sent." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi

        kill -INT "${CLIENT_PID}" 2>/dev/null || true

        CLIENT_RC=0
        wait "${CLIENT_PID}" || CLIENT_RC=$?
        CLIENT_PID=""

        if [ "${CLIENT_RC}" -ne 0 ]; then
            echo "FAIL: Active client exited with non-zero code ${CLIENT_RC} on SIGINT." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi
        if grep -qE "failed in|FATAL ERROR|terminate called|(^|[^* ])Aborted|Avbrutt|SIGABRT|Segmentation fault|assure" "${TMP_DIR}/client.log"; then
            echo "FAIL: Crash or assertion failure in active client log on SIGINT." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi
        if [ ! -s "${TMP_DIR}/sigint-active.sca" ] || [ ! -s "${TMP_DIR}/sigint-passive.sca" ] ||
           [ ! -s "${TMP_DIR}/sigint-active.vec" ] || [ ! -s "${TMP_DIR}/sigint-passive.vec" ]; then
            echo "FAIL: Active/passive scalar or vector files missing after graceful SIGINT stop." >&2
            exit 1
        fi

        stop_and_check_server
        ;;

    # 3. Passive control-protocol disable flags (-Y, -X, -W, -Z)
    passive_disable_ctrl_tcp|passive_disable_ctrl_sctp|passive_disable_ctrl_mptcp|passive_disable_ctrl_quic)
        SERVER_TLS_ARGS=""
        CLIENT_TLS_ARGS=""
        READY_PROTO="tcp"
        READY_PORT="${CONTROL_PORT}"

        case "${SCENARIO}" in
            passive_disable_ctrl_tcp)
                SERVER_CTRL_ARGS="-no-control-over-tcp -no-control-over-mptcp -control-over-sctp"
                CLIENT_CTRL_ARG="-control-over-tcp"
                READY_PROTO="sctp"
                ;;
            passive_disable_ctrl_sctp)
                SERVER_CTRL_ARGS="-control-over-tcp -no-control-over-sctp"
                CLIENT_CTRL_ARG="-control-over-sctp"
                ;;
            passive_disable_ctrl_mptcp)
                SERVER_CTRL_ARGS="-no-control-over-tcp -no-control-over-mptcp -control-over-sctp"
                CLIENT_CTRL_ARG="-control-over-mptcp"
                READY_PROTO="sctp"
                ;;
            passive_disable_ctrl_quic)
                generate_ca_and_certs "qdis"
                SERVER_CTRL_ARGS="-control-over-tcp -no-control-over-sctp -no-control-over-quic"
                CLIENT_CTRL_ARG="-control-over-quic"
                SERVER_TLS_ARGS="--tls-key ${TMP_DIR}/qdis_srv.key --tls-cert ${TMP_DIR}/qdis_srv.crt --tls-ca ${TMP_DIR}/qdis_ca.crt"
                CLIENT_TLS_ARGS="--tls-hostname localhost --tls-ca ${TMP_DIR}/qdis_ca.crt --tls-key ${TMP_DIR}/qdis_cli.key --tls-cert ${TMP_DIR}/qdis_cli.crt --control-hbinterval 500 --control-hbassocmaxrtx 2"
                ;;
        esac

        "${NETPERFMETER}" "${PORT}" ${SERVER_CTRL_ARGS} ${SERVER_TLS_ARGS} \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${READY_PORT}" "${READY_PROTO}" 5; then
            exit 1
        fi

        CLIENT_RC=0
        "${NETPERFMETER}" "${TARGET}:${PORT}" "${CLIENT_CTRL_ARG}" ${CLIENT_TLS_ARGS} \
            -runtime=2 -udp const10:const1000:const10:const1000 \
            >"${TMP_DIR}/client.log" 2>&1 || CLIENT_RC=$?
        check_clean_client_failure "${CLIENT_RC}"

        stop_and_check_server
        ;;

    # 4a. QUIC TLS failure: corrupt PEM certificate and private key files
    quic_tls_corrupt_pem)
        CORRUPT_KEY="${TMP_DIR}/corrupt.key"
        CORRUPT_CRT="${TMP_DIR}/corrupt.crt"
        printf "CORRUPT KEY DATA\n" >"${CORRUPT_KEY}"
        printf "CORRUPT CERT DATA\n" >"${CORRUPT_CRT}"

        # Passive server disables QUIC when PEM files are corrupt while keeping TCP control up
        "${NETPERFMETER}" "${PORT}" -control-over-tcp -no-control-over-sctp -control-over-quic \
            --tls-key "${CORRUPT_KEY}" --tls-cert "${CORRUPT_CRT}" \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi

        # Active client with corrupt PEM files must fail cleanly with non-zero exit code
        CLIENT_RC=0
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-quic \
            --tls-hostname localhost --tls-key "${CORRUPT_KEY}" --tls-cert "${CORRUPT_CRT}" \
            -runtime=2 -quic const10:const1000:const10:const1000 \
            >"${TMP_DIR}/client.log" 2>&1 || CLIENT_RC=$?
        check_clean_client_failure "${CLIENT_RC}"

        stop_and_check_server
        ;;

    # 4b. QUIC TLS failure: untrusted CA, untrusted client cert (mTLS), or hostname mismatch
    quic_tls_untrusted_ca|quic_tls_untrusted_client_cert|quic_tls_hostname_mismatch)
        generate_ca_and_certs "ca1"
        generate_ca_and_certs "ca2"

        "${NETPERFMETER}" "${PORT}" -no-control-over-tcp -no-control-over-sctp -control-over-quic \
            --tls-key "${TMP_DIR}/ca1_srv.key" \
            --tls-cert "${TMP_DIR}/ca1_srv.crt" \
            --tls-ca "${TMP_DIR}/ca1_ca.crt" \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "quic" 5; then
            exit 1
        fi

        case "${SCENARIO}" in
            quic_tls_untrusted_ca)
                CLIENT_TLS_ARGS="--tls-hostname localhost --tls-ca ${TMP_DIR}/ca2_ca.crt --tls-key ${TMP_DIR}/ca1_cli.key --tls-cert ${TMP_DIR}/ca1_cli.crt"
                ;;
            quic_tls_untrusted_client_cert)
                CLIENT_TLS_ARGS="--tls-hostname localhost --tls-ca ${TMP_DIR}/ca1_ca.crt --tls-key ${TMP_DIR}/ca2_cli.key --tls-cert ${TMP_DIR}/ca2_cli.crt"
                ;;
            quic_tls_hostname_mismatch)
                CLIENT_TLS_ARGS="--tls-hostname wrong.hostname.example --tls-ca ${TMP_DIR}/ca1_ca.crt --tls-key ${TMP_DIR}/ca1_cli.key --tls-cert ${TMP_DIR}/ca1_cli.crt"
                ;;
        esac

        CLIENT_RC=0
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-quic ${CLIENT_TLS_ARGS} \
            -runtime=2 -quic const10:const1000:const10:const1000 \
            >"${TMP_DIR}/client.log" 2>&1 || CLIENT_RC=$?
        check_clean_client_failure "${CLIENT_RC}"

        stop_and_check_server
        ;;

    *)
        echo "FAIL: Unknown scenario '${SCENARIO}'" >&2
        exit 1
        ;;
esac

echo "PASS: Edge case test (${SCENARIO}) passed."
exit 0
