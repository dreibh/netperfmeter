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
#   test_runtimeestimator.sh <runtimeestimator_bin>

set -eu

RUNTIMEESTIMATOR="${1:-runtimeestimator}"
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_runtimeestimator_test')

cleanup() {
    trap - EXIT INT TERM
    if [ -n "${TMP_DIR}" ] && [ -d "${TMP_DIR}" ]; then
        rm -rf "${TMP_DIR}"
    fi
}

cleanup_signal() {
    _sig="$1"
    cleanup
    if [ "${_sig}" = "INT" ]; then
        exit 130
    else
        exit 143
    fi
}

trap cleanup EXIT
trap 'cleanup_signal INT' INT
trap 'cleanup_signal TERM' TERM

START_FILE="${TMP_DIR}/runtime.dat"

# Helper: run command expecting success (exit 0) and empty stderr
run_expect_ok() {
    _out_file="$1"
    _err_file="$2"
    shift 2

    if ! "${RUNTIMEESTIMATOR}" "$@" >"${_out_file}" 2>"${_err_file}"; then
        echo "FAIL: '${RUNTIMEESTIMATOR} $*' exited with non-zero status." >&2
        cat "${_err_file}" >&2
        exit 1
    fi

    if [ -s "${_err_file}" ]; then
        echo "FAIL: Unexpected stderr output from '${RUNTIMEESTIMATOR} $*':" >&2
        cat "${_err_file}" >&2
        exit 1
    fi
}

# Helper: run command expecting failure (non-zero exit), empty stdout, and expected stderr regex
run_expect_fail() {
    _expected_regex="$1"
    shift

    _rc=0
    "${RUNTIMEESTIMATOR}" "$@" >"${TMP_DIR}/fail_out.txt" 2>"${TMP_DIR}/fail_err.txt" || _rc=$?

    if [ "${_rc}" -eq 0 ]; then
        echo "FAIL: '${RUNTIMEESTIMATOR} $*' unexpectedly succeeded (exit code 0)." >&2
        exit 1
    fi

    if [ -s "${TMP_DIR}/fail_out.txt" ]; then
        echo "FAIL: '${RUNTIMEESTIMATOR} $*' wrote unexpected output to stdout:" >&2
        cat "${TMP_DIR}/fail_out.txt" >&2
        exit 1
    fi

    if ! grep -Eq "${_expected_regex}" "${TMP_DIR}/fail_err.txt"; then
        echo "FAIL: '${RUNTIMEESTIMATOR} $*' stderr did not match '${_expected_regex}':" >&2
        cat "${TMP_DIR}/fail_err.txt" >&2
        exit 1
    fi
}

# ====== 1. Run 0: Initialise start file ===================================
run_expect_ok "${TMP_DIR}/run0.out" "${TMP_DIR}/run0.err" "${START_FILE}" 4 0

if [ ! -f "${START_FILE}" ]; then
    echo "FAIL: Start file '${START_FILE}' was not created on run 0." >&2
    exit 1
fi

perl -e '
    use strict;
    use warnings;
    use Time::HiRes qw(gettimeofday);

    my ($start_file, $out_file) = @ARGV;

    # Verify 8-byte big-endian microsecond timestamp in start file
    my $size = -s $start_file;
    if (!defined($size) || $size != 8) {
        warn "FAIL: Expected start file size 8 bytes, got " . ($size // "undef") . "\n";
        exit 1;
    }

    open(my $bfh, "<:raw", $start_file) or die "Cannot open $start_file: $!\n";
    my $bytes_read = read($bfh, my $buf, 8);
    close($bfh);
    if (!defined($bytes_read) || $bytes_read != 8) {
        warn "FAIL: Short read from $start_file\n";
        exit 1;
    }

    my ($hi, $lo) = unpack("N2", $buf);
    my $stored_us = $hi * 4294967296 + $lo;

    my ($sec, $usec) = gettimeofday();
    my $now_us = $sec * 1_000_000 + $usec;
    if (abs($now_us - $stored_us) > 60_000_000) {
        warn "FAIL: Stored big-endian timestamp ($stored_us) differs from system clock ($now_us) by > 60 s\n";
        exit 1;
    }

    # Verify Run 0 stdout formatting (strip ANSI color sequences)
    open(my $ofh, "<", $out_file) or die "Cannot open $out_file: $!\n";
    my @lines = <$ofh>;
    close($ofh);
    if (@lines != 1) {
        warn "FAIL: Expected 1 stdout line for run 0, got " . scalar(@lines) . "\n";
        exit 1;
    }
    chomp(my $line = $lines[0]);
    $line =~ s/\e\[[0-9;]*m//g;

    if ($line ne "##### ESTIMATION: - [0 of 4 - 0.00%]") {
        warn "FAIL: Unexpected run 0 output: \x27$line\x27\n";
        exit 1;
    }

    exit 0;
' "${START_FILE}" "${TMP_DIR}/run0.out"

cp "${START_FILE}" "${TMP_DIR}/runtime_run0.bak"

# ====== 2. Run 1 of 4 after 1 s sleep: Live mid-batch estimation ==========
sleep 1
run_expect_ok "${TMP_DIR}/run1.out" "${TMP_DIR}/run1.err" "${START_FILE}" 4 1

# Verify start file was not altered by Run 1
if ! cmp -s "${START_FILE}" "${TMP_DIR}/runtime_run0.bak"; then
    echo "FAIL: Start file was modified during non-zero run." >&2
    exit 1
fi

perl -e '
    use strict;
    use warnings;

    my ($out_file) = @ARGV;
    open(my $fh, "<", $out_file) or die "Cannot open $out_file: $!\n";
    my @lines = <$fh>;
    close($fh);
    if (@lines != 1) {
        warn "FAIL: Expected 1 stdout line for run 1, got " . scalar(@lines) . "\n";
        exit 1;
    }
    chomp(my $line = $lines[0]);
    $line =~ s/\e\[[0-9;]*m//g;

    if ($line !~ /\A##### ESTIMATION: \d{2}-[A-Za-z]{3}-\d{4} \d{2}:\d{2}:\d{2}\.\d{4} \(in (\d+\.\d{2}) minutes\)  \[1 of 4 - 25\.00%\]\z/) {
        warn "FAIL: Unexpected run 1 output format: \x27$line\x27\n";
        exit 1;
    }

    my $rem_min = $1 + 0;
    if ($rem_min <= 0.0 || $rem_min > 2.0) {
        warn "FAIL: Implausible remaining minutes after 1 s run (3 remaining): $rem_min\n";
        exit 1;
    }

    exit 0;
' "${TMP_DIR}/run1.out"

# ====== 3. Run 4 of 4: Batch completion (0.00 minutes, 100.00%) ===========
run_expect_ok "${TMP_DIR}/run4.out" "${TMP_DIR}/run4.err" "${START_FILE}" 4 4

perl -e '
    use strict;
    use warnings;

    my ($out_file) = @ARGV;
    open(my $fh, "<", $out_file) or die "Cannot open $out_file: $!\n";
    my @lines = <$fh>;
    close($fh);
    if (@lines != 1) {
        warn "FAIL: Expected 1 stdout line for run 4, got " . scalar(@lines) . "\n";
        exit 1;
    }
    chomp(my $line = $lines[0]);
    $line =~ s/\e\[[0-9;]*m//g;

    if ($line !~ /\A##### ESTIMATION: \d{2}-[A-Za-z]{3}-\d{4} \d{2}:\d{2}:\d{2}\.\d{4} \(in 0\.00 minutes\)  \[4 of 4 - 100\.00%\]\z/) {
        warn "FAIL: Unexpected completion output: \x27$line\x27\n";
        exit 1;
    }

    exit 0;
' "${TMP_DIR}/run4.out"

# ====== 4. Deterministic multi-minute scaling with synthetic start file ===
SYNTH_FILE="${TMP_DIR}/synthetic.dat"
perl -e '
    use strict;
    use warnings;
    use Time::HiRes qw(gettimeofday);

    my ($path) = @ARGV;
    my ($sec, $usec) = gettimeofday();
    # Set start timestamp to 60 seconds in the past
    my $past_us = ($sec - 60) * 1_000_000 + $usec;
    my $hi = int($past_us / 4294967296);
    my $lo = $past_us - ($hi * 4294967296);

    open(my $fh, ">:raw", $path) or die "Cannot create $path: $!\n";
    print $fh pack("N2", $hi, $lo);
    close($fh);
' "${SYNTH_FILE}"

# 1 of 4 completed after 60 s -> 3 runs left * 60 s = 180 s = 3.00 minutes
run_expect_ok "${TMP_DIR}/synth1.out" "${TMP_DIR}/synth1.err" "${SYNTH_FILE}" 4 1
# 3 of 4 completed after 60 s -> 1 run left * 20 s = 20 s = 0.33 minutes
run_expect_ok "${TMP_DIR}/synth3.out" "${TMP_DIR}/synth3.err" "${SYNTH_FILE}" 4 3

perl -e '
    use strict;
    use warnings;

    my ($f1, $f3) = @ARGV;

    sub check_minutes {
        my ($path, $cur, $tot, $pct_str, $expected_min, $tol_min) = @_;
        open(my $fh, "<", $path) or die "Cannot open $path: $!\n";
        chomp(my $line = <$fh>);
        close($fh);
        $line =~ s/\e\[[0-9;]*m//g;

        if ($line !~ /\A##### ESTIMATION: \d{2}-[A-Za-z]{3}-\d{4} \d{2}:\d{2}:\d{2}\.\d{4} \(in (\d+\.\d{2}) minutes\)  \[$cur of $tot - \Q$pct_str\E%\]\z/) {
            warn "FAIL: Unexpected output in $path: \x27$line\x27\n";
            exit 1;
        }
        my $meas = $1 + 0;
        if (abs($meas - $expected_min) > $tol_min) {
            warn "FAIL: Expected ~$expected_min min in $path, got $meas min\n";
            exit 1;
        }
    }

    check_minutes($f1, 1, 4, "25.00", 3.00, 0.25);
    check_minutes($f3, 3, 4, "75.00", 0.33, 0.10);
    exit 0;
' "${TMP_DIR}/synth1.out" "${TMP_DIR}/synth3.out"

# ====== 5. Single-run batch edge case (1 of 1) ============================
SINGLE_FILE="${TMP_DIR}/single.dat"
run_expect_ok "${TMP_DIR}/single0.out" "${TMP_DIR}/single0.err" "${SINGLE_FILE}" 1 0
run_expect_ok "${TMP_DIR}/single1.out" "${TMP_DIR}/single1.err" "${SINGLE_FILE}" 1 1

# ====== 6. Negative / error handling tests ================================
# Missing / wrong argument counts
run_expect_fail "Usage:"
run_expect_fail "Usage:" "${START_FILE}"
run_expect_fail "Usage:" "${START_FILE}" 10
run_expect_fail "Usage:" "${START_FILE}" 10 1 extra_arg
run_expect_fail "Usage:" --non-existent-option
run_expect_fail "Usage:" -Z

# Negative numbers without "--" are parsed by getopt_long_only() as unknown options
run_expect_fail "Usage:" "${START_FILE}" -5 0
run_expect_fail "Usage:" "${START_FILE}" 10 -1

# Invalid total_runs (must be >= 1; "--" terminates option parsing for "-5")
run_expect_fail "ERROR: Invalid total runs parameter" "${START_FILE}" 0 0
run_expect_fail "ERROR: Invalid total runs parameter" "${START_FILE}" -- -5 0
run_expect_fail "ERROR: Invalid total runs parameter" "${START_FILE}" abc 0
run_expect_fail "ERROR: Invalid total runs parameter" "${START_FILE}" 10abc 0

# Invalid current_run (must be >= 0; "--" terminates option parsing for "-1")
run_expect_fail "ERROR: Invalid current run parameter" "${START_FILE}" 10 -- -1
run_expect_fail "ERROR: Invalid current run parameter" "${START_FILE}" 10 xyz
run_expect_fail "ERROR: Invalid current run parameter" "${START_FILE}" 10 1xyz

# current_run > total_runs
run_expect_fail "ERROR: Current run must be less or equal to total runs" "${START_FILE}" 10 11

# Unwritable start file path on run 0
run_expect_fail "ERROR: Unable to create file" "${TMP_DIR}/no_such_subdir/runtime.dat" 10 0

# Non-existent start file on run > 0
run_expect_fail "ERROR: Unable to open file" "${TMP_DIR}/does_not_exist.dat" 10 1

# Truncated / empty start file (< 8 bytes) on run > 0
TRUNC_FILE="${TMP_DIR}/truncated.dat"
printf "1234" >"${TRUNC_FILE}"
run_expect_fail "ERROR: Unable to read timestamp from file" "${TRUNC_FILE}" 10 1

echo "PASS: runtimeestimator test passed."
exit 0
