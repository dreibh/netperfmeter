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
#   test_combinesummaries.sh <combinesummaries_bin>

set -eu

COMBINESUMMARIES="${1:-combinesummaries}"
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_combinesummaries_test')

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

if [ ! -f "${COMBINESUMMARIES}" ]; then
    echo "FAIL: Executable '${COMBINESUMMARIES}' not found." >&2
    exit 1
fi

# Helper: run command with stdin file expecting success (exit 0) and empty stderr
run_expect_ok() {
    _stdin_file="$1"
    _out_file="$2"
    _err_file="$3"
    shift 3

    if ! "${COMBINESUMMARIES}" "$@" <"${_stdin_file}" >"${_out_file}" 2>"${_err_file}"; then
        echo "FAIL: '${COMBINESUMMARIES} $*' exited with non-zero status." >&2
        cat "${_out_file}" >&2
        cat "${_err_file}" >&2
        exit 1
    fi

    if [ -s "${_err_file}" ]; then
        echo "FAIL: Unexpected stderr output from '${COMBINESUMMARIES} $*':" >&2
        cat "${_err_file}" >&2
        exit 1
    fi
}

# Helper: run command with stdin file expecting failure (non-zero exit) and expected stderr regex
run_expect_fail() {
    _stdin_file="$1"
    _expected_regex="$2"
    shift 2

    _rc=0
    "${COMBINESUMMARIES}" "$@" <"${_stdin_file}" >"${TMP_DIR}/fail_out.txt" 2>"${TMP_DIR}/fail_err.txt" || _rc=$?

    if [ "${_rc}" -eq 0 ]; then
        echo "FAIL: '${COMBINESUMMARIES} $*' unexpectedly succeeded (exit code 0)." >&2
        exit 1
    fi

    if ! grep -Eq "${_expected_regex}" "${TMP_DIR}/fail_err.txt"; then
        echo "FAIL: '${COMBINESUMMARIES} $*' stderr did not match '${_expected_regex}':" >&2
        cat "${TMP_DIR}/fail_err.txt" >&2
        exit 1
    fi
}

# ====== 1. Create sample input summary tables (TSV & CSV, mixed LF/CRLF) ====
SIM_DIR="${TMP_DIR}/tables"
mkdir -p "${SIM_DIR}"

# Table 1 (TSV with CRLF line endings to verify \r stripping)
TBL1_TSV="${SIM_DIR}/part1.data"
perl -e '
    use strict;
    use warnings;

    my ($path) = @ARGV;
    open(my $fh, ">:raw", $path) or die "Cannot write $path: $!\n";
    print $fh "RunNo\tThroughput\r\n";
    print $fh "1\t100.5\r\n";
    print $fh "2\t200.0\r\n";
    close($fh);
' "${TBL1_TSV}"

# Table 2 (TSV with LF line endings)
TBL2_TSV="${SIM_DIR}/part2.data"
printf "RunNo\tThroughput\n1\t300.25\n2\t400.75\n" >"${TBL2_TSV}"

# Table 3 & 4 (Semicolon-separated tables for -s ";" testing)
TBL1_CSV="${SIM_DIR}/part1.csv"
printf "RunNo;Throughput\n1;100.5\n2;200.0\n" >"${TBL1_CSV}"

TBL2_CSV="${SIM_DIR}/part2.csv"
printf "RunNo;Throughput\n1;300.25\n" >"${TBL2_CSV}"

# Empty (0-byte) and header-only tables for edge-case testing
TBL_EMPTY="${SIM_DIR}/empty.data"
: >"${TBL_EMPTY}"

TBL_HEADER_ONLY="${SIM_DIR}/header_only.data"
printf "RunNo\tThroughput\n" >"${TBL_HEADER_ONLY}"

# ====== 2. Combine tables without line numbers (default TSV, verbose stdout, CRLF & blank stdin lines)
OUT_DEFAULT="${TMP_DIR}/combined_default.data"
CMD_DEFAULT="${TMP_DIR}/cmd_default.txt"
printf -- '--simulationsdirectory=%s\r\n\r\n--varnames="Protocol\tRate"\r\n--values="TCP\t10"\r\n--input=part1.data\r\n\n--values="UDP\t20"\r\n--input=part2.data\r\n' \
    "${SIM_DIR}" > "${CMD_DEFAULT}"

run_expect_ok "${CMD_DEFAULT}" "${TMP_DIR}/def.out" "${TMP_DIR}/def.err" "${OUT_DEFAULT}"

if ! grep -q "Ready> " "${TMP_DIR}/def.out"; then
    echo "FAIL: Expected 'Ready> ' interactive prompts on stdout without -q." >&2
    cat "${TMP_DIR}/def.out" >&2
    exit 1
fi

if ! grep -q "Wrote 5 lines" "${TMP_DIR}/def.out"; then
    echo "FAIL: Expected 'Wrote 5 lines' summary on stdout." >&2
    cat "${TMP_DIR}/def.out" >&2
    exit 1
fi

perl -e '
    use strict;
    use warnings;

    my ($path) = @ARGV;
    open(my $fh, "<:raw", $path) or die "Cannot open $path: $!\n";
    my @lines = <$fh>;
    close($fh);

    if (@lines != 5) {
        warn "FAIL: Expected 5 lines in $path, got " . scalar(@lines) . "\n";
        exit 1;
    }

    my @expected = (
        "Protocol\tRate\tRunNo\tThroughput\n",
        "TCP\t10\t1\t100.5\n",
        "TCP\t10\t2\t200.0\n",
        "UDP\t20\t1\t300.25\n",
        "UDP\t20\t2\t400.75\n",
    );
    for my $i (0 .. $#expected) {
        if ($lines[$i] ne $expected[$i]) {
            warn "FAIL: Line " . ($i + 1) . " mismatch. Expected \x27$expected[$i]\x27, got \x27$lines[$i]\x27\n";
            exit 1;
        }
    }
    exit 0;
' "${OUT_DEFAULT}"

# ====== 3. Combine tables with line numbers (-l), custom separator (-s ";"), quiet (-q), positional varnames
OUT_LINENUM="${TMP_DIR}/combined_linenum.csv"
CMD_LINENUM="${TMP_DIR}/cmd_linenum.txt"
cat << EOF > "${CMD_LINENUM}"
--simulationsdirectory=${SIM_DIR}
--values="TCP;10"
--input=part1.csv
--values="UDP;20"
--input=part2.csv
EOF

run_expect_ok "${CMD_LINENUM}" "${TMP_DIR}/ln.out" "${TMP_DIR}/ln.err" \
    --line-numbers --separator ";" --quiet "${OUT_LINENUM}" "Protocol;Rate"

if [ -s "${TMP_DIR}/ln.out" ]; then
    echo "FAIL: Expected empty stdout when --quiet is specified." >&2
    cat "${TMP_DIR}/ln.out" >&2
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

    if (@lines != 4) {
        warn "FAIL: Expected 4 lines in $path, got " . scalar(@lines) . "\n";
        exit 1;
    }

    my @expected = (
        "Protocol;Rate;SubLineNo;RunNo;Throughput",
        "1;TCP;10;1;100.5",
        "2;TCP;10;2;200.0",
        "3;UDP;20;1;300.25",
    );
    for my $i (0 .. $#expected) {
        if ($lines[$i] ne $expected[$i]) {
            warn "FAIL: Line " . ($i + 1) . " mismatch. Expected \x27$expected[$i]\x27, got \x27$lines[$i]\x27\n";
            exit 1;
        }
    }
    exit 0;
' "${OUT_LINENUM}"

# Also verify -l followed by -n (--no-line-numbers) disables line numbering
OUT_NOLINE="${TMP_DIR}/combined_noline.csv"
run_expect_ok "${CMD_LINENUM}" "${TMP_DIR}/nl.out" "${TMP_DIR}/nl.err" \
    -l -n -s ";" -q "${OUT_NOLINE}" "Protocol;Rate"

if ! head -n 1 "${OUT_NOLINE}" | grep -qx "Protocol;Rate;RunNo;Throughput"; then
    echo "FAIL: Expected -n / --no-line-numbers to override -l:" >&2
    head -n 1 "${OUT_NOLINE}" >&2
    exit 1
fi

# ====== 4. Edge cases: empty file, header-only file, escaped quotes & default compression
OUT_EDGE="${TMP_DIR}/combined_edge.data"
CMD_EDGE="${TMP_DIR}/cmd_edge.txt"
printf -- '--simulationsdirectory=%s\n--varnames="Protocol\tNote"\n--values="NONE\t\\"empty\\""\n--input=empty.data\n--values="HDR\t\\"only\\""\n--input=header_only.data\n--values="TCP\t\\"ok\\""\n--input=part1.data\n' \
    "${SIM_DIR}" > "${CMD_EDGE}"

run_expect_ok "${CMD_EDGE}" "${TMP_DIR}/edge.out" "${TMP_DIR}/edge.err" -q "${OUT_EDGE}"

perl -e '
    use strict;
    use warnings;

    my ($path) = @ARGV;
    open(my $fh, "<", $path) or die "Cannot open $path: $!\n";
    my @lines = <$fh>;
    close($fh);
    chomp @lines;

    # empty.data emits 0 lines; header_only.data emits header (line 1); part1.data skips header and emits 2 data rows
    if (@lines != 3) {
        warn "FAIL: Expected 3 lines in $path, got " . scalar(@lines) . "\n";
        exit 1;
    }
    if ($lines[0] ne "Protocol\tNote\tRunNo\tThroughput" ||
        $lines[1] ne "TCP\t\\\"ok\\\"\t1\t100.5" ||
        $lines[2] ne "TCP\t\\\"ok\\\"\t2\t200.0") {
        warn "FAIL: Unexpected edge-case output in $path:\n" . join("\n", @lines) . "\n";
        exit 1;
    }
    exit 0;
' "${OUT_EDGE}"

# ====== 5. Compression formats round-trip (.gz, .bz2, .xz, .zst) ===========
for _ext in gz bz2 xz zst; do
    _out_comp="${TMP_DIR}/combined.data.${_ext}"
    run_expect_ok "${CMD_DEFAULT}" "${TMP_DIR}/comp_${_ext}.out" "${TMP_DIR}/comp_${_ext}.err" \
        -c 1 -q "${_out_comp}"
    if [ ! -s "${_out_comp}" ]; then
        echo "FAIL: Compressed output '${_out_comp}' is missing or empty." >&2
        exit 1
    fi
done

# Also test default compression level (without -c) on .bz2 output
OUT_DEFAULT_BZ2="${TMP_DIR}/combined_deflevel.data.bz2"
run_expect_ok "${CMD_DEFAULT}" "${TMP_DIR}/defbz2.out" "${TMP_DIR}/defbz2.err" -q "${OUT_DEFAULT_BZ2}"
if [ ! -s "${OUT_DEFAULT_BZ2}" ]; then
    echo "FAIL: Default-level .bz2 output is missing or empty." >&2
    exit 1
fi

# Decompress and compare against plain output, plus test compressed inputs
for _pair in "gz:gzip" "bz2:bzip2" "xz:xz" "zst:zstd"; do
    _ext="${_pair%%:*}"
    _tool="${_pair#*:}"
    if command -v "${_tool}" >/dev/null 2>&1; then
        "${_tool}" -dc "${TMP_DIR}/combined.data.${_ext}" >"${TMP_DIR}/decomp_${_ext}.data"
        if ! cmp -s "${OUT_DEFAULT}" "${TMP_DIR}/decomp_${_ext}.data"; then
            echo "FAIL: Decompressed .${_ext} output differs from plain output." >&2
            exit 1
        fi

        "${_tool}" -c "${TBL1_TSV}" >"${SIM_DIR}/part1.data.${_ext}"
        "${_tool}" -c "${TBL2_TSV}" >"${SIM_DIR}/part2.data.${_ext}"
        CMD_COMP_IN="${TMP_DIR}/cmd_comp_${_ext}.txt"
        printf -- '--simulationsdirectory=%s\n--varnames="Protocol\tRate"\n--values="TCP\t10"\n--input=part1.data.%s\n--values="UDP\t20"\n--input=part2.data.%s\n' \
            "${SIM_DIR}" "${_ext}" "${_ext}" > "${CMD_COMP_IN}"
        OUT_COMP_IN="${TMP_DIR}/from_comp_${_ext}.data"
        run_expect_ok "${CMD_COMP_IN}" "${TMP_DIR}/incomp_${_ext}.out" "${TMP_DIR}/incomp_${_ext}.err" \
            -q "${OUT_COMP_IN}"
        if ! cmp -s "${OUT_DEFAULT}" "${OUT_COMP_IN}"; then
            echo "FAIL: Combining compressed .${_ext} input files differed from plain output." >&2
            exit 1
        fi
    fi
done

# ====== 6. Negative / error handling tests ================================
EMPTY_STDIN="${TMP_DIR}/empty_stdin.txt"
: >"${EMPTY_STDIN}"

# Missing output_file / extra positional args / unknown options
run_expect_fail "${EMPTY_STDIN}" "Usage:"
run_expect_fail "${EMPTY_STDIN}" "Usage:" --non-existent-option
run_expect_fail "${EMPTY_STDIN}" "Usage:" -Z
run_expect_fail "${EMPTY_STDIN}" "ERROR: Invalid option extra_arg!" "${TMP_DIR}/out.data" "Var1" extra_arg

# Invalid quotation in variable names or values
run_expect_fail "${EMPTY_STDIN}" "ERROR: Invalid quotation in variable names string!" \
    "${TMP_DIR}/out.data" 'Unclosed"Quote'

CMD_BAD_VAL_QUOTE="${TMP_DIR}/cmd_bad_val_quote.txt"
printf -- '--values="Unclosed\n' >"${CMD_BAD_VAL_QUOTE}"
run_expect_fail "${CMD_BAD_VAL_QUOTE}" "ERROR: Invalid quotation in variable values string!" \
    -q "${TMP_DIR}/out.data" "Var1"

# Invalid compression levels
run_expect_fail "${EMPTY_STDIN}" "ERROR: Compression level 1 is invalid for format Plain!" \
    -c 1 "${TMP_DIR}/bad_plain.data"
run_expect_fail "${EMPTY_STDIN}" "ERROR: Compression level 99 is invalid for format BZip2!" \
    -c 99 "${TMP_DIR}/bad_bz2.data.bz2"

# Missing --values= before --input= (initially and after a previous --input= clears varValues)
CMD_NO_VALS="${TMP_DIR}/cmd_no_vals.txt"
printf -- "--input=part1.data\n" >"${CMD_NO_VALS}"
run_expect_fail "${CMD_NO_VALS}" 'ERROR: No values given \(parameter --values=\.\.\.\)!' \
    -q "${TMP_DIR}/out.data" "Var1"

CMD_CONSECUTIVE_INPUT="${TMP_DIR}/cmd_consec_input.txt"
cat << EOF > "${CMD_CONSECUTIVE_INPUT}"
--simulationsdirectory=${SIM_DIR}
--values=TCP
--input=part1.data
--input=part2.data
EOF
run_expect_fail "${CMD_CONSECUTIVE_INPUT}" 'ERROR: No values given \(parameter --values=\.\.\.\)!' \
    -q "${TMP_DIR}/out.data" "Var1"

# Invalid stdin command
CMD_BAD_CMD="${TMP_DIR}/cmd_bad_cmd.txt"
printf -- "--invalid-directive=123\n" >"${CMD_BAD_CMD}"
run_expect_fail "${CMD_BAD_CMD}" "ERROR: Invalid command!" \
    -q "${TMP_DIR}/out.data" "Var1"

# Non-existent input file
CMD_MISSING_IN="${TMP_DIR}/cmd_missing_in.txt"
cat << EOF > "${CMD_MISSING_IN}"
--simulationsdirectory=${SIM_DIR}
--values=TCP
--input=does_not_exist.data
EOF
run_expect_fail "${CMD_MISSING_IN}" "ERROR:" -q "${TMP_DIR}/out.data" "Var1"

# Unwritable output file path (OutputFileException)
run_expect_fail "${EMPTY_STDIN}" "ERROR:" -q "${TMP_DIR}/non_existent_subdir/out.data" "Var1"

echo "PASS: combinesummaries test passed."
exit 0
