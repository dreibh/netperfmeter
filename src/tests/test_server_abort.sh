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
#   test_server_abort.sh <netperfmeter_bin> <port> <address> <mode>

set -eu

NETPERFMETER="${1:-netperfmeter}"
PORT="${2:-25000}"
ADDRESS="${3:-127.0.0.1}"
MODE="${4:-error_on_abort_on}"

CONTROL_PORT=$((PORT + 1))

case "${ADDRESS}" in
    *:*) TARGET="[${ADDRESS}]" ;;
    *)   TARGET="${ADDRESS}" ;;
esac

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_abort_test')
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
    _timeout="${3:-5}"
    _elapsed=0

    while [ "$_elapsed" -lt "$_timeout" ]; do
        if [ -n "${SERVER_PID}" ] && ! kill -0 "${SERVER_PID}" 2>/dev/null; then
            echo "FAIL: Passive server process terminated early." >&2
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

# 1. Start passive server
"${NETPERFMETER}" "${PORT}" -control-over-tcp -no-control-over-sctp >"${TMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" 5; then
    exit 1
fi
if ! wait_for_listener "${ADDRESS}" "${PORT}" 5; then
    exit 1
fi

case "${MODE}" in
    error_on_abort_on)
        # Active client runs with error_on_abort=on and runtime=10s
        "${NETPERFMETER}" "${TARGET}:${PORT}" -control-over-tcp -runtime=10 \
            -tcp const20:const1000:const10:const1000:error_on_abort=on \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        # Allow bidirectional traffic to establish and run, then terminate server
        sleep 1
        kill -INT "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
        SERVER_PID=""

        # Client must abort promptly with a clean non-zero exit code (not SIGABRT/SIGSEGV)
        _waited=0
        while kill -0 "${CLIENT_PID}" 2>/dev/null; do
            if [ "${_waited}" -ge 6 ]; then
                echo "FAIL: Active client did not abort within 6s after server stopped." >&2
                cat "${TMP_DIR}/client.log" >&2
                exit 1
            fi
            sleep 1
            _waited=$((_waited + 1))
        done

        CLIENT_RC=0
        wait "${CLIENT_PID}" || CLIENT_RC=$?
        CLIENT_PID=""

        if [ "${CLIENT_RC}" -eq 0 ]; then
            echo "FAIL: Expected non-zero exit code when error_on_abort=on and server stopped." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi
        if [ "${CLIENT_RC}" -ge 128 ]; then
            echo "FAIL: Active client terminated via signal (exit code ${CLIENT_RC})." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi
        if grep -qE "terminate called|Aborted|Avbrutt|SIGABRT|Segmentation fault|assure" "${TMP_DIR}/client.log"; then
            echo "FAIL: Crash or assertion failure detected in active client log." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi
        ;;

    error_on_abort_off)
        # Relay TCP control traffic (PORT + 3 -> CONTROL_PORT) throughout the run,
        # and relay UDP data traffic (PORT + 2 -> PORT) during the initial flow
        # identification handshake before closing the UDP relay socket mid-transfer
        # so subsequent client UDP sends trigger ECONNREFUSED under error_on_abort=off.
        PROXY_DATA_PORT=$((PORT + 2))
        PROXY_CTRL_PORT=$((PORT + 3))

        perl -MIO::Socket::IP -MIO::Select -e '
            use strict;
            use warnings;

            my ($listen_host, $udp_listen_port, $udp_target_port, $tcp_listen_port, $tcp_target_port) = @ARGV;

            my $tcp_srv = IO::Socket::IP->new(
                LocalHost => $listen_host,
                LocalPort => $tcp_listen_port,
                Proto     => "tcp",
                Listen    => 5,
                ReuseAddr => 1
            ) or die "tcp listen: $!";

            my $udp_srv = IO::Socket::IP->new(
                LocalHost => $listen_host,
                LocalPort => $udp_listen_port,
                Proto     => "udp",
                ReuseAddr => 1
            ) or die "udp listen: $!";

            my $udp_upstream = IO::Socket::IP->new(
                PeerHost => $listen_host,
                PeerPort => $udp_target_port,
                Proto    => "udp"
            ) or die "udp connect: $!";

            $tcp_srv->blocking(0);
            $udp_srv->blocking(0);
            $udp_upstream->blocking(0);

            my $sel = IO::Select->new($tcp_srv, $udp_srv, $udp_upstream);
            my %tcp_peer;
            my $client_udp_addr;
            my $udp_packets_relayed = 0;

            while (my @ready = $sel->can_read(10)) {
                for my $fh (@ready) {
                    if ($fh == $tcp_srv) {
                        my $cli = $tcp_srv->accept();
                        next unless $cli;
                        my $dst = IO::Socket::IP->new(
                            PeerHost => $listen_host,
                            PeerPort => $tcp_target_port,
                            Proto    => "tcp"
                        );
                        if (!$dst) {
                            close($cli);
                            next;
                        }
                        $cli->blocking(0);
                        $dst->blocking(0);
                        $tcp_peer{$cli} = $dst;
                        $tcp_peer{$dst} = $cli;
                        $sel->add($cli, $dst);
                    }
                    elsif ($udp_srv && $fh == $udp_srv) {
                        my $buf = "";
                        my $sender = recv($udp_srv, $buf, 65536, 0);
                        if (defined $sender) {
                            $client_udp_addr = $sender;
                            send($udp_upstream, $buf, 0);
                            $udp_packets_relayed++;
                            if ($udp_packets_relayed >= 3) {
                                $sel->remove($udp_srv, $udp_upstream);
                                close($udp_srv);
                                close($udp_upstream);
                                $udp_srv = undef;
                                $udp_upstream = undef;
                            }
                        }
                    }
                    elsif ($udp_upstream && $fh == $udp_upstream) {
                        my $buf = "";
                        my $sender = recv($udp_upstream, $buf, 65536, 0);
                        if (defined($sender) && defined($client_udp_addr) && $udp_srv) {
                            send($udp_srv, $buf, 0, $client_udp_addr);
                        }
                    }
                    elsif (exists $tcp_peer{$fh}) {
                        my $peer = $tcp_peer{$fh};
                        my $buf = "";
                        my $n = sysread($fh, $buf, 65536);
                        if (!defined($n) || $n <= 0) {
                            $sel->remove($fh, $peer);
                            delete $tcp_peer{$fh};
                            delete $tcp_peer{$peer};
                            close($fh);
                            close($peer);
                            next;
                        }
                        my $off = 0;
                        while ($off < length($buf)) {
                            my $w = syswrite($peer, $buf, length($buf) - $off, $off);
                            if (!defined($w)) {
                                $sel->remove($fh, $peer);
                                delete $tcp_peer{$fh};
                                delete $tcp_peer{$peer};
                                close($fh);
                                close($peer);
                                last;
                            }
                            $off += $w;
                        }
                    }
                }
            }
        ' "${ADDRESS}" "${PROXY_DATA_PORT}" "${PORT}" "${PROXY_CTRL_PORT}" "${CONTROL_PORT}" >"${TMP_DIR}/relay.log" 2>&1 &
        RELAY_PID=$!

        if ! wait_for_listener "${ADDRESS}" "${PROXY_CTRL_PORT}" 5; then
            exit 1
        fi

        "${NETPERFMETER}" "${TARGET}:${PROXY_DATA_PORT}" -control-over-tcp -runtime=2 \
            -udp const20:const1000:const0:const0:error_on_abort=off \
            >"${TMP_DIR}/client.log" 2>&1 &
        CLIENT_PID=$!

        CLIENT_RC=0
        wait "${CLIENT_PID}" || CLIENT_RC=$?
        CLIENT_PID=""

        kill -INT "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
        SERVER_PID=""

        if [ "${CLIENT_RC}" -ne 0 ]; then
            echo "FAIL: Expected clean exit code 0 with error_on_abort=off, got ${CLIENT_RC}." >&2
            echo "--- Client Log ---" >&2
            cat "${TMP_DIR}/client.log" >&2
            echo "--- Server Log ---" >&2
            cat "${TMP_DIR}/server.log" >&2
            exit 1
        fi
        if grep -qE "FATAL ERROR|terminate called|Aborted|Avbrutt|SIGABRT|Segmentation fault|assure" "${TMP_DIR}/client.log"; then
            echo "FAIL: Unexpected fatal error or crash in active client log." >&2
            cat "${TMP_DIR}/client.log" >&2
            exit 1
        fi
        ;;

    *)
        echo "FAIL: Unknown mode '${MODE}'" >&2
        exit 1
        ;;
esac

echo "PASS: Mid-session server abort test (${MODE}) passed."
exit 0
