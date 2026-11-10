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
CONTROL_OVER_SCTP=0
EXPECTED_FLOWS=1
GENERAL_EXTRA_ARGS=""
STREAM_ARGS=""
EXTRA_CONN_ARGS=""

ALL_FLOW_PROTOS="${PROTOCOL}"
ALL_FLOW_SPECS="${SPEC}"

# Automatic retry budget for stochastic specs
case "${SPEC}" in
    *exp*|*pareto*|*uniform*|*truncnormal*|*normal*)
        MAX_RETRIES=5
        ;;
esac

if [ "$#" -ge 7 ]; then
    shift 7
    while [ "$#" -gt 0 ]; do
        case "$1" in
            --no-scalar|--without-scalar) ENABLE_SCALAR=0 ;;
            --no-vector|--without-vector) ENABLE_VECTOR=0 ;;
            --retries=*|--max-retries=*)  MAX_RETRIES="${1#*=}" ;;
            --control-over-sctp|-control-over-sctp) CONTROL_OVER_SCTP=1 ;;
            --stream|-stream)
                # Additional stream within the current SCTP association
                _spec="$2"
                ALL_FLOW_PROTOS="${ALL_FLOW_PROTOS};sctp"
                ALL_FLOW_SPECS="${ALL_FLOW_SPECS};${_spec}"
                EXPECTED_FLOWS=$((EXPECTED_FLOWS + 1))
                case "${_spec}" in
                    *exp*|*pareto*|*uniform*|*truncnormal*|*normal*)
                        MAX_RETRIES=5
                        ;;
                esac
                STREAM_ARGS="${STREAM_ARGS} ${_spec}"
                shift
                ;;
            -tcp|-udp|-sctp)
                # Additional separate transport connection / association
                _proto="${1#-}"
                _spec="$2"
                ALL_FLOW_PROTOS="${ALL_FLOW_PROTOS};${_proto}"
                ALL_FLOW_SPECS="${ALL_FLOW_SPECS};${_spec}"
                EXPECTED_FLOWS=$((EXPECTED_FLOWS + 1))
                case "${_spec}" in
                    *exp*|*pareto*|*uniform*|*truncnormal*|*normal*)
                        MAX_RETRIES=5
                        ;;
                esac
                EXTRA_CONN_ARGS="${EXTRA_CONN_ARGS} $1 $2"
                shift
                ;;
            *) GENERAL_EXTRA_ARGS="${GENERAL_EXTRA_ARGS} $1" ;;
        esac
        shift
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

compute_on_time() {
    _dur_str="$1"
    _rt="$2"
    if [ -z "${_dur_str}" ]; then
        echo "${_rt}"
        return 0
    fi
    perl -e '
        my ($dur_str, $rt) = @ARGV;
        if (!$dur_str) { print $rt; exit 0; }
        my @durs = split(/,/, $dur_str);
        my $on_time = 0.0;
        my $state = 0;
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
        print $on_time;
    ' "${_dur_str}" "${_rt}"
}

# Deduce global TX and RX expectations aggregated across ALL flows
GLOBAL_ACTIVE_EXPECT_TX=0
GLOBAL_ACTIVE_EXPECT_RX=0
GLOBAL_ACTIVE_FRAG_TX=0
GLOBAL_ACTIVE_FRAG_RX=0

OLD_IFS="$IFS"
IFS=";"
for _fspec in ${ALL_FLOW_SPECS}; do
    IFS=":"
    set -- ${_fspec}
    _o_rate="${1:-0}"
    _o_size="${2:-0}"
    _i_rate="${3:-0}"
    _i_size="${4:-0}"

    _f_maxmsgsize=0
    _f_onoff=""
    shift 4 2>/dev/null || true
    while [ "$#" -gt 0 ]; do
        case "$1" in
            maxmsgsize=*) _f_maxmsgsize="${1#maxmsgsize=}" ;;
            onoff=*|events=*) _f_onoff="${1#*=}" ;;
        esac
        shift
    done

    # A flow is active if its frame size > 0 (supports both constant/stochastic rate and saturated const0 bulk)
    case "${_o_size}" in
        0|const0) ;;
        *)
            GLOBAL_ACTIVE_EXPECT_TX=1
            if [ "${_f_maxmsgsize}" -gt 0 ] && perl -e "exit(!($(parse_mean "${_o_size}") > ${_f_maxmsgsize}))"; then
                GLOBAL_ACTIVE_FRAG_TX=1
            fi
            ;;
    esac

    case "${_i_size}" in
        0|const0) ;;
        *)
            GLOBAL_ACTIVE_EXPECT_RX=1
            if [ "${_f_maxmsgsize}" -gt 0 ] && perl -e "exit(!($(parse_mean "${_i_size}") > ${_f_maxmsgsize}))"; then
                GLOBAL_ACTIVE_FRAG_RX=1
            fi
            ;;
    esac
    IFS=";"
done
IFS="$OLD_IFS"

GLOBAL_PASSIVE_EXPECT_TX="${GLOBAL_ACTIVE_EXPECT_RX}"
GLOBAL_PASSIVE_EXPECT_RX="${GLOBAL_ACTIVE_EXPECT_TX}"
GLOBAL_PASSIVE_FRAG_TX="${GLOBAL_ACTIVE_FRAG_RX}"
GLOBAL_PASSIVE_FRAG_RX="${GLOBAL_ACTIVE_FRAG_TX}"

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
    _all_specs="$2"
    _all_protos="$3"
    _runtime="${4:-$RUNTIME}"
    _file="${TMP_DIR}/test-${_role}.sca${EXT}"

    if [ ! -s "${_file}" ]; then
        echo "FAIL: Scalar output '${_file}' is missing or empty." >&2
        return 1
    fi

    _scalar_eval_rc=0
    decompress_cat "${_file}" | perl -e '
        use strict;
        use warnings;

        my ($filename, $role, $all_specs_str, $all_protos_str, $runtime) = @ARGV;

        my @flow_specs = split(/;/, $all_specs_str);
        my @flow_protos = split(/;/, $all_protos_str);
        my $expected_flows = scalar(@flow_specs);

        my (%stats, %rates, %flow_objects);
        my ($fatal_errors, $stat_errors) = (0, 0);

        sub parse_mean {
            my ($dist) = @_;
            return 0 if !$dist || $dist eq "0";
            if ($dist =~ /^const([0-9.]+)/) { return $1 + 0; }
            elsif ($dist =~ /^uniform([0-9.]+),([0-9.]+)/) { return ($1 + $2) / 2.0; }
            elsif ($dist =~ /^exp([0-9.]+)/) { return $1 + 0; }
            elsif ($dist =~ /^pareto([0-9.]+),([0-9.]+)/) {
                my ($k, $a) = ($1 + 0, $2 + 0);
                return $a > 1.0 ? ($a * $k) / ($a - 1.0) : $k;
            } elsif ($dist =~ /^(?:trunc)?normal([0-9.]+),([0-9.]+)/) { return $1 + 0; }
            return 0;
        }

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
                return ($is_rate ? (0, 0) : ($1 + 0, 0.40));
            } elsif ($dist =~ /^pareto([0-9.]+),([0-9.]+)/) {
                my ($k, $a) = ($1 + 0, $2 + 0);
                return ($is_rate ? (($a > 0 ? (($a + 1.0)/$a)*$k : $k), 0.30)
                                 : (($a > 1.0 ? ($a*$k)/($a - 1.0) : $k), 0.35));
            } elsif ($dist =~ /^(?:trunc)?normal([0-9.]+),([0-9.]+)/) {
                return ($1 + 0, 0.25);
            }
            return (0, 0);
        }

        sub compute_on_time {
            my ($dur_str, $rt) = @_;
            return $rt if !$dur_str;
            my @durs = split(/,/, $dur_str);
            my $on_time = 0.0;
            my $state = 0;
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

        while (<STDIN>) {
            if (/^scalar\s+"([^"]+)"\s+"(Transmitted|Received)\s+(Bytes|Packets|Frames)"\s+([0-9.]+)/) {
                my ($obj, $dir, $metric, $val) = ($1, $2, $3, $4 + 0);
                $stats{$obj}{$dir}{$metric} = $val;
                $flow_objects{$obj} = 1 if $obj =~ /\.flow\[\d+\]$/;
            } elsif (/^scalar\s+"([^"]+)"\s+"(Transmitted|Received)\s+Frame Rate"\s+([0-9.]+)/) {
                my ($obj, $dir, $val) = ($1, $2, $3 + 0);
                $rates{$obj}{$dir} = $val;
            }
        }

        my $num_flows_found = scalar(keys %flow_objects);
        if ($expected_flows > 1 && $num_flows_found != $expected_flows) {
            warn "FAIL in $filename: Expected $expected_flows flows, found $num_flows_found\n";
            $fatal_errors++;
        }

        # Multi-connection aggregation verification: .total must equal sum of flows
        if ($num_flows_found > 1) {
            foreach my $dir ("Transmitted", "Received") {
                foreach my $metric ("Bytes", "Packets", "Frames") {
                    my $sum = 0;
                    foreach my $flow_obj (keys %flow_objects) {
                        $sum += $stats{$flow_obj}{$dir}{$metric} // 0;
                    }
                    my ($total_key) = grep { /\.total$/ } keys %stats;
                    if ($total_key) {
                        my $total_val = $stats{$total_key}{$dir}{$metric} // 0;
                        if ($sum != $total_val) {
                            warn "FAIL in $filename: Multi-connection aggregation mismatch for $dir $metric (Sum of flows=$sum, Total=$total_val)\n";
                            $fatal_errors++;
                        }
                    }
                }
            }
        }

        # Validate each flow object individually against its respective spec
        for (my $idx = 0; $idx < $expected_flows; $idx++) {
            my ($flow_obj) = grep { /\.flow\[$idx\]$/ } keys %stats;
            if (!$flow_obj) {
                warn "FAIL in $filename: Missing metrics for flow[$idx]\n";
                $fatal_errors++;
                next;
            }

            my $spec = $flow_specs[$idx];
            my $proto = $flow_protos[$idx] // "tcp";
            my ($out_r, $out_s, $in_r, $in_s, @opts) = split(/:/, $spec);
            $out_r //= "0"; $out_s //= "0"; $in_r //= "0"; $in_s //= "0";

            my ($maxmsgsize, $onoff_str) = (0, "");
            foreach my $opt (@opts) {
                if ($opt =~ /^maxmsgsize=(\d+)/) { $maxmsgsize = $1 + 0; }
                elsif ($opt =~ /^(?:onoff|events)=(.+)/) { $onoff_str = $1; }
            }

            my $on_time = compute_on_time($onoff_str, $runtime + 0);
            my $out_active = (!($out_s =~ /^(?:0|const0)$/)) ? 1 : 0;
            my $in_active  = (!($in_s  =~ /^(?:0|const0)$/)) ? 1 : 0;

            my ($expect_tx, $expect_rx);
            my ($flow_tx_r, $flow_tx_s, $flow_rx_r, $flow_rx_s);
            if ($role eq "active") {
                $expect_tx = $out_active;
                $expect_rx = $in_active;
                ($flow_tx_r, $flow_tx_s) = ($out_r, $out_s);
                ($flow_rx_r, $flow_rx_s) = ($in_r, $in_s);
            } else {
                $expect_tx = $in_active;
                $expect_rx = $out_active;
                ($flow_tx_r, $flow_tx_s) = ($in_r, $in_s);
                ($flow_rx_r, $flow_rx_s) = ($out_r, $out_s);
            }

            my $frag_tx = ($maxmsgsize > 0 && parse_mean($flow_tx_s) > $maxmsgsize) ? 1 : 0;
            my $frag_rx = ($maxmsgsize > 0 && parse_mean($flow_rx_s) > $maxmsgsize) ? 1 : 0;

            foreach my $dir ("Transmitted", "Received") {
                my $expected = ($dir eq "Transmitted") ? $expect_tx : $expect_rx;
                my $frag     = ($dir eq "Transmitted") ? $frag_tx   : $frag_rx;
                my $dist_r   = ($dir eq "Transmitted") ? $flow_tx_r : $flow_rx_r;
                my $dist_s   = ($dir eq "Transmitted") ? $flow_tx_s : $flow_rx_s;

                my $b = $stats{$flow_obj}{$dir}{Bytes} // 0;
                my $p = $stats{$flow_obj}{$dir}{Packets} // 0;
                my $f = $stats{$flow_obj}{$dir}{Frames} // 0;
                my $r = $rates{$flow_obj}{$dir} // 0;

                my $is_rand_r = ($dist_r =~ /^(?:exp|pareto|uniform|truncnormal|normal)/);
                my $is_rand_s = ($dist_s =~ /^(?:exp|pareto|uniform|truncnormal|normal)/);
                my $flow_has_random = ($is_rand_r || $is_rand_s);

                if ($expected) {
                    if ($b <= 0 || $p <= 0 || $f <= 0) {
                        warn "FAIL in $filename ($flow_obj): Expected $dir B/P/F > 0 (got B=$b, P=$p, F=$f)\n";
                        $fatal_errors++;
                    }
                    if ($b < $p) {
                        warn "FAIL in $filename ($flow_obj): Invariant violation: $dir Bytes ($b) < Packets ($p)\n";
                        $fatal_errors++;
                    }
                    if ($b < $f) {
                        warn "FAIL in $filename ($flow_obj): Invariant violation: $dir Bytes ($b) < Frames ($f)\n";
                        $fatal_errors++;
                    }
                    if ($p < $f) {
                        warn "FAIL in $filename ($flow_obj): Invariant violation: $dir Packets ($p) < Frames ($f)\n";
                        $fatal_errors++;
                    }

                    if ($frag) {
                        if ($p <= $f) {
                            warn "FAIL in $filename ($flow_obj): Expected fragmented $dir Packets ($p) > Frames ($f)\n";
                            $fatal_errors++;
                        }
                    } else {
                        if ($dist_s =~ /^const/ && !($dist_r =~ /^(?:0|const0)$/)) {
                            if ($p != $f) {
                                warn "FAIL in $filename ($flow_obj): Expected unfragmented constant $dir Packets ($p) == Frames ($f)\n";
                                $fatal_errors++;
                            }
                        }
                    }

                    my ($target_rate, $tol_rate) = parse_dist($dist_r, 1);
                    my ($target_size, $tol_size) = parse_dist($dist_s, 0);

                    my $eff_tol_size = ($f > 0 && $f <= 30) ? ($tol_size * 1.5) : $tol_size;
                    my $eff_tol_rate = ($runtime > 0 && ($runtime <= 3 || $f <= 30)) ? ($tol_rate * 1.5) : $tol_rate;

                    # On-off schedule validation
                    if ($onoff_str && $target_rate > 0) {
                        my $cont_frames = $target_rate * $runtime;
                        my $expected_onoff_frames = $target_rate * $on_time;

                        if ($on_time < $runtime) {
                            my $upper_bound = $cont_frames * 0.90;
                            if ($f > $upper_bound) {
                                warn "FAIL in $filename ($flow_obj): On-off flow transmitted $f frames, expected < $upper_bound\n";
                                $fatal_errors++;
                            }
                        }

                        if ($expected_onoff_frames > 0) {
                            my $diff = abs($f - $expected_onoff_frames);
                            if ($diff > ($expected_onoff_frames * ($eff_tol_rate + 0.15))) {
                                warn "FAIL in $filename ($flow_obj): Measured frames ($f) deviated from expected ($expected_onoff_frames)\n";
                                if ($flow_has_random) { $stat_errors++; }
                                else { $fatal_errors++; }
                            }
                        }
                    } else {
                        # Standard rate validation (bypassed for saturated bulk flows where target_rate == 0)
                        if ($target_rate > 0) {
                            my $diff = abs($r - $target_rate);
                            if ($diff > ($target_rate * $eff_tol_rate)) {
                                my $pct = sprintf("%.1f%%", ($diff / $target_rate) * 100);
                                warn "FAIL in $filename ($flow_obj): $dir Frame Rate ($r) deviated from expected ($target_rate) by $pct\n";
                                if ($flow_has_random) { $stat_errors++; }
                                else { $fatal_errors++; }
                            }
                        }
                    }

                    # Average frame size check during active transmissions
                    if ($target_size > 0 && $f > 0) {
                        my $avg_size = $b / $f;
                        my $diff = abs($avg_size - $target_size);
                        if ($diff > ($target_size * $eff_tol_size)) {
                            my $pct = sprintf("%.1f%%", ($diff / $target_size) * 100);
                            warn "FAIL in $filename ($flow_obj): $dir Avg Frame Size ($avg_size) deviated from expected ($target_size) by $pct\n";
                            if ($is_rand_s) { $stat_errors++; }
                            else { $fatal_errors++; }
                        }
                    }
                } else {
                    if ($b != 0 || $p != 0 || $f != 0) {
                        warn "FAIL in $filename ($flow_obj): Expected zero $dir metrics (got B=$b, P=$p, F=$f)\n";
                        $fatal_errors++;
                    }
                }
            }
        }

        # Check total object invariants
        my ($total_obj) = grep { /\.total$/ } keys %stats;
        if ($total_obj) {
            foreach my $dir ("Transmitted", "Received") {
                my $b = $stats{$total_obj}{$dir}{Bytes} // 0;
                my $p = $stats{$total_obj}{$dir}{Packets} // 0;
                my $f = $stats{$total_obj}{$dir}{Frames} // 0;
                if ($b < $p) {
                    warn "FAIL in $filename ($total_obj): Total Bytes ($b) < Packets ($p)\n";
                    $fatal_errors++;
                }
                if ($p < $f) {
                    warn "FAIL in $filename ($total_obj): Total Packets ($p) < Frames ($f)\n";
                    $fatal_errors++;
                }
            }
        }

        if ($fatal_errors > 0) { exit 1; }
        elsif ($stat_errors > 0) { exit 2; }
        else { exit 0; }
    ' "${_file}" "${_role}" "${_all_specs}" "${_all_protos}" "${_runtime}" || _scalar_eval_rc=$?

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

# ====== Check Vector Results ==============================================
# All vector checks test structural invariants (monotonicity, framing, intervals).
# Any failure in vector verification is fatal (return 1).
check_vector() {
    _role="$1"
    _expect_tx="$2"
    _expect_rx="$3"
    _frag_tx="$4"
    _frag_rx="$5"
    _all_specs="$6"

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

            my ($filename, $expect_tx, $expect_rx, $frag_tx, $frag_rx, $all_specs_str, $role) = @ARGV;
            my $line_num = 0;
            my $is_flow_vector = 0;
            my $data_rows = 0;
            my $errors = 0;

            my ($last_seq, $last_b, $last_p, $last_f);
            my (%max_b, %max_p, %max_f, %last_meas_b, %last_meas_p, %last_meas_f);

            my @flow_specs = split(/;/, $all_specs_str);
            my %flow_onoff;
            for (my $i = 0; $i < @flow_specs; $i++) {
                if ($flow_specs[$i] =~ /(?:onoff|events)=([^:]+)/) {
                    $flow_onoff{$i} = $1;
                }
            }

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
                        # Transition from ON -> OFF at $cum_t; allow up to 250ms scheduler jitter under load
                        if ($t_start >= $cum_t && ($t_start - $cum_t) <= 0.250) {
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

                    if ($bytes < $pkts) {
                        warn "FAIL in $filename (line $line_num): AbsBytes ($bytes) < AbsPackets ($pkts)\n";
                        $errors++;
                    }
                    if ($pkts < $frms) {
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
                    my $flow_id  = $fields[4] + 0;
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
                    if ($pkts < $frms) {
                        warn "FAIL in $filename (line $line_num): $action AbsPackets ($pkts) < AbsFrames ($frms)\n";
                        $errors++;
                    }

                    if (!defined $max_b{$action} || $bytes > $max_b{$action}) { $max_b{$action} = $bytes; }
                    if (!defined $max_p{$action} || $pkts  > $max_p{$action}) { $max_p{$action} = $pkts; }
                    if (!defined $max_f{$action} || $frms  > $max_f{$action}) { $max_f{$action} = $frms; }

                    # On-off silence check during OFF intervals (checked per-flow, skip Total aggregate)
                    if ($flow_id >= 0 && defined $flow_onoff{$flow_id}) {
                        my $onoff_spec = $flow_onoff{$flow_id};
                        if ($action eq "Sent" && $role eq "active" && $interval > 0) {
                            my $t_start = ($rel_time > $interval) ? ($rel_time - $interval) : 0.0;
                            my $t_end   = $rel_time;
                            my $act     = interval_active_time($onoff_spec, $t_start, $t_end);

                            if ($act == 0.0) {
                                my $is_boundary = is_near_active_boundary($onoff_spec, $t_start);
                                if ($is_boundary) {
                                    # Allow at most 1 trailing boundary frame from the active interval
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

            if (!$is_flow_vector) {
                foreach my $dir ("Sent", "Received") {
                    my $expected  = ($dir eq "Sent") ? $expect_tx : $expect_rx;
                    my $expect_fg = ($dir eq "Sent") ? $frag_tx   : $frag_rx;

                    my $b = $max_b{$dir} // 0;
                    my $p = $max_p{$dir} // 0;
                    my $f = $max_f{$dir} // 0;

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
        ' "${_file}" "${_expect_tx}" "${_expect_rx}" "${_frag_tx}" "${_frag_rx}" "${_all_specs}" "${_role}"; then
            echo "FAIL: Vector verification failed for '${_file}'." >&2
            echo "--- Content of ${_file} ---" >&2
            decompress_cat "${_file}" >&2
            return 1
        fi
    done
}

# ====== Listener Readiness Probe (TCP & SCTP via Perl) =====================
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
                echo "--- Server Log ---" >&2
                cat "${TMP_DIR}/server.log" >&2
            fi
            return 1
        fi

        if [ "${_proto}" = "sctp" ]; then
            if perl -e '
                use strict;
                use warnings;
                use Socket qw(AF_INET AF_INET6 SOCK_STREAM sockaddr_in sockaddr_in6 inet_aton inet_pton);

                my ($host, $port) = @ARGV;
                my $is_v6 = ($host =~ /:/);
                my $family = $is_v6 ? AF_INET6 : AF_INET;
                my $proto = 132; # IPPROTO_SCTP

                foreach my $sock_type (1, 5) {
                    my $sock;
                    if (socket($sock, $family, $sock_type, $proto)) {
                        my $addr = $is_v6 ? sockaddr_in6($port, inet_pton(AF_INET6, $host))
                                          : sockaddr_in($port, inet_aton($host));
                        if (connect($sock, $addr)) {
                            close($sock);
                            exit 0;
                        }
                        close($sock);
                    }
                }
                exit 1;
            ' "$_host" "$_port" 2>/dev/null; then
                return 0
            fi
        else
            if perl -MIO::Socket::IP -e \
               'exit(!IO::Socket::IP->new(PeerHost => $ARGV[0], PeerPort => $ARGV[1], Proto => "tcp", GetAddrInfoFlags => 0, Timeout => 1))' \
               "$_host" "$_port" 2>/dev/null; then
                return 0
            fi
        fi

        sleep 1
        _elapsed=$((_elapsed + 1))
    done

    echo "FAIL: Passive server failed to listen on ${_proto}://${_host}:${_port} within ${_timeout}s." >&2
    if [ -s "${TMP_DIR}/server.log" ]; then
        echo "--- Server Log ---" >&2
        cat "${TMP_DIR}/server.log" >&2
    fi
    return 1
}

# ====== Single Attempt Execution ==========================================
run_attempt() {
    _attempt="$1"

    TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_test')
    SERVER_PID=""

    stop_server() {
        if [ -n "${SERVER_PID}" ]; then
            _server_rc=0
            if kill -0 "${SERVER_PID}" 2>/dev/null; then
                kill -INT "${SERVER_PID}" 2>/dev/null || true
                wait "${SERVER_PID}" 2>/dev/null || _server_rc=$?
            else
                wait "${SERVER_PID}" 2>/dev/null || _server_rc=$?
            fi
            SERVER_PID=""

            # Fail if passive instance crashed (e.g., exit status 134 on SIGABRT)
            if [ "${_server_rc}" -ne 0 ]; then
                echo "FAIL: Passive server instance crashed with exit code ${_server_rc}." >&2
                if [ -s "${TMP_DIR}/server.log" ]; then
                    echo "--- Server Log ---" >&2
                    cat "${TMP_DIR}/server.log" >&2
                fi
                return 1
            fi

            # Fail if assertion errors or core dump notices appear in server.log
            if [ -s "${TMP_DIR}/server.log" ] && grep -qE "failed in|FATAL ERROR|Aborted|Avbrutt" "${TMP_DIR}/server.log"; then
                echo "FAIL: Assertion failure or crash detected in server log." >&2
                echo "--- Server Log ---" >&2
                cat "${TMP_DIR}/server.log" >&2
                return 1
            fi
        fi
        return 0
    }

    # 1. Start passive node
    if [ "${CONTROL_OVER_SCTP}" -eq 1 ]; then
        SERVER_CONTROL_ARGS="-control-over-tcp -control-over-sctp"
        CLIENT_CONTROL_ARG="-control-over-sctp"
        CTRL_PROBE_PROTO="sctp"
    else
        SERVER_CONTROL_ARGS="-control-over-tcp -no-control-over-sctp"
        CLIENT_CONTROL_ARG="-control-over-tcp"
        CTRL_PROBE_PROTO="tcp"
    fi

    "${NETPERFMETER}" "${PORT}" ${SERVER_CONTROL_ARGS} >"${TMP_DIR}/server.log" 2>&1 &
    SERVER_PID=$!

    # 2. Wait until control channel (PORT + 1) is ready
    if ! wait_for_listener "${ADDRESS}" "${CONTROL_PORT}" "${CTRL_PROBE_PROTO}" 5; then
        stop_server
        return 1
    fi

    # For connection-oriented data flows, also wait for data port readiness
    if [ "${PROTOCOL}" = "tcp" ] || [ "${PROTOCOL}" = "sctp" ]; then
        if ! wait_for_listener "${ADDRESS}" "${PORT}" "${PROTOCOL}" 5; then
            stop_server
            return 1
        fi
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

    # General options precede flow specifications, allowing extra stream specs
    # passed in ${STREAM_ARGS} to follow "-${PROTOCOL}" "${SPEC}"
    if ! "${NETPERFMETER}" "${TARGET}:${PORT}" \
        ${CLIENT_CONTROL_ARG} \
        -runtime="${RUNTIME}" \
        --config="${TMP_DIR}/test.config" \
        ${SCALAR_ARG} \
        ${VECTOR_ARG} \
        ${GENERAL_EXTRA_ARGS} \
        "-${PROTOCOL}" "${SPEC}" \
        ${STREAM_ARGS} \
        ${EXTRA_CONN_ARGS} \
        >"${TMP_DIR}/client.log" 2>&1; then
        echo "FAIL: Active instance exited with an error." >&2
        cat "${TMP_DIR}/client.log" >&2
        stop_server || true
        return 1
    fi

    # Explicitly check server shutdown status
    if ! stop_server; then
        return 1
    fi

    # 4. Verify Configuration output (--config)
    if [ ! -s "${TMP_DIR}/test.config" ]; then
        echo "FAIL: Configuration file (--config) missing or empty." >&2
        return 1
    fi

    # 5. Verify Scalar outputs
    if [ "${ENABLE_SCALAR}" -eq 1 ]; then
        _scalar_rc=0
        check_scalar "active"  "${ALL_FLOW_SPECS}" "${ALL_FLOW_PROTOS}" "${RUNTIME}" || _scalar_rc=$?
        if [ "${_scalar_rc}" -ne 0 ]; then
            return "${_scalar_rc}"
        fi

        check_scalar "passive" "${ALL_FLOW_SPECS}" "${ALL_FLOW_PROTOS}" "${RUNTIME}" || _scalar_rc=$?
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
        if ! check_vector "active"  "${GLOBAL_ACTIVE_EXPECT_TX}"  "${GLOBAL_ACTIVE_EXPECT_RX}"  "${GLOBAL_ACTIVE_FRAG_TX}"  "${GLOBAL_ACTIVE_FRAG_RX}" "${ALL_FLOW_SPECS}"; then
            return 1
        fi
        if ! check_vector "passive" "${GLOBAL_PASSIVE_EXPECT_TX}" "${GLOBAL_PASSIVE_EXPECT_RX}" "${GLOBAL_PASSIVE_FRAG_TX}" "${GLOBAL_PASSIVE_FRAG_RX}" "${ALL_FLOW_SPECS}"; then
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
        echo "WARNING: Attempt ${ATTEMPT}/${MAX_RETRIES} for ${PROTOCOL} flow (${SPEC}) failed due to statistical outlier in random distribution." >&2
        if [ "${ATTEMPT}" -lt "${MAX_RETRIES}" ]; then
            rm -rf "${TMP_DIR}"
            sleep 1
        else
            echo "All ${MAX_RETRIES} attempts failed; artifacts preserved in ${TMP_DIR}" >&2
        fi
    else
        echo "FAIL: Attempt ${ATTEMPT} for ${PROTOCOL} flow (${SPEC}) failed with non-retryable error (exit code ${_attempt_rc}). Aborting." >&2
        echo "Artifacts preserved in ${TMP_DIR}" >&2
        exit 1
    fi
    ATTEMPT=$((ATTEMPT + 1))
done

if [ "${SUCCESS}" -eq 1 ]; then
    echo "PASS: ${PROTOCOL} flow test (Port: ${PORT}, Host: ${ADDRESS}, Spec: ${SPEC}, Flows: ${EXPECTED_FLOWS}) passed after ${ATTEMPT} attempt(s)."
    exit 0
else
    exit 1
fi
