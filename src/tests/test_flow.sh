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
#
# Examples:
#   test_flow.sh ./netperfmeter 25000 127.0.0.1 tcp const10:const1000:const10:const1000 plain 2
#   test_flow.sh ./netperfmeter 25010 ::1       udp const10:const1000:const0:const0     zst   2

set -eu

NETPERFMETER="${1:-netperfmeter}"
PORT="${2:-25000}"
ADDRESS="${3:-127.0.0.1}"
PROTOCOL="${4:-tcp}"
SPEC="${5:-const10:const1000:const10:const1000}"
FORMAT="${6:-plain}"
RUNTIME="${7:-5}"

if [ "$#" -ge 7 ]; then
    shift 7
    EXTRA_ARGS="$*"
else
    EXTRA_ARGS=""
fi

# NetPerfMeter control channel runs on BP + 1
CONTROL_PORT=$((PORT + 1))

# Resolve file extension according to chosen compression format
case "${FORMAT}" in
    gz|gzip)     EXT=".gz" ;;
    bz2|bzip2)   EXT=".bz2" ;;
    xz|lzma)     EXT=".xz" ;;
    zst|zstd)    EXT=".zst" ;;
    plain|none)  EXT="" ;;
    .*)          EXT="${FORMAT}" ;;
    *)           EXT=".${FORMAT}" ;;
esac

# Format endpoint for IPv4 vs IPv6
case "${ADDRESS}" in
    *:*) TARGET="[${ADDRESS}]" ;;
    *)   TARGET="${ADDRESS}" ;;
esac

# Deduce expected directional activity from SPEC (out_rate:out_size:in_rate:in_size)
OLD_IFS="$IFS"
IFS=":"
set -- ${SPEC}
IFS="$OLD_IFS"

OUT_RATE="${1:-0}"
OUT_SIZE="${2:-0}"
IN_RATE="${3:-0}"
IN_SIZE="${4:-0}"

# Active TX -> Passive RX
case "${OUT_RATE}:${OUT_SIZE}" in
    0:0|const0:0|0:const0|const0:const0) OUT_ACTIVE=0 ;;
    *) OUT_ACTIVE=1 ;;
esac

# Passive TX -> Active RX
case "${IN_RATE}:${IN_SIZE}" in
    0:0|const0:0|0:const0|const0:const0) IN_ACTIVE=0 ;;
    *) IN_ACTIVE=1 ;;
esac

ACTIVE_EXPECT_TX="${OUT_ACTIVE}"
ACTIVE_EXPECT_RX="${IN_ACTIVE}"
PASSIVE_EXPECT_TX="${IN_ACTIVE}"
PASSIVE_EXPECT_RX="${OUT_ACTIVE}"

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

# ====== Decompression Helper ==============================================
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
    _file="${TMP_DIR}/test-${_role}.sca${EXT}"

    if [ ! -s "${_file}" ]; then
        echo "FAIL: Scalar output '${_file}' is missing or empty." >&2
        return 1
    fi

    # Verify that Transmitted and Received metrics strictly match > 0 vs == 0
    if ! decompress_cat "${_file}" | perl -e '
        use strict;
        use warnings;

        my ($filename, $expect_tx, $expect_rx) = @ARGV;
        my $line_num = 0;
        my %count;
        my $errors = 0;

        while (<STDIN>) {
            $line_num++;
            if (/^scalar\s+"[^"]+"\s+"(Transmitted|Received)\s+([^"]+)"\s+([0-9.]+)/) {
                my ($dir, $metric, $raw_val) = ($1, $2, $3);
                my $val = $raw_val + 0.0;
                my $expected = ($dir eq "Transmitted") ? $expect_tx : $expect_rx;
                $count{$dir}++;

                if ($expected) {
                    if ($val <= 0) {
                        warn "FAIL in $filename (line $line_num): Expected $dir $metric > 0, got $raw_val\n";
                        $errors++;
                    }
                } else {
                    if ($val != 0) {
                        warn "FAIL in $filename (line $line_num): Expected $dir $metric == 0, got $raw_val\n";
                        $errors++;
                    }
                }
            }
        }

        if (!$count{Transmitted}) {
            warn "FAIL in $filename (line $line_num): No \"Transmitted\" scalar metrics found\n";
            $errors++;
        }
        if (!$count{Received}) {
            warn "FAIL in $filename (line $line_num): No \"Received\" scalar metrics found\n";
            $errors++;
        }

        exit($errors ? 1 : 0);
    ' "${_file}" "${_expect_tx}" "${_expect_rx}"; then
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

    # 1. Global (measurement-wide) vector file
    _global_file="${TMP_DIR}/test-${_role}.vec${EXT}"
    if [ ! -s "${_global_file}" ]; then
        echo "FAIL: Global vector output '${_global_file}' is missing or empty." >&2
        return 1
    fi

    # 2. Per-flow vector file(s)
    set -- "${TMP_DIR}"/test-"${_role}"-*.vec"${EXT}"
    if [ ! -e "$1" ] || [ ! -s "$1" ]; then
        echo "FAIL: Per-flow vector output for role '${_role}' (test-${_role}-*.vec${EXT}) is missing or empty." >&2
        return 1
    fi

    # 3. Validate each vector file (global and all per-flow files)
    for _file in "${_global_file}" "$@"; do
        if [ ! -s "${_file}" ]; then
            continue
        fi

        # Header check
        if ! decompress_cat "${_file}" | grep -q "^AbsTime"; then
            echo "FAIL in ${_file}: Missing valid 'AbsTime' header." >&2
            echo "--- Content of ${_file} ---" >&2
            decompress_cat "${_file}" >&2
            return 1
        fi

        # Comprehensive data & monotonicity verification
        if ! decompress_cat "${_file}" | perl -e '
            use strict;
            use warnings;

            my ($filename, $expect_tx, $expect_rx) = @ARGV;
            my $line_num = 0;
            my $is_flow_vector = 0;
            my $data_rows = 0;
            my $errors = 0;

            # Tracking for flow vector files (packet arrival log)
            my ($last_seq, $last_abs_b, $last_abs_p, $last_abs_f);

            # Tracking for measurement vector files (summary snapshots)
            my (%max_abs_bytes, %last_meas_bytes, %last_meas_pkts, %last_meas_frames);

            while (<STDIN>) {
                $line_num++;
                chomp;
                next if /^\s*$/;

                # Header detection
                if (/^AbsTime/) {
                    if (/SeqNumber/) {
                        $is_flow_vector = 1;
                    }
                    next;
                }

                my @fields = split(/\t/);
                $data_rows++;

                if ($is_flow_vector) {
                    # --- Per-Flow Vector File ---
                    # Col 0: Line, 1: AbsTime, 2: RelTime, 3: SeqNumber,
                    # Col 4: AbsBytes, 5: AbsPackets, 6: AbsFrames, 7: RelBytes...
                    if (@fields < 7) {
                        warn "FAIL in $filename (line $line_num): Malformed flow vector row (expected >= 7 columns, got " . scalar(@fields) . ")\n";
                        $errors++;
                        next;
                    }

                    my $seq   = $fields[3] + 0;
                    my $bytes = $fields[4] + 0;
                    my $pkts  = $fields[5] + 0;
                    my $frms  = $fields[6] + 0;

                    # Flow vector files record received packets
                    if (!$expect_rx) {
                        warn "FAIL in $filename (line $line_num): Unexpected data row when expect_rx == 0 (AbsBytes: $bytes)\n";
                        $errors++;
                    }

                    # Monotonicity checks
                    if (defined $last_seq && $seq < $last_seq) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic SeqNumber ($seq < $last_seq)\n";
                        $errors++;
                    }
                    if (defined $last_abs_b && $bytes < $last_abs_b) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic AbsBytes ($bytes < $last_abs_b)\n";
                        $errors++;
                    }
                    if (defined $last_abs_p && $pkts < $last_abs_p) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic AbsPackets ($pkts < $last_abs_p)\n";
                        $errors++;
                    }
                    if (defined $last_abs_f && $frms < $last_abs_f) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic AbsFrames ($frms < $last_abs_f)\n";
                        $errors++;
                    }

                    $last_seq   = $seq;
                    $last_abs_b = $bytes;
                    $last_abs_p = $pkts;
                    $last_abs_f = $frms;

                } else {
                    # --- Global / Measurement-Wide Vector File ---
                    # Col 4: FlowID, 7: Action ("Sent"|"Received"|"Lost"),
                    # Col 8: AbsBytes, 9: AbsPackets, 10: AbsFrames...
                    next if @fields < 11;
                    my $flow_id = $fields[4];
                    my $action  = $fields[7];
                    $action =~ s/^"|"$//g;

                    next unless ($action eq "Sent" || $action eq "Received");

                    my $bytes = $fields[8]  + 0;
                    my $pkts  = $fields[9]  + 0;
                    my $frms  = $fields[10] + 0;

                    if (!defined $max_abs_bytes{$action} || $bytes > $max_abs_bytes{$action}) {
                        $max_abs_bytes{$action} = $bytes;
                    }

                    my $key = "$action:$flow_id";
                    if (defined $last_meas_bytes{$key} && $bytes < $last_meas_bytes{$key}) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic $action AbsBytes ($bytes < $last_meas_bytes{$key})\n";
                        $errors++;
                    }
                    if (defined $last_meas_pkts{$key} && $pkts < $last_meas_pkts{$key}) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic $action AbsPackets ($pkts < $last_meas_pkts{$key})\n";
                        $errors++;
                    }
                    if (defined $last_meas_frames{$key} && $frms < $last_meas_frames{$key}) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic $action AbsFrames ($frms < $last_meas_frames{$key})\n";
                        $errors++;
                    }

                    $last_meas_bytes{$key}  = $bytes;
                    $last_meas_pkts{$key}   = $pkts;
                    $last_meas_frames{$key} = $frms;
                }
            }

            # Completeness and direction checks
            if ($is_flow_vector) {
                if ($expect_rx) {
                    if ($data_rows == 0) {
                        warn "FAIL in $filename (line $line_num): Expected received data rows, but file had 0 data rows\n";
                        $errors++;
                    } elsif (($last_abs_b // 0) <= 0) {
                        warn "FAIL in $filename (line $line_num): Expected final AbsBytes > 0, got " . ($last_abs_b // 0) . "\n";
                        $errors++;
                    }
                } else {
                    if ($data_rows > 0) {
                        warn "FAIL in $filename (line $line_num): Expected 0 data rows when expect_rx == 0, got $data_rows rows\n";
                        $errors++;
                    }
                }
            } else {
                if ($expect_tx) {
                    if (($max_abs_bytes{Sent} // 0) <= 0) {
                        warn "FAIL in $filename (line $line_num): Expected Sent traffic > 0, max AbsBytes was " . ($max_abs_bytes{Sent} // 0) . "\n";
                        $errors++;
                    }
                } else {
                    if (($max_abs_bytes{Sent} // 0) != 0) {
                        warn "FAIL in $filename (line $line_num): Expected Sent traffic == 0, max AbsBytes was $max_abs_bytes{Sent}\n";
                        $errors++;
                    }
                }

                if ($expect_rx) {
                    if (($max_abs_bytes{Received} // 0) <= 0) {
                        warn "FAIL in $filename (line $line_num): Expected Received traffic > 0, max AbsBytes was " . ($max_abs_bytes{Received} // 0) . "\n";
                        $errors++;
                    }
                } else {
                    if (($max_abs_bytes{Received} // 0) != 0) {
                        warn "FAIL in $filename (line $line_num): Expected Received traffic == 0, max AbsBytes was $max_abs_bytes{Received}\n";
                        $errors++;
                    }
                }
            }

            exit($errors ? 1 : 0);
        ' "${_file}" "${_expect_tx}" "${_expect_rx}"; then
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

# 1. Start passive node (server) listening on PORT (control on PORT+1 via TCP)
"${NETPERFMETER}" "${PORT}" -no-control-over-sctp >"${TMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

# Wait until control channel (PORT + 1) is ready
wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" 5

# 2. Run active node (client)
if ! "${NETPERFMETER}" "${TARGET}:${PORT}" \
    -control-over-tcp \
    -runtime="${RUNTIME}" \
    "-${PROTOCOL}" "${SPEC}" \
    --config="${TMP_DIR}/test.config" \
    -scalar="${TMP_DIR}/test.sca${EXT}" \
    -vector="${TMP_DIR}/test.vec${EXT}" \
    ${EXTRA_ARGS} \
    >"${TMP_DIR}/client.log" 2>&1; then
    echo "FAIL: Active instance exited with an error." >&2
    echo "--- Client Log ---" >&2
    cat "${TMP_DIR}/client.log" >&2
    echo "--- Server Log ---" >&2
    cat "${TMP_DIR}/server.log" >&2
    exit 1
fi

# 3. Verify Configuration output (--config)
if [ ! -s "${TMP_DIR}/test.config" ]; then
    echo "FAIL: Configuration file (--config) missing or empty." >&2
    exit 1
fi

# 4. Verify Scalar outputs with expected directional constraints
check_scalar "active"  "${ACTIVE_EXPECT_TX}"  "${ACTIVE_EXPECT_RX}"
check_scalar "passive" "${PASSIVE_EXPECT_TX}" "${PASSIVE_EXPECT_RX}"

# 5. Verify Vector outputs with expected directional constraints
check_vector "active"  "${ACTIVE_EXPECT_TX}"  "${ACTIVE_EXPECT_RX}"
check_vector "passive" "${PASSIVE_EXPECT_TX}" "${PASSIVE_EXPECT_RX}"

SUCCESS=1
echo "PASS: ${PROTOCOL} flow test (Port: ${PORT}, Host: ${ADDRESS}, Spec: ${SPEC}, Format: ${FORMAT}) passed."
exit 0
