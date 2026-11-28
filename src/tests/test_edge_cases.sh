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
        if [ -s "${TMP_DIR}/server.log" ]; then
            _eoi_cnt=$(perl -ne '$c++ if /End of input for /; END { print $c+0 }' "${TMP_DIR}/server.log")
            if [ "${_eoi_cnt}" -gt 24 ]; then
                echo "FAIL: Poll-loop spin detected in server log (${_eoi_cnt} 'End of input' messages)!" >&2
                head -n 60 "${TMP_DIR}/server.log" >&2
                exit 1
            fi
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

    # 5a. Mid-measurement TCP data EOF while control channel stays open (--loglevel 1 poll-spin check)
    tcp_data_eof_no_spin)
        "${NETPERFMETER}" "${PORT}" -control-over-tcp -no-control-over-sctp -no-control-over-quic \
            --loglevel 1 \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi
        if ! wait_for_listener "${ADDRESS}" "${PORT}" "tcp" 5; then
            exit 1
        fi

        PROXY_BASE_PORT=$((PORT + 4))
        PROXY_CTRL_PORT=$((PROXY_BASE_PORT + 1))

        # Relay TCP control throughout the entire 3s run, but close the TCP data
        # relay after 0.7s so the server sees data EOF for >2s while control stays up.
        perl -MIO::Socket::IP -MIO::Select -MTime::HiRes=time -e '
            use strict;
            use warnings;

            my ($host, $data_lport, $data_tport, $ctrl_lport, $ctrl_tport) = @ARGV;

            my $ctrl_srv = IO::Socket::IP->new(
                LocalHost => $host, LocalPort => $ctrl_lport,
                Proto => "tcp", Listen => 5, ReuseAddr => 1
            ) or die "ctrl listen: $!";
            my $data_srv = IO::Socket::IP->new(
                LocalHost => $host, LocalPort => $data_lport,
                Proto => "tcp", Listen => 5, ReuseAddr => 1
            ) or die "data listen: $!";

            $ctrl_srv->blocking(0);
            $data_srv->blocking(0);

            my $sel = IO::Select->new($ctrl_srv, $data_srv);
            my (%peer, %is_data);
            my $drop_data_at;

            while (1) {
                my @ready = $sel->can_read(0.1);
                my $now = time();
                if (defined($drop_data_at) && $now >= $drop_data_at) {
                    for my $fh (keys %is_data) {
                        my $s = $is_data{$fh};
                        next unless $s;
                        $sel->remove($s);
                        delete $peer{$s};
                        close($s);
                    }
                    %is_data = ();
                    $drop_data_at = undef;
                }

                for my $fh (@ready) {
                    if ($fh == $ctrl_srv || ($data_srv && $fh == $data_srv)) {
                        my $is_d = ($data_srv && $fh == $data_srv) ? 1 : 0;
                        my $cli = $fh->accept();
                        next unless $cli;
                        my $dst = IO::Socket::IP->new(
                            PeerHost => $host,
                            PeerPort => ($is_d ? $data_tport : $ctrl_tport),
                            Proto    => "tcp"
                        );
                        if (!$dst) { close($cli); next; }
                        $cli->blocking(0);
                        $dst->blocking(0);
                        $peer{$cli} = $dst;
                        $peer{$dst} = $cli;
                        if ($is_d) {
                            $is_data{$cli} = $cli;
                            $is_data{$dst} = $dst;
                            $drop_data_at = $now + 0.7 unless defined $drop_data_at;
                        }
                        $sel->add($cli, $dst);
                    }
                    elsif (exists $peer{$fh}) {
                        my $dst = $peer{$fh};
                        my $buf = "";
                        my $n = sysread($fh, $buf, 65536);
                        if (!defined($n) || $n <= 0) {
                            $sel->remove($fh, $dst);
                            delete $peer{$fh}; delete $peer{$dst};
                            delete $is_data{$fh}; delete $is_data{$dst};
                            close($fh); close($dst);
                            next;
                        }
                        my $off = 0;
                        while ($off < length($buf)) {
                            my $w = syswrite($dst, $buf, length($buf) - $off, $off);
                            last unless defined $w;
                            $off += $w;
                        }
                    }
                }
            }
        ' "${ADDRESS}" "${PROXY_BASE_PORT}" "${PORT}" "${PROXY_CTRL_PORT}" "${CONTROL_PORT}" \
            >"${TMP_DIR}/relay.log" 2>&1 &
        RELAY_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${PROXY_CTRL_PORT}" "tcp" 5; then
            exit 1
        fi

        CLIENT_RC=0
        "${NETPERFMETER}" "${TARGET}:${PROXY_BASE_PORT}" -control-over-tcp -runtime=3 \
            -scalar="${TMP_DIR}/eof.sca" \
            -tcp const20:const500:const20:const500:error_on_abort=off \
            >"${TMP_DIR}/client.log" 2>&1 || CLIENT_RC=$?

        if [ "${CLIENT_RC}" -ne 0 ]; then
            echo "FAIL: Client failed with error_on_abort=off after TCP data EOF (rc=${CLIENT_RC})." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi

        stop_and_check_server
        ;;

    # 5b. Mid-measurement 3-stream SCTP data EOF while control stays open (poll-spin regression)
    multistream_sctp_data_eof_no_spin)
        # Bind SCTP data on 127.0.0.1:PORT and TCP control on [::1]:(PORT+1) so a
        # delayed-close TCP proxy on 127.0.0.1:(PORT+1) can hold the server's control
        # socket open for 2.0s after the 3-stream SCTP client is killed mid-transfer.
        "${NETPERFMETER}" "${PORT}" -control-over-tcp -no-control-over-sctp -no-control-over-quic \
            -L 127.0.0.1 -l ::1 --loglevel 1 \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "::1" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi
        if ! wait_for_listener "127.0.0.1" "${PORT}" "sctp" 5; then
            exit 1
        fi

        perl -MIO::Socket::IP -MIO::Select -MTime::HiRes=time -e '
            use strict;
            use warnings;

            my ($ctrl_port) = @ARGV;
            my $ctrl_srv = IO::Socket::IP->new(
                LocalHost => "127.0.0.1", LocalPort => $ctrl_port,
                Proto => "tcp", Listen => 5, ReuseAddr => 1
            ) or die "ctrl listen: $!";
            $ctrl_srv->blocking(0);

            my $sel = IO::Select->new($ctrl_srv);
            my (%cli_to_srv, %srv_to_cli, @delayed_close);

            while (1) {
                my @ready = $sel->can_read(0.1);
                my $now = time();
                @delayed_close = grep {
                    my ($srv_sock, $close_at) = @$_;
                    if ($now >= $close_at) {
                        $sel->remove($srv_sock);
                        close($srv_sock);
                        0;
                    } else { 1; }
                } @delayed_close;

                for my $fh (@ready) {
                    if ($fh == $ctrl_srv) {
                        my $cli = $ctrl_srv->accept();
                        next unless $cli;
                        my $srv = IO::Socket::IP->new(
                            PeerHost => "::1", PeerPort => $ctrl_port, Proto => "tcp", GetAddrInfoFlags => 0
                        );
                        if (!$srv) { close($cli); next; }
                        $cli->blocking(0);
                        $srv->blocking(0);
                        $cli_to_srv{$cli} = $srv;
                        $srv_to_cli{$srv} = $cli;
                        $sel->add($cli, $srv);
                    }
                    elsif (exists $cli_to_srv{$fh}) {
                        my $srv = $cli_to_srv{$fh};
                        my $buf = "";
                        my $n = sysread($fh, $buf, 65536);
                        if (!defined($n) || $n <= 0) {
                            # Hold server control connection open for 2.0s while 3-stream SCTP socket is at EOF!
                            $sel->remove($fh);
                            delete $cli_to_srv{$fh};
                            delete $srv_to_cli{$srv};
                            close($fh);
                            push @delayed_close, [$srv, $now + 2.0];
                            next;
                        }
                        my $off = 0;
                        while ($off < length($buf)) {
                            my $w = syswrite($srv, $buf, length($buf) - $off, $off);
                            last unless defined $w;
                            $off += $w;
                        }
                    }
                    elsif (exists $srv_to_cli{$fh}) {
                        my $cli = $srv_to_cli{$fh};
                        my $buf = "";
                        my $n = sysread($fh, $buf, 65536);
                        if (!defined($n) || $n <= 0) {
                            $sel->remove($fh, $cli);
                            delete $srv_to_cli{$fh};
                            delete $cli_to_srv{$cli};
                            close($fh); close($cli);
                            next;
                        }
                        my $off = 0;
                        while ($off < length($buf)) {
                            my $w = syswrite($cli, $buf, length($buf) - $off, $off);
                            last unless defined $w;
                            $off += $w;
                        }
                    }
                }
            }
        ' "${CONTROL_PORT}" >"${TMP_DIR}/relay.log" 2>&1 &
        RELAY_PID=$!

        if ! wait_for_listener "127.0.0.1" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi

        "${NETPERFMETER}" "127.0.0.1:${PORT}" -control-over-tcp -runtime=15 \
            -sctp const10:const1000:const10:const1000 \
                  const10:const500:const10:const500 \
                  const10:const500:const10:const500 \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        _waited=0
        while [ "${_waited}" -lt 5 ]; do
            if [ -s "${TMP_DIR}/client.log" ] && grep -q "Starting measurement" "${TMP_DIR}/client.log"; then
                break
            fi
            sleep 1
            _waited=$((_waited + 1))
        done
        sleep 1

        # Abruptly kill client: kernel immediately shuts down the 3-stream SCTP association
        # while the TCP control proxy holds the server's control socket open for 2.0s!
        kill -9 "${CLIENT_PID}" 2>/dev/null || true
        wait "${CLIENT_PID}" 2>/dev/null || true
        CLIENT_PID=""
        sleep 3

        # Verify server survived with zero poll-loop spin, and can serve a second 2-stream SCTP run
        "${NETPERFMETER}" "127.0.0.1:${PORT}" -control-over-tcp -runtime=2 \
            -scalar="${TMP_DIR}/post_eof.sca" \
            -sctp const10:const500:const10:const500 const10:const500:const10:const500 \
            >"${TMP_DIR}/client2.log" 2>&1

        if [ ! -s "${TMP_DIR}/post_eof-passive.sca" ]; then
            echo "FAIL: Subsequent 2-stream SCTP run failed after multi-stream EOF." >&2
            cat "${TMP_DIR}/client2.log" >&2
            exit 1
        fi

        stop_and_check_server
        ;;

    # 5c. Silent UDP blackhole mid-measurement triggers QUIC control max_idle_timeout
    control_quic_keepalive_timeout)
        generate_ca_and_certs "qbh"

        "${NETPERFMETER}" "${PORT}" -no-control-over-tcp -no-control-over-sctp -control-over-quic \
            --loglevel 1 \
            --control-hbinterval 5000 --control-hbassocmaxrtx 1 \
            --tls-key "${TMP_DIR}/qbh_srv.key" \
            --tls-cert "${TMP_DIR}/qbh_srv.crt" \
            --tls-ca "${TMP_DIR}/qbh_ca.crt" \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "quic" 5; then
            exit 1
        fi

        PROXY_BASE_PORT=$((PORT + 4))
        PROXY_CTRL_PORT=$((PROXY_BASE_PORT + 1))

        # Forward UDP packets for QUIC control (PORT+5 <-> PORT+1) and UDP data (PORT+4 <-> PORT)
        # for 1.2s after first control packet, then silently blackhole all UDP packets
        # (keeping sockets open so no ICMP Port Unreachable is sent).
        perl -MIO::Socket::IP -MIO::Select -MTime::HiRes=time -e '
            use strict;
            use warnings;

            my ($host, $data_lport, $data_tport, $ctrl_lport, $ctrl_tport, $ready_file) = @ARGV;

            my $ctrl_listen = IO::Socket::IP->new(
                LocalHost => $host, LocalPort => $ctrl_lport, Proto => "udp", ReuseAddr => 1
            ) or die "udp ctrl listen: $!";
            my $data_listen = IO::Socket::IP->new(
                LocalHost => $host, LocalPort => $data_lport, Proto => "udp", ReuseAddr => 1
            ) or die "udp data listen: $!";

            $ctrl_listen->blocking(0);
            $data_listen->blocking(0);

            if (open(my $rf, ">", $ready_file)) { print $rf "ok\n"; close($rf); }

            my $sel = IO::Select->new($ctrl_listen, $data_listen);
            my (%cli_to_up, %up_to_cli, %up_is_ctrl);
            my $blackhole_at;

            while (1) {
                my @ready = $sel->can_read(0.1);
                my $now = time();
                my $blackhole = (defined($blackhole_at) && $now >= $blackhole_at) ? 1 : 0;

                for my $fh (@ready) {
                    my $buf = "";
                    my $sender = recv($fh, $buf, 65536, 0);
                    next unless defined $sender;
                    next if $blackhole;

                    if ($fh == $ctrl_listen || $fh == $data_listen) {
                        my $is_c = ($fh == $ctrl_listen) ? 1 : 0;
                        my $key = ($is_c ? "C:" : "D:") . $sender;
                        my $up = $cli_to_up{$key};
                        if (!$up) {
                            $up = IO::Socket::IP->new(
                                PeerHost => $host,
                                PeerPort => ($is_c ? $ctrl_tport : $data_tport),
                                Proto    => "udp"
                            ) or next;
                            $up->blocking(0);
                            $cli_to_up{$key} = $up;
                            $up_to_cli{$up}  = [$fh, $sender];
                            $up_is_ctrl{$up} = $is_c;
                            $sel->add($up);
                        }
                        if ($is_c && !defined($blackhole_at)) {
                            $blackhole_at = $now + 1.2;
                        }
                        send($up, $buf, 0);
                    }
                    elsif (exists $up_to_cli{$fh}) {
                        my ($listen_sock, $cli_addr) = @{$up_to_cli{$fh}};
                        send($listen_sock, $buf, 0, $cli_addr);
                    }
                }
            }
        ' "${ADDRESS}" "${PROXY_BASE_PORT}" "${PORT}" "${PROXY_CTRL_PORT}" "${CONTROL_PORT}" "${TMP_DIR}/udp_proxy.ready" \
            >"${TMP_DIR}/relay.log" 2>&1 &
        RELAY_PID=$!

        _w=0
        while [ ! -f "${TMP_DIR}/udp_proxy.ready" ] && [ "${_w}" -lt 50 ]; do
            perl -e 'select(undef, undef, undef, 0.1);'
            _w=$((_w + 1))
        done

        # max_idle_timeout = 400ms * 3 = 1.2s; client (-runtime=20) must abort well before 20s!
        "${NETPERFMETER}" "${TARGET}:${PROXY_BASE_PORT}" -control-over-quic \
            --control-hbinterval 5000 --control-hbassocmaxrtx 1 \
            --tls-hostname localhost \
            --tls-ca "${TMP_DIR}/qbh_ca.crt" \
            --tls-key "${TMP_DIR}/qbh_cli.key" \
            --tls-cert "${TMP_DIR}/qbh_cli.crt" \
            -runtime=25 -udp const10:const500:const0:const0 \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        _waited=0
        while kill -0 "${CLIENT_PID}" 2>/dev/null; do
            if [ "${_waited}" -ge 15 ]; then
                echo "FAIL: Active client did not time out on blackholed QUIC control connection within 15s." >&2
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

        # Allow passive server's 1.2s QUIC idle timeout to fire as well, then verify a fresh direct run
        sleep 2
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-quic \
            --control-hbinterval 5000 --control-hbassocmaxrtx 2 \
            --tls-hostname localhost \
            --tls-ca "${TMP_DIR}/qbh_ca.crt" \
            --tls-key "${TMP_DIR}/qbh_cli.key" \
            --tls-cert "${TMP_DIR}/qbh_cli.crt" \
            -runtime=2 -scalar="${TMP_DIR}/qbh_post.sca" \
            -udp const10:const500:const10:const500 \
            >"${TMP_DIR}/client2.log" 2>&1

        if [ ! -s "${TMP_DIR}/qbh_post-passive.sca" ]; then
            echo "FAIL: Server failed to handle fresh QUIC control session after blackhole timeout." >&2
            cat "${TMP_DIR}/client2.log" >&2
            exit 1
        fi

        stop_and_check_server
        ;;

    # 5d. Mid-measurement 3-stream QUIC data disconnect while TCP control stays open (no poll spin)
    multistream_quic_data_eof_no_spin)
        generate_ca_and_certs "qdeof"

        # Bind QUIC data on 127.0.0.1:(PORT-1) and TCP control on [::1]:(PORT+1) so a
        # delayed-close TCP proxy on 127.0.0.1:(PORT+1) holds the server's control socket
        # open for 2.0s after the 3-stream QUIC client terminates mid-transfer.
        "${NETPERFMETER}" "${PORT}" -control-over-tcp -no-control-over-sctp -no-control-over-quic \
            -L 127.0.0.1 -l ::1 --loglevel 1 \
            --tls-key "${TMP_DIR}/qdeof_srv.key" \
            --tls-cert "${TMP_DIR}/qdeof_srv.crt" \
            --tls-ca "${TMP_DIR}/qdeof_ca.crt" \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "::1" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi

        perl -MIO::Socket::IP -MIO::Select -MTime::HiRes=time -e '
            use strict;
            use warnings;

            my ($ctrl_port) = @ARGV;
            my $ctrl_srv = IO::Socket::IP->new(
                LocalHost => "127.0.0.1", LocalPort => $ctrl_port,
                Proto => "tcp", Listen => 5, ReuseAddr => 1
            ) or die "ctrl listen: $!";
            $ctrl_srv->blocking(0);

            my $sel = IO::Select->new($ctrl_srv);
            my (%cli_to_srv, %srv_to_cli, @delayed_close);

            while (1) {
                my @ready = $sel->can_read(0.1);
                my $now = time();
                @delayed_close = grep {
                    my ($srv_sock, $close_at) = @$_;
                    if ($now >= $close_at) {
                        $sel->remove($srv_sock);
                        close($srv_sock);
                        0;
                    } else { 1; }
                } @delayed_close;

                for my $fh (@ready) {
                    if ($fh == $ctrl_srv) {
                        my $cli = $ctrl_srv->accept();
                        next unless $cli;
                        my $srv = IO::Socket::IP->new(
                            PeerHost => "::1", PeerPort => $ctrl_port, Proto => "tcp", GetAddrInfoFlags => 0
                        );
                        if (!$srv) { close($cli); next; }
                        $cli->blocking(0);
                        $srv->blocking(0);
                        $cli_to_srv{$cli} = $srv;
                        $srv_to_cli{$srv} = $cli;
                        $sel->add($cli, $srv);
                    }
                    elsif (exists $cli_to_srv{$fh}) {
                        my $srv = $cli_to_srv{$fh};
                        my $buf = "";
                        my $n = sysread($fh, $buf, 65536);
                        if (!defined($n) || $n <= 0) {
                            $sel->remove($fh);
                            delete $cli_to_srv{$fh};
                            delete $srv_to_cli{$srv};
                            close($fh);
                            push @delayed_close, [$srv, $now + 2.0];
                            next;
                        }
                        my $off = 0;
                        while ($off < length($buf)) {
                            my $w = syswrite($srv, $buf, length($buf) - $off, $off);
                            last unless defined $w;
                            $off += $w;
                        }
                    }
                    elsif (exists $srv_to_cli{$fh}) {
                        my $cli = $srv_to_cli{$fh};
                        my $buf = "";
                        my $n = sysread($fh, $buf, 65536);
                        if (!defined($n) || $n <= 0) {
                            $sel->remove($fh, $cli);
                            delete $srv_to_cli{$fh};
                            delete $cli_to_srv{$cli};
                            close($fh); close($cli);
                            next;
                        }
                        my $off = 0;
                        while ($off < length($buf)) {
                            my $w = syswrite($cli, $buf, length($buf) - $off, $off);
                            last unless defined $w;
                            $off += $w;
                        }
                    }
                }
            }
        ' "${CONTROL_PORT}" >"${TMP_DIR}/relay.log" 2>&1 &
        RELAY_PID=$!

        if ! wait_for_listener "127.0.0.1" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi

        "${NETPERFMETER}" "127.0.0.1:${PORT}" -control-over-tcp \
            --tls-hostname localhost \
            --tls-ca "${TMP_DIR}/qdeof_ca.crt" \
            --tls-key "${TMP_DIR}/qdeof_cli.key" \
            --tls-cert "${TMP_DIR}/qdeof_cli.crt" \
            -runtime=15 \
            -quic const15:const500:const15:const500 \
                  const15:const500:const15:const500 \
                  const15:const500:const15:const500 \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        _waited=0
        while [ "${_waited}" -lt 5 ]; do
            if [ -s "${TMP_DIR}/client.log" ] && grep -q "Starting measurement" "${TMP_DIR}/client.log"; then
                break
            fi
            sleep 1
            _waited=$((_waited + 1))
        done
        sleep 1

        # Kill client: kernel QUIC socket teardown sends CONNECTION_CLOSE while TCP control stays open 2s
        kill -9 "${CLIENT_PID}" 2>/dev/null || true
        wait "${CLIENT_PID}" 2>/dev/null || true
        CLIENT_PID=""
        sleep 3

        "${NETPERFMETER}" "127.0.0.1:${PORT}" -control-over-tcp \
            --tls-hostname localhost \
            --tls-ca "${TMP_DIR}/qdeof_ca.crt" \
            --tls-key "${TMP_DIR}/qdeof_cli.key" \
            --tls-cert "${TMP_DIR}/qdeof_cli.crt" \
            -runtime=2 -scalar="${TMP_DIR}/qdeof.sca" \
            -quic const10:const500:const10:const500 const10:const500:const10:const500 \
            >"${TMP_DIR}/client2.log" 2>&1

        if [ ! -s "${TMP_DIR}/qdeof-passive.sca" ]; then
            echo "FAIL: Subsequent 2-stream QUIC run failed after 3-stream QUIC disconnect." >&2
            cat "${TMP_DIR}/client2.log" >&2
            exit 1
        fi

        stop_and_check_server
        ;;

    # 5e. NorNet regression: 3 multi-stream SCTP associations (9 flows) + QUIC control killed mid-run
    nornet_quic_ctrl_sctp_multistream_abort)
        generate_ca_and_certs "nornet"

        "${NETPERFMETER}" "${PORT}" -no-control-over-tcp -no-control-over-sctp -control-over-quic \
            --loglevel 1 \
            --control-hbinterval 5000 --control-hbassocmaxrtx 1 \
            --tls-key "${TMP_DIR}/nornet_srv.key" \
            --tls-cert "${TMP_DIR}/nornet_srv.crt" \
            --tls-ca "${TMP_DIR}/nornet_ca.crt" \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "quic" 5; then
            exit 1
        fi
        if ! wait_for_listener "${ADDRESS}" "${PORT}" "sctp" 5; then
            exit 1
        fi

        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-quic \
            --control-hbinterval 5000 --control-hbassocmaxrtx 1 \
            --tls-hostname localhost \
            --tls-ca "${TMP_DIR}/nornet_ca.crt" \
            --tls-key "${TMP_DIR}/nornet_cli.key" \
            --tls-cert "${TMP_DIR}/nornet_cli.crt" \
            -runtime=30 \
            --sctp const10:const200:const10:const200 \
                   const10:const500:const10:const500 \
                   const0:const0:const10:const1543:maxmsgsize=1400 \
            --sctp const10:const200:const10:const200 \
                   const10:const500:const10:const500 \
                   const0:const0:const10:const1543:maxmsgsize=1400 \
            --sctp const10:const200:const10:const200 \
                   const10:const500:const10:const500 \
                   const0:const0:const10:const1543:maxmsgsize=1400 \
            >"${TMP_DIR}/client1.log" 2>&1 &
        CLIENT_PID=$!

        _waited=0
        while [ "${_waited}" -lt 6 ]; do
            if [ -s "${TMP_DIR}/client1.log" ] && grep -q "Starting measurement" "${TMP_DIR}/client1.log"; then
                break
            fi
            sleep 1
            _waited=$((_waited + 1))
        done
        sleep 1

        # Kill client 1 abruptly: SCTP sockets close immediately while QUIC control
        # remains open on the server until max_idle_timeout (1.5s) expires.
        kill -9 "${CLIENT_PID}" 2>/dev/null || true
        wait "${CLIENT_PID}" 2>/dev/null || true
        CLIENT_PID=""
        sleep 6

        # Verify server stayed alive without poll-loop spin and handles a fresh run cleanly
        "${NETPERFMETER}" "${TARGET}:${PORT}" --control-over-quic \
            --control-hbinterval 5000 --control-hbassocmaxrtx 2 \
            --tls-hostname localhost \
            --tls-ca "${TMP_DIR}/nornet_ca.crt" \
            --tls-key "${TMP_DIR}/nornet_cli.key" \
            --tls-cert "${TMP_DIR}/nornet_cli.crt" \
            -runtime=2 -scalar="${TMP_DIR}/nornet2.sca" -vector="${TMP_DIR}/nornet2.vec" \
            --sctp const10:const500:const10:const500 const10:const500:const10:const500 \
            >"${TMP_DIR}/client2.log" 2>&1

        if [ ! -s "${TMP_DIR}/nornet2-passive.sca" ] || [ ! -s "${TMP_DIR}/nornet2-passive.vec" ]; then
            echo "FAIL: Follow-up run after NorNet abort scenario did not produce passive outputs." >&2
            cat "${TMP_DIR}/client2.log" >&2
            exit 1
        fi

        stop_and_check_server
        ;;

    # 5f. Port-scan / unidentified incoming connection logging (protocol name + remote IP:port)
    port_scan_peer_logging)
        "${NETPERFMETER}" "${PORT}" -control-over-tcp -no-control-over-sctp -no-control-over-quic \
            >"${TMP_DIR}/server.log" 2>&1 &
        SERVER_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "tcp" 5; then
            exit 1
        fi
        if ! wait_for_listener "${ADDRESS}" "${PORT}" "tcp" 5; then
            exit 1
        fi

        # Probe 1: TCP connect + immediate FIN close on data PORT
        FIN_PORT=$(perl -MIO::Socket::IP -e '
            my ($h, $p) = @ARGV;
            my $s = IO::Socket::IP->new(PeerHost => $h, PeerPort => $p, Proto => "tcp", GetAddrInfoFlags => 0) or die $!;
            my $lp = $s->sockport();
            close($s);
            print $lp;
        ' "${ADDRESS}" "${PORT}")

        # Probe 2: TCP connect + SO_LINGER={1,0} RST abort on data PORT
        RST_PORT=$(perl -MIO::Socket::IP -MSocket=SOL_SOCKET,SO_LINGER -e '
            my ($h, $p) = @ARGV;
            my $s = IO::Socket::IP->new(PeerHost => $h, PeerPort => $p, Proto => "tcp", GetAddrInfoFlags => 0) or die $!;
            my $lp = $s->sockport();
            setsockopt($s, SOL_SOCKET, SO_LINGER, pack("ii", 1, 0));
            close($s);
            print $lp;
        ' "${ADDRESS}" "${PORT}")

        # Probe 3: HTTP banner on TCP data PORT (triggers TLV / EOF warning + shutdown)
        HTTP_DATA_PORT=$(perl -MIO::Socket::IP -e '
            my ($h, $p) = @ARGV;
            my $s = IO::Socket::IP->new(PeerHost => $h, PeerPort => $p, Proto => "tcp", GetAddrInfoFlags => 0) or die $!;
            my $lp = $s->sockport();
            syswrite($s, "GET / HTTP/1.0\r\n\r\n");
            close($s);
            print $lp;
        ' "${ADDRESS}" "${PORT}")

        # Probe 4: Stray UDP scanner payload on UDP data PORT
        UDP_SCAN_PORT=$(perl -MIO::Socket::IP -e '
            my ($h, $p) = @ARGV;
            my $s = IO::Socket::IP->new(PeerHost => $h, PeerPort => $p, Proto => "udp", GetAddrInfoFlags => 0) or die $!;
            my $lp = $s->sockport();
            send($s, "NMAP_UDP_PROBE_PAYLOAD", 0);
            close($s);
            print $lp;
        ' "${ADDRESS}" "${PORT}")

        # Probe 5: HTTP banner on TCP control PORT+1 (triggers truncated/unrecognized control warning)
        HTTP_CTRL_PORT=$(perl -MIO::Socket::IP -e '
            my ($h, $p) = @ARGV;
            my $s = IO::Socket::IP->new(PeerHost => $h, PeerPort => $p, Proto => "tcp", GetAddrInfoFlags => 0) or die $!;
            my $lp = $s->sockport();
            syswrite($s, "GET / HTTP/1.0\r\n\r\n");
            close($s);
            print $lp;
        ' "${ADDRESS}" "${CONTROL_PORT}")

        sleep 1

        # Verify server remained healthy by running a normal TCP+UDP measurement
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=1 \
            -scalar="${TMP_DIR}/scan_ok.sca" \
            -tcp const10:const500:const10:const500 \
            -udp const10:const500:const10:const500 \
            >"${TMP_DIR}/client.log" 2>&1

        if [ ! -s "${TMP_DIR}/scan_ok-passive.sca" ]; then
            echo "FAIL: Server failed normal measurement after port-scan probes." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi

        stop_and_check_server

        # Validate protocol names and remote IP:port (127.0.0.1 or [::ffff:127.0.0.1] for IPv4; [::1] for IPv6)
        case "${ADDRESS}" in
            *:*) _addr_rx='\[::1\]' ;;
            *)   _addr_rx='(\[::ffff:127\.0\.0\.1\]|127\.0\.0\.1)' ;;
        esac

        if ! grep -Eq "End of input for unidentified flow on TCP socket [0-9]+ from ${_addr_rx}:${FIN_PORT}!" "${TMP_DIR}/server.log"; then
            echo "FAIL: Missing expected 'End of input ... on TCP socket ... from ...:${FIN_PORT}!' in server.log:" >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
        if ! grep -Eq "Shutdown of still unidentified incoming connection on TCP socket [0-9]+ from ${_addr_rx}:${FIN_PORT}!" "${TMP_DIR}/server.log"; then
            echo "FAIL: Missing expected 'Shutdown of still unidentified incoming connection on TCP socket ... from ...:${FIN_PORT}!' in server.log:" >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
        if ! grep -Eq "Shutdown of still unidentified incoming connection on TCP socket [0-9]+ from ${_addr_rx}:${RST_PORT}!" "${TMP_DIR}/server.log"; then
            echo "FAIL: Missing cached peer address on TCP RST scan (port ${RST_PORT}) in server.log:" >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
        if ! grep -Eq "on TCP socket [0-9]+ from ${_addr_rx}:${HTTP_DATA_PORT}!" "${TMP_DIR}/server.log"; then
            echo "FAIL: Missing TCP data HTTP banner warning (port ${HTTP_DATA_PORT}) in server.log:" >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
        if ! grep -Eq "Received garbage on UDP socket [0-9]+ from ${_addr_rx}:${UDP_SCAN_PORT}!" "${TMP_DIR}/server.log"; then
            echo "FAIL: Missing UDP probe garbage warning (port ${UDP_SCAN_PORT}) in server.log:" >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
        if ! grep -Eq "on TCP socket [0-9]+ from ${_addr_rx}:${HTTP_CTRL_PORT}!" "${TMP_DIR}/server.log"; then
            echo "FAIL: Missing TCP control HTTP banner warning (port ${HTTP_CTRL_PORT}) in server.log:" >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
        ;;

    *)
        echo "FAIL: Unknown scenario '${SCENARIO}'" >&2
        exit 1
        ;;
esac

echo "PASS: Edge case test (${SCENARIO}) passed."
exit 0
