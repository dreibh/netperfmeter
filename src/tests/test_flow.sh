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
MAX_RETRIES=1
PASSTHROUGH_ARGS=""

# Automatic retry budget: random distributions (exp, pareto, uniform, truncnormal, normal) can produce
# statistical outliers, especially in small sample sizes; allow up to 5 retries.
# Purely deterministic tests (const only) run strictly once.
case "${SPEC}" in
    *exp*|*pareto*|*uniform*|*truncnormal*|*normal*)
        MAX_RETRIES=5
        ;;
esac

if [ "$#" -ge 7 ]; then
    shift 7
    for arg in "$@"; do
        case "$arg" in
            --no-scalar|--without-scalar) ENABLE_SCALAR=0 ;;
            --no-vector|--without-vector) ENABLE_VECTOR=0 ;;
            --retries=*|--max-retries=*)  MAX_RETRIES="${arg#*=}" ;;
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

# Deduce directional activity and distributions from SPEC
OLD_IFS="$IFS"
IFS=":"
set -- ${SPEC}
IFS="$OLD_IFS"

OUT_RATE="${1:-0}"
OUT_SIZE="${2:-0}"
IN_RATE="${3:-0}"
IN_SIZE="${4:-0}"

# Check for framing (maxmsgsize) and on-off scheduling (onoff) in options ($5, $6, ...)
MAXMSGSIZE=0
ONOFF_SPEC=""
HAS_ONOFF=0
ONOFF_ACTIVE_IN_RUNTIME=0

shift 4 2>/dev/null || true
while [ "$#" -gt 0 ]; do
    case "$1" in
        maxmsgsize=*)
            MAXMSGSIZE="${1#maxmsgsize=}"
            ;;
        onoff=*|events=*)
            HAS_ONOFF=1
            ONOFF_SPEC="${1#*=}"
            # onoff specifies relative durations (d1, d2, ...).
            # Convert durations into timeline transitions to verify if any active window starts before RUNTIME.
            if perl -e '
                my ($dur_str, $runtime) = @ARGV;
                my @durs = split(/,/, $dur_str);
                my $cum_t = 0.0;
                for (my $i = 0; $i < @durs; $i++) {
                    $cum_t += ($durs[$i] + 0);
                    # $i=0 is Off duration, $i=1 is On duration, $i=2 is Off duration, etc.
                    if ($i % 2 == 0 && $cum_t < ($runtime + 0)) {
                        exit 0;
                    }
                }
                exit 1;
            ' "${ONOFF_SPEC}" "${RUNTIME}"; then
                ONOFF_ACTIVE_IN_RUNTIME=1
            fi
            ;;
    esac
    shift
done

case "${OUT_RATE}:${OUT_SIZE}" in
    0:0|const0:0|0:const0|const0:const0) OUT_ACTIVE=0 ;;
    *)
        # If on-off events exist but no active interval starts before RUNTIME, no bytes are sent
        if [ "${HAS_ONOFF}" -eq 1 ] && [ "${ONOFF_ACTIVE_IN_RUNTIME}" -eq 0 ]; then
            OUT_ACTIVE=0
        else
            OUT_ACTIVE=1
        fi
        ;;
esac

case "${IN_RATE}:${IN_SIZE}" in
    0:0|const0:0|0:const0|const0:const0) IN_ACTIVE=0 ;;
    *) IN_ACTIVE=1 ;;
esac

ACTIVE_EXPECT_TX="${OUT_ACTIVE}"
ACTIVE_EXPECT_RX="${IN_ACTIVE}"
PASSIVE_EXPECT_TX="${IN_ACTIVE}"
PASSIVE_EXPECT_RX="${OUT_ACTIVE}"

parse_mean() {
    _dist="$1"
    perl -e '
        my $d = $ARGV[0] // "0";
        if ($d =~ /^const([0-9.]+)/) { print $1; }
        elsif ($d =~ /^uniform([0-9.]+),([0-9.]+)/) { print (($1 + $2) / 2.0); }
        elsif ($d =~ /^exp([0-9.]+)/) { print $1; }
        elsif ($d =~ /^pareto([0-9.]+),([0-9.]+)/) {
            my ($k, $a) = ($1 + 0, $2 + 0);
            print ($a > 1.0 ? ($a * $k) / ($a - 1.0) : $k);
        }
        elsif ($d =~ /^(?:trunc)?normal([0-9.]+),([0-9.]+)/) { print $1; }
        else { print 0; }
    ' "${_dist}"
}

OUT_SIZE_MEAN=$(parse_mean "${OUT_SIZE}")
IN_SIZE_MEAN=$(parse_mean "${IN_SIZE}")

ACTIVE_FRAG_TX=0
ACTIVE_FRAG_RX=0
PASSIVE_FRAG_TX=0
PASSIVE_FRAG_RX=0

if [ "${MAXMSGSIZE}" -gt 0 ]; then
    if [ "${OUT_ACTIVE}" -eq 1 ] && perl -e "exit(!(${OUT_SIZE_MEAN} > ${MAXMSGSIZE}))"; then
        ACTIVE_FRAG_TX=1
        PASSIVE_FRAG_RX=1
    fi
    if [ "${IN_ACTIVE}" -eq 1 ] && perl -e "exit(!(${IN_SIZE_MEAN} > ${MAXMSGSIZE}))"; then
        ACTIVE_FRAG_RX=1
        PASSIVE_FRAG_TX=1
    fi
fi

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
# Returns:
#   0 = Success
#   1 = Fatal failure (crash, invariant failure, deterministic const deviation) -> DO NOT RETRY
#   2 = Statistical value comparison deviation on random distributions -> RETRYABLE
check_scalar() {
    _role="$1"
    _expect_tx="$2"
    _expect_rx="$3"
    _frag_tx="$4"
    _frag_rx="$5"
    _dist_tx_rate="$6"
    _dist_tx_size="$7"
    _dist_rx_rate="$8"
    _dist_rx_size="$9"
    _protocol="${10:-$PROTOCOL}"
    _onoff="${11:-}"
    _runtime="${12:-$RUNTIME}"
    _file="${TMP_DIR}/test-${_role}.sca${EXT}"

    if [ ! -s "${_file}" ]; then
        echo "FAIL: Scalar output '${_file}' is missing or empty." >&2
        return 1
    fi

    _scalar_eval_rc=0
    decompress_cat "${_file}" | perl -e '
        use strict;
        use warnings;

        my ($filename, $expect_tx, $expect_rx, $frag_tx, $frag_rx,
            $dist_tx_rate, $dist_tx_size, $dist_rx_rate, $dist_rx_size,
            $protocol, $onoff_str, $runtime) = @ARGV;
        my $line_num = 0;
        my (%stats, %rates);
        my ($fatal_errors, $stat_errors) = (0, 0);

        # Compute total active time during [0, runtime] using cumulative durations
        sub compute_on_time {
            my ($dur_str, $rt) = @_;
            return $rt if !$dur_str;
            my @durs = split(/,/, $dur_str);
            my $on_time = 0.0;
            my $state = 0; # 0 = OFF, 1 = ON
            my $prev_t = 0.0;
            my $cum_t = 0.0;

            foreach my $d (@durs) {
                $cum_t += ($d + 0);
                my $t = ($cum_t < $rt) ? $cum_t : ($rt + 0);
                if ($t > $prev_t) {
                    $on_time += ($t - $prev_t) if $state == 1;
                    $prev_t = $t;
                }
                $state = 1 - $state;
                last if $cum_t >= $rt;
            }
            if ($prev_t < $rt && $state == 1) {
                $on_time += ($rt - $prev_t);
            }
            return $on_time;
        }

        my $on_time = compute_on_time($onoff_str, $runtime + 0);
        my $duty_cycle = ($runtime > 0) ? ($on_time / $runtime) : 1.0;

        sub parse_dist {
            my ($dist, $is_rate) = @_;
            return (0, 0) if !$dist || $dist eq "0";

            if ($dist =~ /^const([0-9.]+)/) {
                return ($1 + 0, 0.20);
            } elsif ($dist =~ /^uniform([0-9.]+),([0-9.]+)/) {
                my ($a, $b) = ($1 + 0, $2 + 0);
                if ($is_rate) {
                    my $hm = ($b > $a && $a > 0) ? ($b - $a) / log($b / $a) : ($a + $b) / 2.0;
                    return ($hm, 0.20);
                } else {
                    return (($a + $b) / 2.0, 0.20);
                }
            } elsif ($dist =~ /^exp([0-9.]+)/) {
                my $mean = $1 + 0;
                return ($is_rate ? (0, 0) : ($mean, 0.40));
            } elsif ($dist =~ /^pareto([0-9.]+),([0-9.]+)/) {
                my ($k, $a) = ($1 + 0, $2 + 0);
                if ($is_rate) {
                    my $hm = ($a > 0) ? (($a + 1.0) / $a) * $k : $k;
                    return ($hm, 0.30);
                } else {
                    my $am = ($a > 1.0) ? ($a * $k) / ($a - 1.0) : $k;
                    return ($am, 0.35);
                }
            } elsif ($dist =~ /^(?:trunc)?normal([0-9.]+),([0-9.]+)/) {
                return ($1 + 0, 0.25);
            }
            return (0, 0);
        }

        while (<STDIN>) {
            $line_num++;
            if (/^scalar\s+"([^"]+)"\s+"(Transmitted|Received)\s+(Bytes|Packets|Frames)"\s+([0-9.]+)/) {
                $stats{$1}{$2}{$3} = $4 + 0;
            } elsif (/^scalar\s+"([^"]+)"\s+"(Transmitted|Received)\s+Frame Rate"\s+([0-9.]+)/) {
                $rates{$1}{$2} = $3 + 0;
            }
        }

        foreach my $obj (keys %stats) {
            foreach my $dir ("Transmitted", "Received") {
                my $expected  = ($dir eq "Transmitted") ? $expect_tx : $expect_rx;
                my $expect_fg = ($dir eq "Transmitted") ? $frag_tx   : $frag_rx;
                my $dist_r    = ($dir eq "Transmitted") ? $dist_tx_rate : $dist_rx_rate;
                my $dist_s    = ($dir eq "Transmitted") ? $dist_tx_size : $dist_rx_size;

                my $b = (defined $stats{$obj}{$dir}{Bytes})   ? $stats{$obj}{$dir}{Bytes}   : 0;
                my $p = (defined $stats{$obj}{$dir}{Packets}) ? $stats{$obj}{$dir}{Packets} : 0;
                my $f = (defined $stats{$obj}{$dir}{Frames})  ? $stats{$obj}{$dir}{Frames}  : 0;
                my $r = (defined $rates{$obj}{$dir})          ? $rates{$obj}{$dir}          : 0;

                my $is_rand_r = ($dist_r =~ /^(?:exp|pareto|uniform|truncnormal|normal)/);
                my $is_rand_s = ($dist_s =~ /^(?:exp|pareto|uniform|truncnormal|normal)/);
                my $flow_has_random = ($is_rand_r || $is_rand_s);

                if ($expected) {
                    if ($b <= 0 || $p <= 0 || $f <= 0) {
                        warn "FAIL in $filename ($obj): Expected $dir B/P/F > 0 (got B=$b, P=$p, F=$f)\n";
                        $fatal_errors++;
                    }
                    if ($b < $p) {
                        warn "FAIL in $filename ($obj): Invariant violation: $dir Bytes ($b) < Packets ($p)\n";
                        $fatal_errors++;
                    }
                    if ($b < $f) {
                        warn "FAIL in $filename ($obj): Invariant violation: $dir Bytes ($b) < Frames ($f)\n";
                        $fatal_errors++;
                    }

                    # Protocol-aware Packet vs. Frame checks:
                    # On TCP, byte-stream socket coalescing allows Packets < Frames on BOTH Transmitted and Received.
                    # On UDP, datagram boundaries require Packets >= Frames.
                    if ($protocol ne "tcp") {
                        if ($p < $f) {
                            if ($flow_has_random && ($f - $p) <= 2) {
                                # Minor datagram loss under heavy -j128 saturation: retryable
                                $stat_errors++;
                            } else {
                                warn "FAIL in $filename ($obj): Invariant violation: UDP $dir Packets ($p) < Frames ($f)\n";
                                $fatal_errors++;
                            }
                        }
                    }

                    if ($expect_fg) {
                        if ($p <= $f) {
                            warn "FAIL in $filename ($obj): Expected $dir Packets ($p) > Frames ($f) under fragmentation\n";
                            $fatal_errors++;
                        }
                    } else {
                        # Constant frame sizes on UDP require exactly 1 packet per frame
                        if ($protocol ne "tcp" && $dist_s =~ /^const/) {
                            if ($p != $f) {
                                warn "FAIL in $filename ($obj): Expected unfragmented UDP Packets ($p) == Frames ($f)\n";
                                $fatal_errors++;
                            }
                        }
                    }

                    my ($target_rate, $tol_rate) = parse_dist($dist_r, 1);
                    my ($target_size, $tol_size) = parse_dist($dist_s, 0);

                    # Small-sample statistical correction (standard error scales with 1/sqrt(N)):
                    # When sample size is very low (N < 20 frames), relative sample variance is higher
                    my $eff_tol_size = ($f > 0 && $f < 20) ? ($tol_size * 1.5) : $tol_size;
                    my $eff_tol_rate = ($runtime > 0 && $runtime <= 8 && $f < 20) ? ($tol_rate * 1.5) : $tol_rate;

                    # On-Off flow validation
                    if ($onoff_str && $target_rate > 0) {
                        my $cont_frames = $target_rate * $runtime;
                        my $expected_onoff_frames = $target_rate * $on_time;

                        # If flow had off periods, total transferred frames must be strictly lower than continuous
                        if ($on_time < $runtime) {
                            my $upper_bound = $cont_frames * 0.90;
                            if ($f > $upper_bound) {
                                warn "FAIL in $filename ($obj): On-off flow transmitted $f frames, expected < $upper_bound (Continuous: $cont_frames)\n";
                                $fatal_errors++;
                            }
                        }

                        # Frame count must converge to active on-time duration
                        if ($expected_onoff_frames > 0) {
                            my $diff = abs($f - $expected_onoff_frames);
                            if ($diff > ($expected_onoff_frames * ($eff_tol_rate + 0.15))) {
                                warn "FAIL in $filename ($obj): Measured frames ($f) deviated from expected ($expected_onoff_frames) for On-time ${on_time}s\n";
                                if ($flow_has_random) {
                                    $stat_errors++;
                                } else {
                                    $fatal_errors++;
                                }
                            }
                        }

                        # In total statistics, the time-averaged rate over the full runtime must be reduced
                        if ($obj =~ /\.total$/ && $on_time < $runtime) {
                            if ($r > ($target_rate * 0.90)) {
                                warn "FAIL in $filename ($obj): Time-averaged frame rate ($r) not lower than continuous rate ($target_rate)\n";
                                $fatal_errors++;
                            }
                        }
                    } else {
                        # Standard continuous flow rate validation
                        if ($obj =~ /flow\[/ && $target_rate > 0) {
                            my $diff = abs($r - $target_rate);
                            if ($diff > ($target_rate * $eff_tol_rate)) {
                                my $pct = sprintf("%.1f%%", ($diff / $target_rate) * 100);
                                warn "FAIL in $filename ($obj): $dir Frame Rate ($r) deviated from expected ($target_rate) by $pct\n";
                                if ($flow_has_random) {
                                    $stat_errors++;
                                } else {
                                    $fatal_errors++;
                                }
                            }
                        }
                    }

                    # Average frame size check during active transmissions
                    if ($target_size > 0 && $f > 0) {
                        my $avg_size = $b / $f;
                        my $diff = abs($avg_size - $target_size);
                        if ($diff > ($target_size * $eff_tol_size)) {
                            my $pct = sprintf("%.1f%%", ($diff / $target_size) * 100);
                            warn "FAIL in $filename ($obj): $dir Avg Frame Size ($avg_size) deviated from expected ($target_size) by $pct\n";
                            if ($is_rand_s) {
                                $stat_errors++;
                            } else {
                                $fatal_errors++;
                            }
                        }
                    }
                } else {
                    if ($b != 0 || $p != 0 || $f != 0) {
                        warn "FAIL in $filename ($obj): Expected zero $dir metrics (got B=$b, P=$p, F=$f)\n";
                        $fatal_errors++;
                    }
                }
            }
        }

        if ($fatal_errors > 0) {
            exit 1;
        } elsif ($stat_errors > 0) {
            exit 2;
        } else {
            exit 0;
        }
    ' "${_file}" "${_expect_tx}" "${_expect_rx}" "${_frag_tx}" "${_frag_rx}" \
      "${_dist_tx_rate}" "${_dist_tx_size}" "${_dist_rx_rate}" "${_dist_rx_size}" \
      "${_protocol}" "${_onoff}" "${_runtime}" || _scalar_eval_rc=$?

    if [ "${_scalar_eval_rc}" -eq 2 ]; then
        echo "WARNING: Statistical value comparison deviation detected in '${_file}'." >&2
        return 2
    elif [ "${_scalar_eval_rc}" -ne 0 ]; then
        echo "FAIL: Scalar verification failed for '${_file}'." >&2
        echo "--- Content of ${_file} ---" >&2
        decompress_cat "${_file}" >&2
        return 1
    fi
}

# ====== Check Vector Results (Global and Per-Flow) ========================
# All vector checks test structural invariants (monotonicity, framing, intervals).
# Any failure in vector verification is fatal (return 1).
check_vector() {
    _role="$1"
    _expect_tx="$2"
    _expect_rx="$3"
    _frag_tx="$4"
    _frag_rx="$5"
    _onoff="${6:-}"
    _protocol="${7:-$PROTOCOL}"

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

            my ($filename, $expect_tx, $expect_rx, $frag_tx, $frag_rx, $onoff_str, $role, $protocol) = @ARGV;
            my $line_num = 0;
            my $is_flow_vector = 0;
            my $data_rows = 0;
            my $errors = 0;

            my ($last_seq, $last_b, $last_p, $last_f);
            my (%max_b, %max_p, %max_f, %last_meas_b, %last_meas_p, %last_meas_f);

            sub interval_active_time {
                my ($dur_str, $t_start, $t_end) = @_;
                return ($t_end - $t_start) if !$dur_str;
                my @durs = split(/,/, $dur_str);
                my $state = 0;
                my $prev_t = 0.0;
                my $cum_t = 0.0;
                my $act = 0.0;

                foreach my $d (@durs) {
                    $cum_t += ($d + 0);
                    if ($cum_t > $t_start) {
                        my $sub_start = ($t_start > $prev_t) ? $t_start : $prev_t;
                        my $sub_end   = ($t_end < $cum_t)     ? $t_end   : $cum_t;
                        if ($sub_end > $sub_start && $state == 1) {
                            $act += ($sub_end - $sub_start);
                        }
                    }
                    $prev_t = $cum_t;
                    $state = 1 - $state;
                    last if $cum_t >= $t_end;
                }
                if ($prev_t < $t_end && $state == 1) {
                    my $sub_start = ($t_start > $prev_t) ? $t_start : $prev_t;
                    $act += ($t_end - $sub_start) if $t_end > $sub_start;
                }
                return $act;
            }

            sub is_near_active_boundary {
                my ($dur_str, $t_start) = @_;
                return 0 if !$dur_str;
                my @durs = split(/,/, $dur_str);
                my $state = 0;
                my $cum_t = 0.0;

                foreach my $d (@durs) {
                    $cum_t += ($d + 0);
                    if ($state == 1) {
                        # Transition from ON -> OFF at $cum_t
                        if ($t_start >= $cum_t && ($t_start - $cum_t) <= 0.100) {
                            return 1;
                        }
                    }
                    $state = 1 - $state;
                }
                return 0;
            }

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

                    if ($bytes < $pkts) {
                        warn "FAIL in $filename (line $line_num): AbsBytes ($bytes) < AbsPackets ($pkts)\n";
                        $errors++;
                    }
                    if ($protocol ne "tcp" && $pkts < $frms) {
                        warn "FAIL in $filename (line $line_num): AbsPackets ($pkts) < AbsFrames ($frms)\n";
                        $errors++;
                    }

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
                    my $rel_time = $fields[2] + 0;
                    my $interval = $fields[3] + 0;
                    my $flow_id  = $fields[4];
                    my $action   = $fields[7];
                    $action =~ s/^"|"$//g;

                    next unless ($action eq "Sent" || $action eq "Received");

                    my $bytes     = $fields[8]  + 0;
                    my $pkts      = $fields[9]  + 0;
                    my $frms      = $fields[10] + 0;
                    my $rel_bytes = (@fields > 11) ? ($fields[11] + 0) : 0;
                    my $rel_pkts  = (@fields > 12) ? ($fields[12] + 0) : 0;
                    my $rel_frms  = (@fields > 13) ? ($fields[13] + 0) : 0;

                    if ($bytes < $pkts) {
                        warn "FAIL in $filename (line $line_num): $action AbsBytes ($bytes) < AbsPackets ($pkts)\n";
                        $errors++;
                    }
                    # TCP byte-stream socket write/read coalescing allows Packets < Frames on both Sent and Received
                    if ($protocol ne "tcp" && $pkts < $frms) {
                        warn "FAIL in $filename (line $line_num): $action AbsPackets ($pkts) < AbsFrames ($frms)\n";
                        $errors++;
                    }

                    if (!defined $max_b{$action} || $bytes > $max_b{$action}) { $max_b{$action} = $bytes; }
                    if (!defined $max_p{$action} || $pkts  > $max_p{$action}) { $max_p{$action} = $pkts; }
                    if (!defined $max_f{$action} || $frms  > $max_f{$action}) { $max_f{$action} = $frms; }

                    # Active transmitter sender check: strictly 0 sent during OFF intervals,
                    # allowing at most 1 trailing boundary frame if the interval starts within 100ms of an active window
                    if ($onoff_str && $action eq "Sent" && $role eq "active" && $interval > 0) {
                        my $t_start = ($rel_time > $interval) ? ($rel_time - $interval) : 0.0;
                        my $t_end   = $rel_time;
                        my $act     = interval_active_time($onoff_str, $t_start, $t_end);

                        if ($act == 0.0) {
                            my $is_boundary = is_near_active_boundary($onoff_str, $t_start);
                            if ($is_boundary) {
                                if ($rel_frms > 1) {
                                    warn "FAIL in $filename (line $line_num): Continuous traffic ($rel_frms frames) sent during OFF period [$t_start, $t_end]\n";
                                    $errors++;
                                }
                            } else {
                                if ($rel_frms != 0 || $rel_bytes != 0) {
                                    warn "FAIL in $filename (line $line_num): Traffic sent during OFF period [$t_start, $t_end] (RelFrames=$rel_frms, RelBytes=$rel_bytes)\n";
                                    $errors++;
                                }
                            }
                        }
                    }

                    my $key = "$action:$flow_id";
                    if (defined $last_meas_b{$key} && $bytes < $last_meas_b{$key}) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic $action AbsBytes ($bytes < $last_meas_b{$key})\n";
                        $errors++;
                    }
                    if (defined $last_meas_p{$key} && $pkts < $last_meas_p{$key}) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic $action AbsPackets ($pkts < $last_meas_p{$key})\n";
                        $errors++;
                    }
                    if (defined $last_meas_f{$key} && $frms < $last_meas_f{$key}) {
                        warn "FAIL in $filename (line $line_num): Non-monotonic $action AbsFrames ($frms < $last_meas_f{$key})\n";
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
        ' "${_file}" "${_expect_tx}" "${_expect_rx}" "${_frag_tx}" "${_frag_rx}" "${_onoff}" "${_role}" "${_protocol}"; then
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

# ====== Single Attempt Execution ==========================================
# Returns:
#   0 = Success
#   1 = Fatal failure (abort immediately)
#   2 = Statistical failure (retryable)
run_attempt() {
    _attempt="$1"

    TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_test')
    SERVER_PID=""

    stop_server() {
        if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
            kill -TERM "${SERVER_PID}" 2>/dev/null || true
            wait "${SERVER_PID}" 2>/dev/null || true
        fi
        SERVER_PID=""
    }

    # 1. Start passive node (server) listening on PORT (control on PORT+1 via TCP)
    "${NETPERFMETER}" "${PORT}" -no-control-over-sctp >"${TMP_DIR}/server.log" 2>&1 &
    SERVER_PID=$!

    # 2. Wait until control channel (PORT + 1) is ready
    if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" 5; then
        stop_server
        return 1
    fi

    # 3. Run active node (client)
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
        cat "${TMP_DIR}/client.log" >&2
        stop_server
        return 1
    fi

    stop_server

    # 4. Verify Configuration output (--config)
    if [ ! -s "${TMP_DIR}/test.config" ]; then
        echo "FAIL: Configuration file (--config) missing or empty." >&2
        return 1
    fi

    # 5. Verify Scalar outputs
    if [ "${ENABLE_SCALAR}" -eq 1 ]; then
        _scalar_rc=0
        check_scalar "active"  "${ACTIVE_EXPECT_TX}"  "${ACTIVE_EXPECT_RX}"  "${ACTIVE_FRAG_TX}"  "${ACTIVE_FRAG_RX}" \
                          "${OUT_RATE}" "${OUT_SIZE}" "${IN_RATE}" "${IN_SIZE}" "${PROTOCOL}" "${ONOFF_SPEC}" "${RUNTIME}" || _scalar_rc=$?
        if [ "${_scalar_rc}" -ne 0 ]; then
            return "${_scalar_rc}"
        fi

        check_scalar "passive" "${PASSIVE_EXPECT_TX}" "${PASSIVE_EXPECT_RX}" "${PASSIVE_FRAG_TX}" "${PASSIVE_FRAG_RX}" \
                          "${IN_RATE}" "${IN_SIZE}" "${OUT_RATE}" "${OUT_SIZE}" "${PROTOCOL}" "${ONOFF_SPEC}" "${RUNTIME}" || _scalar_rc=$?
        if [ "${_scalar_rc}" -ne 0 ]; then
            return "${_scalar_rc}"
        fi
    else
        set -- "${TMP_DIR}"/test-*.sca*
        if [ -e "$1" ]; then
            echo "FAIL: Scalar output '$1' was generated, but --no-scalar was requested." >&2
            return 1
        fi
    fi

    # 6. Verify Vector outputs
    if [ "${ENABLE_VECTOR}" -eq 1 ]; then
        if ! check_vector "active"  "${ACTIVE_EXPECT_TX}"  "${ACTIVE_EXPECT_RX}"  "${ACTIVE_FRAG_TX}"  "${ACTIVE_FRAG_RX}" "${ONOFF_SPEC}" "${PROTOCOL}"; then
            return 1
        fi
        if ! check_vector "passive" "${PASSIVE_EXPECT_TX}" "${PASSIVE_EXPECT_RX}" "${PASSIVE_FRAG_TX}" "${PASSIVE_FRAG_RX}" "${ONOFF_SPEC}" "${PROTOCOL}"; then
            return 1
        fi
    else
        set -- "${TMP_DIR}"/test-*.vec*
        if [ -e "$1" ]; then
            echo "FAIL: Vector output '$1' was generated, but --no-vector was requested." >&2
            return 1
        fi
    fi

    return 0
}

# ====== Main Execution Loop with Retries ==================================
TMP_DIR=""
SERVER_PID=""
cleanup_exit() {
    if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill -TERM "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
}
trap cleanup_exit EXIT INT TERM

ATTEMPT=1
SUCCESS=0

while [ "${ATTEMPT}" -le "${MAX_RETRIES}" ]; do
    _attempt_rc=0
    run_attempt "${ATTEMPT}" || _attempt_rc=$?

    if [ "${_attempt_rc}" -eq 0 ]; then
        SUCCESS=1
        rm -rf "${TMP_DIR}"
        break
    elif [ "${_attempt_rc}" -eq 2 ]; then
        # Retry ONLY allowed for statistical value comparison failures on random distributions
        echo "WARNING: Attempt ${ATTEMPT}/${MAX_RETRIES} for ${PROTOCOL} flow (${SPEC}) failed due to statistical outlier in random distribution." >&2
        if [ "${ATTEMPT}" -lt "${MAX_RETRIES}" ]; then
            rm -rf "${TMP_DIR}"
            sleep 1
        else
            echo "All ${MAX_RETRIES} attempts failed; artifacts preserved in ${TMP_DIR}" >&2
        fi
    else
        # Fatal failure: binary crash, abort, connection break, invariant failure, framing error, etc.
        # DO NOT RETRY!
        echo "FAIL: Attempt ${ATTEMPT} for ${PROTOCOL} flow (${SPEC}) failed with non-retryable error (exit code ${_attempt_rc}). Aborting." >&2
        echo "Artifacts preserved in ${TMP_DIR}" >&2
        exit 1
    fi
    ATTEMPT=$((ATTEMPT + 1))
done

if [ "${SUCCESS}" -eq 1 ]; then
    echo "PASS: ${PROTOCOL} flow test (Port: ${PORT}, Host: ${ADDRESS}, Spec: ${SPEC}) passed after ${ATTEMPT} attempt(s)."
    exit 0
else
    exit 1
fi
