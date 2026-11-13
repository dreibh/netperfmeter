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
#   test_extractvectors.sh <extractvectors_bin>

set -eu

EXTRACTVECTORS="${1:-extractvectors}"
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_extractvectors_test')

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

if [ ! -f "${EXTRACTVECTORS}" ]; then
    echo "FAIL: Executable '${EXTRACTVECTORS}' not found." >&2
    exit 1
fi

# Helper: run command expecting success (exit 0) and empty stderr
run_expect_ok() {
    _out_file="$1"
    _err_file="$2"
    shift 2

    if ! "${EXTRACTVECTORS}" "$@" >"${_out_file}" 2>"${_err_file}"; then
        echo "FAIL: '${EXTRACTVECTORS} $*' exited with non-zero status." >&2
        cat "${_out_file}" >&2
        cat "${_err_file}" >&2
        exit 1
    fi

    if [ -s "${_err_file}" ]; then
        echo "FAIL: Unexpected stderr output from '${EXTRACTVECTORS} $*':" >&2
        cat "${_err_file}" >&2
        exit 1
    fi
}

# Helper: run command expecting failure (non-zero exit), empty stdout, and expected stderr regex
run_expect_fail() {
    _expected_regex="$1"
    shift

    _rc=0
    "${EXTRACTVECTORS}" "$@" >"${TMP_DIR}/fail_out.txt" 2>"${TMP_DIR}/fail_err.txt" || _rc=$?

    if [ "${_rc}" -eq 0 ]; then
        echo "FAIL: '${EXTRACTVECTORS} $*' unexpectedly succeeded (exit code 0)." >&2
        exit 1
    fi

    if [ -s "${TMP_DIR}/fail_out.txt" ]; then
        echo "FAIL: '${EXTRACTVECTORS} $*' wrote unexpected output to stdout:" >&2
        cat "${TMP_DIR}/fail_out.txt" >&2
        exit 1
    fi

    if ! grep -Eq "${_expected_regex}" "${TMP_DIR}/fail_err.txt"; then
        echo "FAIL: '${EXTRACTVECTORS} $*' stderr did not match '${_expected_regex}':" >&2
        cat "${TMP_DIR}/fail_err.txt" >&2
        exit 1
    fi
}

# ====== 1. Create OMNeT++ v2 sample vector file (with mixed LF/CRLF) ======
SAMPLE_VEC="${TMP_DIR}/sample.vec"
perl -e '
    use strict;
    use warnings;

    my ($path) = @ARGV;
    open(my $fh, ">:raw", $path) or die "Cannot write $path: $!\n";
    my @lines = (
        "version 2",
        "run General-0-20261113-12:00:00-1234",
        "attr configname General",
        "vector 0 net.client[0].sctp \"Slow Start Threshold 2:Total\" ETV",
        "vector 1 net.client[0].sctp \"Congestion Window 2:Total\" ETV",
        "vector 2 net.server[0].sctp \"Congestion Window 4:Total\" ETV",
        "vector 3 net.client[0].app \"SingleWordVector\" ETV",
        "0\t174\t0.003672\t10000000",
        "1\t174\t0.003672\t4380",
        "2\t238\t0.003913\t3000",
        "3\t250\t0.005000\t42.5",
        "1\t260\t0.010000\t8760",
    );
    # Include both LF and CRLF line endings to verify \r stripping
    for my $i (0 .. $#lines) {
        my $eol = ($i % 2 == 1) ? "\r\n" : "\n";
        print $fh $lines[$i] . $eol;
    }
    close($fh);
' "${SAMPLE_VEC}"

# ====== 2. Default extraction (all vectors, TSV, no line numbers, no split) ==
OUT_ALL="${TMP_DIR}/all.data"
run_expect_ok "${TMP_DIR}/all.out" "${TMP_DIR}/all.err" "${SAMPLE_VEC}" "${OUT_ALL}"

if ! grep -q "Wrote 6 lines" "${TMP_DIR}/all.out"; then
    echo "FAIL: Expected 'Wrote 6 lines' summary on stdout." >&2
    cat "${TMP_DIR}/all.out" >&2
    exit 1
fi

perl -e '
    use strict;
    use warnings;

    my ($path) = @ARGV;
    open(my $fh, "<", $path) or die "Cannot open $path: $!\n";
    my @lines = <$fh>;
    close($fh);
    chomp @lines;

    if (@lines != 6) {
        warn "FAIL: Expected 6 lines (1 header + 5 data) in $path, got " . scalar(@lines) . "\n";
        exit 1;
    }
    if ($lines[0] ne "Time\tEvent\tObject\tVector\tSplit\tValue") {
        warn "FAIL: Unexpected header: \x27$lines[0]\x27\n";
        exit 1;
    }

    my @f1 = split(/\t/, $lines[1], -1);
    if (@f1 != 6 ||
        abs($f1[0] - 0.003672) > 1e-6 ||
        $f1[1] ne "174" ||
        $f1[2] ne "\"net.client[0].sctp\"" ||
        $f1[3] ne "\"Slow Start Threshold 2:Total\"" ||
        $f1[4] ne "\"\"" ||
        abs($f1[5] - 10000000.0) > 1e-3) {
        warn "FAIL: Unexpected row 1: \x27$lines[1]\x27\n";
        exit 1;
    }

    my @f4 = split(/\t/, $lines[4], -1);
    if (@f4 != 6 ||
        $f4[2] ne "\"net.client[0].app\"" ||
        $f4[3] ne "\"SingleWordVector\"" ||
        $f4[4] ne "\"\"" ||
        abs($f4[5] - 42.5) > 1e-6) {
        warn "FAIL: Unexpected single-word vector row 4: \x27$lines[4]\x27\n";
        exit 1;
    }
    exit 0;
' "${OUT_ALL}"

# ====== 3. Prefix filtering, selective '!' splitting, line numbers, custom separator & quiet mode
OUT_FILTERED="${TMP_DIR}/filtered.csv"
run_expect_ok "${TMP_DIR}/filt.out" "${TMP_DIR}/filt.err" \
    --line-numbers \
    --separator ";" \
    --quiet \
    "${SAMPLE_VEC}" "${OUT_FILTERED}" \
    "!Congestion Window" "SingleWordVector"

if grep -Eq "^(ExtractVectors |Wrote )" "${TMP_DIR}/filt.out"; then
    echo "FAIL: --quiet did not suppress banner/summary lines on stdout:" >&2
    cat "${TMP_DIR}/filt.out" >&2
    exit 1
fi

if ! grep -q 'Adding vector "Congestion Window", split "2:Total"' "${TMP_DIR}/filt.out"; then
    echo "FAIL: Expected 'Adding vector' status line on stdout:" >&2
    cat "${TMP_DIR}/filt.out" >&2
    exit 1
fi

perl -e '
    use strict;
    use warnings;

    my ($path) = @ARGV;
    open(my $fh, "<", $path) or die "Cannot open $path: $!\n";
    my @lines = <$fh>;
    close($fh);
    chomp @lines;

    # 1 header + 4 matching rows (2x Congestion Window 2:Total, 1x Congestion Window 4:Total, 1x SingleWordVector)
    if (@lines != 5) {
        warn "FAIL: Expected 5 lines in $path, got " . scalar(@lines) . "\n";
        exit 1;
    }
    if ($lines[0] ne "Time;Event;Object;Vector;Split;Value") {
        warn "FAIL: Unexpected semicolon header: \x27$lines[0]\x27\n";
        exit 1;
    }

    # Row 1: Congestion Window 2:Total split via "!" prefix -> line=1, Vector="Congestion Window", Split="2:Total"
    my @r1 = split(/;/, $lines[1], -1);
    if (@r1 != 7 ||
        $r1[0] ne "1" ||
        $r1[2] ne "174" ||
        $r1[4] ne "\"Congestion Window\"" ||
        $r1[5] ne "\"2:Total\"" ||
        abs($r1[6] - 4380.0) > 1e-6) {
        warn "FAIL: Unexpected row 1 with !split and line numbers: \x27$lines[1]\x27\n";
        exit 1;
    }

    # Row 3: SingleWordVector WITHOUT "!" prefix -> line=3, Vector="SingleWordVector", Split=""
    my @r3 = split(/;/, $lines[3], -1);
    if (@r3 != 7 ||
        $r3[0] ne "3" ||
        $r3[4] ne "\"SingleWordVector\"" ||
        $r3[5] ne "\"\"" ||
        abs($r3[6] - 42.5) > 1e-6) {
        warn "FAIL: Unexpected row 3 without split: \x27$lines[3]\x27\n";
        exit 1;
    }
    exit 0;
' "${OUT_FILTERED}"

# ====== 4. Global -p / --split and -a / --no-split + -n / --no-line-numbers
OUT_GLOBAL_SPLIT="${TMP_DIR}/global_split.data"
run_expect_ok "${TMP_DIR}/gsplit.out" "${TMP_DIR}/gsplit.err" \
    -l -n -a -p -q \
    "${SAMPLE_VEC}" "${OUT_GLOBAL_SPLIT}" \
    "Slow Start Threshold"

perl -e '
    use strict;
    use warnings;

    my ($path) = @ARGV;
    open(my $fh, "<", $path) or die "Cannot open $path: $!\n";
    my @lines = <$fh>;
    close($fh);
    chomp @lines;

    if (@lines != 2) {
        warn "FAIL: Expected 2 lines in $path, got " . scalar(@lines) . "\n";
        exit 1;
    }
    my @r1 = split(/\t/, $lines[1], -1);
    if (@r1 != 6 ||
        $r1[3] ne "\"Slow Start Threshold\"" ||
        $r1[4] ne "\"2:Total\"") {
        warn "FAIL: Global -p split failed: \x27$lines[1]\x27\n";
        exit 1;
    }
    exit 0;
' "${OUT_GLOBAL_SPLIT}"

# ====== 5. Compression formats round-trip (.gz, .bz2, .xz, .zst) ===========
for _ext in gz bz2 xz zst; do
    _out_comp="${TMP_DIR}/compressed.data.${_ext}"
    run_expect_ok "${TMP_DIR}/comp_${_ext}.out" "${TMP_DIR}/comp_${_ext}.err" \
        -c 1 -q "${SAMPLE_VEC}" "${_out_comp}"
    if [ ! -s "${_out_comp}" ]; then
        echo "FAIL: Compressed output '${_out_comp}' is missing or empty." >&2
        exit 1
    fi
done

# Verify compressed outputs with external decompressors when available, and
# test compressed input files (.vec.gz, .vec.bz2, .vec.xz, .vec.zst)
for _pair in "gz:gzip" "bz2:bzip2" "xz:xz" "zst:zstd"; do
    _ext="${_pair%%:*}"
    _tool="${_pair#*:}"
    if command -v "${_tool}" >/dev/null 2>&1; then
        "${_tool}" -dc "${TMP_DIR}/compressed.data.${_ext}" >"${TMP_DIR}/decomp_${_ext}.data"
        if ! cmp -s "${OUT_ALL}" "${TMP_DIR}/decomp_${_ext}.data"; then
            echo "FAIL: Decompressed .${_ext} output differs from plain output." >&2
            exit 1
        fi

        _in_comp="${TMP_DIR}/sample.vec.${_ext}"
        "${_tool}" -c "${SAMPLE_VEC}" >"${_in_comp}"
        _roundtrip_out="${TMP_DIR}/roundtrip_${_ext}.data"
        run_expect_ok "${TMP_DIR}/rt_${_ext}.out" "${TMP_DIR}/rt_${_ext}.err" \
            -q "${_in_comp}" "${_roundtrip_out}"
        if ! cmp -s "${OUT_ALL}" "${_roundtrip_out}"; then
            echo "FAIL: Round-trip extraction from .vec.${_ext} differs from plain extraction." >&2
            exit 1
        fi
    fi
done

# ====== 6. Partial prefix match warning (exits 0 with WARNING on stderr) ===
WARN_OUT="${TMP_DIR}/warn.data"
if ! "${EXTRACTVECTORS}" -q "${SAMPLE_VEC}" "${WARN_OUT}" "Congestion Window" "NonExistentPrefix" \
    >"${TMP_DIR}/warn.stdout" 2>"${TMP_DIR}/warn.stderr"; then
    echo "FAIL: Expected exit status 0 when 1 of 2 prefixes matched." >&2
    exit 1
fi
if grep -Eq "^(ExtractVectors |Wrote )" "${TMP_DIR}/warn.stdout"; then
    echo "FAIL: -q did not suppress banner/summary lines on stdout." >&2
    cat "${TMP_DIR}/warn.stdout" >&2
    exit 1
fi
if ! grep -q "WARNING: Found only 1 of 2 specified!" "${TMP_DIR}/warn.stderr"; then
    echo "FAIL: Expected partial-match warning on stderr, got:" >&2
    cat "${TMP_DIR}/warn.stderr" >&2
    exit 1
fi

# ====== 7. Negative / error handling tests ================================
# Missing / wrong argument counts and invalid options
run_expect_fail "Usage:"
run_expect_fail "Usage:" "${SAMPLE_VEC}"
run_expect_fail "Usage:" --non-existent-option
run_expect_fail "Usage:" -Z

# Invalid compression levels
run_expect_fail "ERROR: Compression level 1 is invalid for format Plain!" \
    -c 1 "${SAMPLE_VEC}" "${TMP_DIR}/bad_plain.data"
run_expect_fail "ERROR: Compression level 99 is invalid for format BZip2!" \
    -c 99 "${SAMPLE_VEC}" "${TMP_DIR}/bad_bz2.data.bz2"

# Non-existent input file (with -q so startup banner does not write to stdout)
run_expect_fail "ERROR:" -q "${TMP_DIR}/does_not_exist.vec" "${TMP_DIR}/no_out.data"

# Missing "version" header on line 1
NO_VER_VEC="${TMP_DIR}/no_version.vec"
printf "vector 0 net.node \"Test\" ETV\n0\t1\t0.1\t1.0\n" >"${NO_VER_VEC}"
run_expect_fail 'ERROR: Missing "version" entry in input file!' \
    -q "${NO_VER_VEC}" "${TMP_DIR}/no_ver_out.data"

# Unsupported version number (version 3)
BAD_VER_VEC="${TMP_DIR}/bad_version.vec"
printf "version 3\n" >"${BAD_VER_VEC}"
run_expect_fail "ERROR: Got unknown version number 3 \\(expected 2\\)" \
    -q "${BAD_VER_VEC}" "${TMP_DIR}/bad_ver_out.data"

# Malformed vector definition line
BAD_DEF_VEC="${TMP_DIR}/bad_def.vec"
printf "version 2\nvector malformed_entry\n" >"${BAD_DEF_VEC}"
run_expect_fail "ERROR: Unexpected vector definition on input file 2!" \
    -q "${BAD_DEF_VEC}" "${TMP_DIR}/bad_def_out.data"

echo "PASS: extractvectors test passed."
exit 0
