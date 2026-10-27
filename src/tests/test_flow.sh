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
#   test_flow.sh <bin> <port> <addr> <proto> <spec> [format] [runtime] [extra_args...]

set -eu

NETPERFMETER="${1:-netperfmeter}"
PORT="${2:-25000}"
ADDRESS="${3:-127.0.0.1}"
PROTOCOL="${4:-tcp}"
SPEC="${5:-const10:const1000:const10:const1000}"
FORMAT="${6:-plain}"
RUNTIME="${7:-5}"

ENABLE_SCALAR=1
ENABLE_VECTOR=1
PASSTHROUGH_ARGS=""

if [ "$#" -ge 7 ]; then
    shift 7
    for arg in "$@"; do
        case "$arg" in
            --no-scalar|--without-scalar) ENABLE_SCALAR=0 ;;
            --no-vector|--without-vector) ENABLE_VECTOR=0 ;;
            *) PASSTHROUGH_ARGS="${PASSTHROUGH_ARGS} $arg" ;;
        esac
    done
fi

CONTROL_PORT=$((PORT + 1))

case "${FORMAT}" in
    gz|gzip)     EXT=".gz" ;;
    bz2|bzip2)   EXT=".bz2" ;;
    xz|lzma)     EXT=".xz" ;;
    zst|zstd)    EXT=".zst" ;;
    plain|none)  EXT="" ;;
    .*)          EXT="${FORMAT}" ;;
    *)           EXT=".${FORMAT}" ;;
esac

case "${ADDRESS}" in
    *:*) TARGET="[${ADDRESS}]" ;;
    *)   TARGET="${ADDRESS}" ;;
esac

# Deduce directional activity and fragmentation expectations from SPEC
OLD_IFS="$IFS"
IFS=":"
set -- ${SPEC}
IFS="$OLD_IFS"

OUT_RATE="${1:-0}"
OUT_SIZE="${2:-0}"
IN_RATE="${3:-0}"
IN_SIZE="${4:-0}"

# Check for maxmsgsize in options ($5, $6, ...)
MAXMSGSIZE=0
shift 4 2>/dev/null || true
while [ "$#" -gt 0 ]; do
    case "$1" in
        maxmsgsize=*)
            MAXMSGSIZE="${1#maxmsgsize=}"
            ;;
    esac
    shift
done

case "${OUT_RATE}:${OUT_SIZE}" in
    0:0|const0:0|0:const0|const0:const0) OUT_ACTIVE=0 ;;
    *) OUT_ACTIVE=1 ;;
esac

case "${IN_RATE}:${IN_SIZE}" in
    0:0|const0:0|0:const0|const0:const0) IN_ACTIVE=0 ;;
    *) IN_ACTIVE=1 ;;
esac

ACTIVE_EXPECT_TX="${OUT_ACTIVE}"
ACTIVE_EXPECT_RX="${IN_ACTIVE}"
PASSIVE_EXPECT_TX="${IN_ACTIVE}"
PASSIVE_EXPECT_RX="${OUT_ACTIVE}"

# Extract numeric frame sizes (e.g., const2000 -> 2000)
OUT_SIZE_NUM=$(echo "${OUT_SIZE}" | tr -dc '0-9' || echo 0)
IN_SIZE_NUM=$(echo "${IN_SIZE}" | tr -dc '0-9' || echo 0)
OUT_SIZE_NUM="${OUT_SIZE_NUM:-0}"
IN_SIZE_NUM="${IN_SIZE_NUM:-0}"

# Fragmentation flags: frame size strictly greater than maxmsgsize
ACTIVE_FRAG_TX=0
ACTIVE_FRAG_RX=0
PASSIVE_FRAG_TX=0
PASSIVE_FRAG_RX=0

if [ "${MAXMSGSIZE}" -gt 0 ]; then
    if [ "${OUT_ACTIVE}" -eq 1 ] && [ "${OUT_SIZE_NUM}" -gt "${MAXMSGSIZE}" ]; then
        ACTIVE_FRAG_TX=1
        PASSIVE_FRAG_RX=1
    fi
    if [ "${IN_ACTIVE}" -eq 1 ] && [ "${IN_SIZE_NUM}" -gt "${MAXMSGSIZE}" ]; then
        ACTIVE_FRAG_RX=1
        PASSIVE_FRAG_TX=1
    fi
fi

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_test')

SERVER_PID=""
SUCCESS=0
cleanup() {
    if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill -TERM "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    if [ "${SUCCESS}" -eq 1 ]; then
        rm -rf "${TMP_DIR}"
    else
        echo "Test failed; artifacts preserved in ${TMP_DIR}" >&2
    fi
}
trap cleanup EXIT INT TERM

decompress_cat() {
    _file="$1"
    case "${_file}" in
        *.gz)         gzip -dc "${_file}" ;;
        *.bz2)        bzip2 -dc "${_file}" ;;
        *.xz)         xz -dc "${_file}" ;;
        *.zst|*.zstd) zstd -dc "${_file}" ;;
        *)            cat "${_file}" ;;
    esac
}

# ====== Check Scalar Results ==============================================
check_scalar() {
    _role="$1"
    _expect_tx="$2"
    _expect_rx="$3"
    _frag_tx="$4"
    _frag_rx="$5"
    _file="${TMP_DIR}/test-${_role}.sca${EXT}"

    if [ ! -s "${_file}" ]; then
        echo "FAIL: Scalar output '${_file}' is missing or empty." >&2
        return 1
    fi

    if ! decompress_cat "${_file}" | perl -e '
        use strict;
        use warnings;

        my ($filename, $expect_tx, $expect_rx, $frag_tx, $frag_rx) = @ARGV;
        my $line_num = 0;
        my (%stats, $errors);

        while (<STDIN>) {
            $line_num++;
            if (/^scalar\s+"([^"]+)"\s+"(Transmitted|Received)\s+(Bytes|Packets|Frames)"\s+([0-9.]+)/) {
                my ($obj, $dir, $metric, $val) = ($1, $2, $3, $4 + 0);
                $stats{$obj}{$dir}{$metric} = $val;
            }
        }

        foreach my $obj (keys %stats) {
            foreach my $dir ("Transmitted", "Received") {
                my $expected  = ($dir eq "Transmitted") ? $expect_tx : $expect_rx;
                my $expect_fg = ($dir eq "Transmitted") ? $frag_tx   : $frag_rx;

                my $b = (defined $stats{$obj}{$dir}{Bytes})   ? $stats{$obj}{$dir}{Bytes}   : 0;
                my $p = (defined $stats{$obj}{$dir}{Packets}) ? $stats{$obj}{$dir}{Packets} : 0;
                my $f = (defined $stats{$obj}{$dir}{Frames})  ? $stats{$obj}{$dir}{Frames}  : 0;

                if ($expected) {
                    if ($b <= 0 || $p <= 0 || $f <= 0) {
                        warn "FAIL in $filename (obj $obj): Expected $dir Bytes/Packets/Frames > 0 (got B=$b, P=$p, F=$f)\n";
                        $errors++;
                    }
                    if ($b < $p) {
                        warn "FAIL in $filename (obj $obj): Invariant violation: $dir Bytes ($b) < Packets ($p)\n";
                        $errors++;
                    }
                    if ($p < $f) {
                        warn "FAIL in $filename (obj $obj): Invariant violation: $dir Packets ($p) < Frames ($f)\n";
                        $errors++;
                    }
                    if ($expect_fg) {
                        if ($p <= $f) {
                            warn "FAIL in $filename (obj $obj): Framing failure: Expected $dir Packets ($p) > Frames ($f) under fragmentation\n";
                            $errors++;
                        }
                    } else {
                        if ($p != $f) {
                            warn "FAIL in $filename (obj $obj): Unfragmented flow expected Packets ($p) == Frames ($f)\n";
                            $errors++;
                        }
                    }
                } else {
                    if ($b != 0 || $p != 0 || $f != 0) {
                        warn "FAIL in $filename (obj $obj): Expected zero $dir metrics (got B=$b, P=$p, F=$f)\n";
                        $errors++;
                    }
                }
            }
        }

        exit($errors ? 1 : 0);
    ' "${_file}" "${_expect_tx}" "${_expect_rx}" "${_frag_tx}" "${_frag_rx}"; then
        echo "FAIL: Scalar verification failed for '${_file}'." >&2
        echo "--- Content of ${_file} ---" >&2
        decompress_cat "${_file}" >&2
        return 1
    fi
}

# ====== Check Vector Results (Global and Per-Flow) ========================
check_vector() {
    _role="$1"
    _expect_tx="$2"
    _expect_rx="$3"
    _frag_tx="$4"
    _frag_rx="$5"

    _global_file="${TMP_DIR}/test-${_role}.vec${EXT}"
    if [ ! -s "${_global_file}" ]; then
        echo "FAIL: Global vector output '${_global_file}' is missing or empty." >&2
        return 1
    fi

    set -- "${TMP_DIR}"/test-"${_role}"-*.vec"${EXT}"
    if [ ! -e "$1" ] || [ ! -s "$1" ]; then
        echo "FAIL: Per-flow vector output for role '${_role}' (test-${_role}-*.vec${EXT}) is missing or empty." >&2
        return 1
    fi

    for _file in "${_global_file}" "$@"; do
        if [ ! -s "${_file}" ]; then
            continue
        fi

        if ! decompress_cat "${_file}" | grep -q "^AbsTime"; then
            echo "FAIL in ${_file}: Missing valid 'AbsTime' header." >&2
            echo "--- Content of ${_file} ---" >&2
            decompress_cat "${_file}" >&2
            return 1
        fi

        if ! decompress_cat "${_file}" | perl -e '
            use strict;
            use warnings;

            my ($filename, $expect_tx, $expect_rx, $frag_tx, $frag_rx) = @ARGV;
            my $line_num = 0;
            my $is_flow_vector = 0;
            my $data_rows = 0;
            my $errors = 0;

            my ($last_seq, $last_b, $last_p, $last_f);
            my (%max_b, %max_p, %max_f, %last_meas_b, %last_meas_p, %last_meas_f);

            while (<STDIN>) {
                $line_num++;
                chomp;
                next if /^\s*$/;

                if (/^AbsTime/) {
                    $is_flow_vector = 1 if /SeqNumber/;
                    next;
                }

                my @fields = split(/\t/);
                $data_rows++;

                if ($is_flow_vector) {
                    if (@fields < 7) {
                        warn "FAIL in $filename (line $line_num): Malformed flow vector row\n";
                        $errors++;
                        next;
                    }

                    my $seq   = $fields[3] + 0;
                    my $bytes = $fields[4] + 0;
                    my $pkts  = $fields[5] + 0;
                    my $frms  = $fields[6] + 0;

                    if (!$expect_rx) {
                        warn "FAIL in $filename (line $line_num): Unexpected data row when expect_rx == 0\n";
                        $errors++;
                    }

                    # Plausibility hierarchy
                    if ($bytes < $pkts) {
                        warn "FAIL in $filename (line $line_num): AbsBytes ($bytes) < AbsPackets ($pkts)\n";
                        $errors++;
                    }
                    if ($pkts < $frms) {
                        warn "FAIL in $filename (line $line_num): AbsPackets ($pkts) < AbsFrames ($frms)\n";
                        $errors++;
                    }

                    # Monotonicity
                    if (defined $last_seq && $seq < $last_seq) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic SeqNumber ($seq < $last_seq)\n";
                        $errors++;
                    }
                    if (defined $last_b && $bytes < $last_b) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic AbsBytes ($bytes < $last_b)\n";
                        $errors++;
                    }
                    if (defined $last_p && $pkts < $last_p) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic AbsPackets ($pkts < $last_p)\n";
                        $errors++;
                    }
                    if (defined $last_f && $frms < $last_f) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic AbsFrames ($frms < $last_f)\n";
                        $errors++;
                    }

                    $last_seq = $seq;
                    $last_b   = $bytes;
                    $last_p   = $pkts;
                    $last_f   = $frms;

                } else {
                    next if @fields < 11;
                    my $flow_id = $fields[4];
                    my $action  = $fields[7];
                    $action =~ s/^"|"$//g;

                    next unless ($action eq "Sent" || $action eq "Received");

                    my $bytes = $fields[8]  + 0;
                    my $pkts  = $fields[9]  + 0;
                    my $frms  = $fields[10] + 0;

                    if ($bytes < $pkts) {
                        warn "FAIL in $filename (line $line_num): $action AbsBytes ($bytes) < AbsPackets ($pkts)\n";
                        $errors++;
                    }
                    if ($pkts < $frms) {
                        warn "FAIL in $filename (line $line_num): $action AbsPackets ($pkts) < AbsFrames ($frms)\n";
                        $errors++;
                    }

                    if (!defined $max_b{$action} || $bytes > $max_b{$action}) { $max_b{$action} = $bytes; }
                    if (!defined $max_p{$action} || $pkts  > $max_p{$action}) { $max_p{$action} = $pkts; }
                    if (!defined $max_f{$action} || $frms  > $max_f{$action}) { $max_f{$action} = $frms; }

                    my $key = "$action:$flow_id";
                    if (defined $last_meas_b{$key} && $bytes < $last_meas_b{$key}) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic $action AbsBytes\n";
                        $errors++;
                    }
                    if (defined $last_meas_p{$key} && $pkts < $last_meas_p{$key}) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic $action AbsPackets\n";
                        $errors++;
                    }
                    if (defined $last_meas_f{$key} && $frms < $last_meas_f{$key}) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic $action AbsFrames\n";
                        $errors++;
                    }

                    $last_meas_b{$key} = $bytes;
                    $last_meas_p{$key} = $pkts;
                    $last_meas_f{$key} = $frms;
                }
            }

            if ($is_flow_vector) {
                if ($expect_rx) {
                    if ($data_rows == 0) {
                        warn "FAIL in $filename (line $line_num): Expected received trace data, got 0 rows\n";
                        $errors++;
                    }
                    if ($frag_rx && defined $last_p && defined $last_f && $last_p <= $last_f) {
                        warn "FAIL in $filename (line $line_num): Framing failure: Expected AbsPackets ($last_p) > AbsFrames ($last_f)\n";
                        $errors++;
                    }
                } else {
                    if ($data_rows > 0) {
                        warn "FAIL in $filename (line $line_num): Expected 0 data rows when expect_rx == 0\n";
                        $errors++;
                    }
                }
            } else {
                foreach my $dir ("Sent", "Received") {
                    my $expected  = ($dir eq "Sent") ? $expect_tx : $expect_rx;
                    my $expect_fg = ($dir eq "Sent") ? $frag_tx   : $frag_rx;

                    my $b = (defined $max_b{$dir}) ? $max_b{$dir} : 0;
                    my $p = (defined $max_p{$dir}) ? $max_p{$dir} : 0;
                    my $f = (defined $max_f{$dir}) ? $max_f{$dir} : 0;

                    if ($expected) {
                        if ($b <= 0 || $p <= 0 || $f <= 0) {
                            warn "FAIL in $filename (line $line_num): Expected max $dir B/P/F > 0 (got B=$b, P=$p, F=$f)\n";
                            $errors++;
                        }
                        if ($expect_fg && $p <= $f) {
                            warn "FAIL in $filename (line $line_num): Expected $dir Packets ($p) > Frames ($f) under fragmentation\n";
                            $errors++;
                        }
                    } else {
                        if ($b != 0 || $p != 0 || $f != 0) {
                            warn "FAIL in $filename (line $line_num): Expected zero $dir traffic (got B=$b, P=$p, F=$f)\n";
                            $errors++;
                        }
                    }
                }
            }

            exit($errors ? 1 : 0);
        ' "${_file}" "${_expect_tx}" "${_expect_rx}" "${_frag_tx}" "${_frag_rx}"; then
            echo "FAIL: Vector verification failed for '${_file}'." >&2
            echo "--- Content of ${_file} ---" >&2
            decompress_cat "${_file}" >&2
            return 1
        fi
    done
}

# ====== Listener Readiness Probe (Perl IO::Socket::IP) =====================
wait_for_listener() {
    _host="$1"
    _port="$2"
    _timeout="${3:-5}"
    _elapsed=0

    while [ "$_elapsed" -lt "$_timeout" ]; do
        if [ -n "${SERVER_PID}" ] && ! kill -0 "${SERVER_PID}" 2>/dev/null; then
            echo "FAIL: Passive server process (PID ${SERVER_PID}) terminated early." >&2
            if [ -s "${TMP_DIR}/server.log" ]; then
                echo "--- Server Log ---" >&2
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

    echo "FAIL: Passive server failed to listen on ${_host}:${_port} within ${_timeout}s." >&2
    if [ -s "${TMP_DIR}/server.log" ]; then
        echo "--- Server Log ---" >&2
        cat "${TMP_DIR}/server.log" >&2
    fi
    return 1
}

"${NETPERFMETER}" "${PORT}" -no-control-over-sctp >"${TMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" 5

SCALAR_ARG=""
VECTOR_ARG=""
if [ "${ENABLE_SCALAR}" -eq 1 ]; then
    SCALAR_ARG="-scalar=${TMP_DIR}/test.sca${EXT}"
fi
if [ "${ENABLE_VECTOR}" -eq 1 ]; then
    VECTOR_ARG="-vector=${TMP_DIR}/test.vec${EXT}"
fi

if ! "${NETPERFMETER}" "${TARGET}:${PORT}" \
    -control-over-tcp \
    -runtime="${RUNTIME}" \
    "-${PROTOCOL}" "${SPEC}" \
    --config="${TMP_DIR}/test.config" \
    ${SCALAR_ARG} \
    ${VECTOR_ARG} \
    ${PASSTHROUGH_ARGS} \
    >"${TMP_DIR}/client.log" 2>&1; then
    echo "FAIL: Active instance exited with an error." >&2
    echo "--- Client Log ---" >&2
    cat "${TMP_DIR}/client.log" >&2
    echo "--- Server Log ---" >&2
    cat "${TMP_DIR}/server.log" >&2
    exit 1
fi

if [ ! -s "${TMP_DIR}/test.config" ]; then
    echo "FAIL: Configuration file (--config) missing or empty." >&2
    exit 1
fi

if [ "${ENABLE_SCALAR}" -eq 1 ]; then
    check_scalar "active"  "${ACTIVE_EXPECT_TX}"  "${ACTIVE_EXPECT_RX}"  "${ACTIVE_FRAG_TX}"  "${ACTIVE_FRAG_RX}"
    check_scalar "passive" "${PASSIVE_EXPECT_TX}" "${PASSIVE_EXPECT_RX}" "${PASSIVE_FRAG_TX}" "${PASSIVE_FRAG_RX}"
else
    set -- "${TMP_DIR}"/test-*.sca*
    if [ -e "$1" ]; then
        echo "FAIL: Scalar output '$1' was generated, but --no-scalar was requested." >&2
        exit 1
    fi
fi

if [ "${ENABLE_VECTOR}" -eq 1 ]; then
    check_vector "active"  "${ACTIVE_EXPECT_TX}"  "${ACTIVE_EXPECT_RX}"  "${ACTIVE_FRAG_TX}"  "${ACTIVE_FRAG_RX}"
    check_vector "passive" "${PASSIVE_EXPECT_TX}" "${PASSIVE_EXPECT_RX}" "${PASSIVE_FRAG_TX}" "${PASSIVE_FRAG_RX}"
else
    set -- "${TMP_DIR}"/test-*.vec*
    if [ -e "$1" ]; then
        echo "FAIL: Vector output '$1' was generated, but --no-vector was requested." >&2
        exit 1
    fi
fi

SUCCESS=1
echo "PASS: ${PROTOCOL} flow test (Port: ${PORT}, Host: ${ADDRESS}, Spec: ${SPEC}, Format: ${FORMAT}) passed."
exit 0
