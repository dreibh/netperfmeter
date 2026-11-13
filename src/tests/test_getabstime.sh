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
#   test_getabstime.sh <getabstime_bin>

set -eu

GETABSTIME="${1:-getabstime}"
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_getabstime_test')

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

# 1. First sample: verify exit status 0, empty stderr, and microsecond format
if ! "${GETABSTIME}" >"${TMP_DIR}/out1.txt" 2>"${TMP_DIR}/err1.txt"; then
    echo "FAIL: '${GETABSTIME}' exited with non-zero status on first call." >&2
    cat "${TMP_DIR}/err1.txt" >&2
    exit 1
fi

if [ -s "${TMP_DIR}/err1.txt" ]; then
    echo "FAIL: Unexpected stderr output from '${GETABSTIME}':" >&2
    cat "${TMP_DIR}/err1.txt" >&2
    exit 1
fi

T1=$(cat "${TMP_DIR}/out1.txt")

# 2. Second sample after 1 second: verify progression and delta bounds
sleep 1

if ! "${GETABSTIME}" >"${TMP_DIR}/out2.txt" 2>"${TMP_DIR}/err2.txt"; then
    echo "FAIL: '${GETABSTIME}' exited with non-zero status on second call." >&2
    cat "${TMP_DIR}/err2.txt" >&2
    exit 1
fi

if [ -s "${TMP_DIR}/err2.txt" ]; then
    echo "FAIL: Unexpected stderr output from '${GETABSTIME}':" >&2
    cat "${TMP_DIR}/err2.txt" >&2
    exit 1
fi

T2=$(cat "${TMP_DIR}/out2.txt")

# 3. Validate format (strictly digits), line count, wall-clock alignment, and monotonicity
perl -e '
    use strict;
    use warnings;
    use Time::HiRes qw(gettimeofday);

    my ($out1_file, $out2_file) = @ARGV;

    sub read_single_line {
        my ($path) = @_;
        open(my $fh, "<", $path) or die "Cannot open $path: $!\n";
        my @lines = <$fh>;
        close($fh);
        if (@lines != 1) {
            warn "FAIL: Expected exactly 1 output line in $path, got " . scalar(@lines) . "\n";
            exit 1;
        }
        chomp(my $val = $lines[0]);
        if ($val !~ /\A[0-9]+\z/) {
            warn "FAIL: Output in $path is not a valid unsigned integer: \x27$val\x27\n";
            exit 1;
        }
        return $val;
    }

    my $t1_str = read_single_line($out1_file);
    my $t2_str = read_single_line($out2_file);

    my ($sec, $usec) = gettimeofday();
    my $now_us = $sec * 1_000_000 + $usec;
    my $t1 = $t1_str + 0;
    my $t2 = $t2_str + 0;

    # Verify microsecond scale and alignment with system clock (+/- 60 s)
    if (abs($now_us - $t2) > 60_000_000) {
        warn "FAIL: Timestamp ($t2) differs from system clock ($now_us) by more than 60 s\n";
        exit 1;
    }

    # Verify monotonicity and ~1 s sleep delta (0.5 s .. 30 s to tolerate loaded CI runners)
    my $delta = $t2 - $t1;
    if ($delta <= 0) {
        warn "FAIL: Non-monotonic timestamps (T1=$t1, T2=$t2)\n";
        exit 1;
    }
    if ($delta < 500_000 || $delta > 30_000_000) {
        warn "FAIL: Unexpected delta after 1 s sleep: ${delta} us (T1=$t1, T2=$t2)\n";
        exit 1;
    }

    exit 0;
' "${TMP_DIR}/out1.txt" "${TMP_DIR}/out2.txt"

# 4. Negative CLI checks: invalid option and unexpected positional argument
for _bad_arg in "--non-existent-option" "-Z" "unexpected_arg"; do
    _rc=0
    "${GETABSTIME}" "${_bad_arg}" >"${TMP_DIR}/bad_out.txt" 2>"${TMP_DIR}/bad_err.txt" || _rc=$?
    if [ "${_rc}" -eq 0 ]; then
        echo "FAIL: '${GETABSTIME} ${_bad_arg}' unexpectedly succeeded (exit code 0)." >&2
        exit 1
    fi
    if [ -s "${TMP_DIR}/bad_out.txt" ]; then
        echo "FAIL: '${GETABSTIME} ${_bad_arg}' wrote unexpected output to stdout." >&2
        cat "${TMP_DIR}/bad_out.txt" >&2
        exit 1
    fi
    if ! grep -q "Usage:" "${TMP_DIR}/bad_err.txt"; then
        echo "FAIL: '${GETABSTIME} ${_bad_arg}' did not print usage message to stderr." >&2
        cat "${TMP_DIR}/bad_err.txt" >&2
        exit 1
    fi
done

echo "PASS: getabstime test passed (T1=${T1}, T2=${T2})."
exit 0
