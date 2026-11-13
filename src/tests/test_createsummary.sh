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
#   test_createsummary.sh <createsummary_bin>

set -eu

CREATESUMMARY="${1:-createsummary}"
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_createsummary_test')

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

if [ ! -f "${CREATESUMMARY}" ]; then
    echo "FAIL: Executable '${CREATESUMMARY}' not found." >&2
    exit 1
fi

# Helper: run command with stdin file expecting success (exit 0) and empty stderr
run_expect_ok() {
    _stdin_file="$1"
    _out_file="$2"
    _err_file="$3"
    shift 3

    if ! "${CREATESUMMARY}" "$@" <"${_stdin_file}" >"${_out_file}" 2>"${_err_file}"; then
        echo "FAIL: '${CREATESUMMARY} $*' exited with non-zero status." >&2
        cat "${_out_file}" >&2
        cat "${_err_file}" >&2
        exit 1
    fi

    if [ -s "${_err_file}" ]; then
        echo "FAIL: Unexpected stderr output from '${CREATESUMMARIES:-$CREATESUMMARY} $*':" >&2
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
    "${CREATESUMMARY}" "$@" <"${_stdin_file}" >"${TMP_DIR}/fail_out.txt" 2>"${TMP_DIR}/fail_err.txt" || _rc=$?

    if [ "${_rc}" -eq 0 ]; then
        echo "FAIL: '${CREATESUMMARY} $*' unexpectedly succeeded (exit code 0)." >&2
        exit 1
    fi

    if ! grep -Eq "${_expected_regex}" "${TMP_DIR}/fail_err.txt"; then
        echo "FAIL: '${CREATESUMMARY} $*' stderr did not match '${_expected_regex}':" >&2
        cat "${TMP_DIR}/fail_err.txt" >&2
        exit 1
    fi
}

# ====== 1. Create OMNeT++ scalar input fixtures (plain & compressed) ======
SIM_DIR="${TMP_DIR}/simulations"
mkdir -p "${SIM_DIR}"

# Run 1 scalar file (with CRLF & LF, scalar entries, statistic/field blocks, and skipped scalar)
SCA1="${SIM_DIR}/run1.sca"
perl -e '
    use strict;
    use warnings;

    my ($path) = @ARGV;
    open(my $fh, ">:raw", $path) or die "Cannot write $path: $!\n";
    my @lines = (
        "version 2",
        "run General-0-20261113-12:00:00-1001",
        "attr configname General",
        "par NetPerfMeterScenario.client[0].app frameSize 1000",
        "scalar NetPerfMeterScenario.client[0].app \"Total Bytes Sent\" 12000",
        "scalar NetPerfMeterScenario.client[0].app \"Throughput 1\" 96.5",
        "scalar NetPerfMeterScenario.client[0].app \"Throughput 2\" 48.25",
        "scalar NetPerfMeterScenario.client[0].app \"IgnoredMetric\" 999",
        "statistic NetPerfMeterScenario.client[0].app \"Frame Delay\"",
        "field count 10",
        "field mean 0.0125",
        "field stddev 0.0025",
        "field sqrsum 0.001625",
        "field min 0.0080",
        "field max 0.0200",
    );
    for my $i (0 .. $#lines) {
        my $eol = ($i % 2 == 1) ? "\r\n" : "\n";
        print $fh $lines[$i] . $eol;
    }
    close($fh);
' "${SCA1}"

# Run 2 scalar file
SCA2="${SIM_DIR}/run2.sca"
cat << 'EOF' > "${SCA2}"
version 2
run General-1-20261113-12:01:00-1002
scalar NetPerfMeterScenario.client[0].app "Total Bytes Sent" 24000
scalar NetPerfMeterScenario.client[0].app "Throughput 1" 193.0
scalar NetPerfMeterScenario.client[0].app "Throughput 2" 96.5
scalar NetPerfMeterScenario.client[0].app "IgnoredMetric" 888
statistic NetPerfMeterScenario.client[0].app "Frame Delay"
field count 20
field mean 0.0150
field stddev 0.0030
field sqrsum 0.004680
field min 0.0090
field max 0.0250
EOF

# ====== 2. Batch mode with plain output, splitting (-p), line numbers (-l), custom separator
RES_PLAIN="${TMP_DIR}/results_plain"
mkdir -p "${RES_PLAIN}"

CMD_PLAIN="${TMP_DIR}/cmd_plain.txt"
cat << EOF > "${CMD_PLAIN}"
--simulationsdirectory=${SIM_DIR}
--resultsdirectory=${RES_PLAIN}
--varnames="Protocol;Rate"
--skip=client.app-IgnoredMetric
--values="TCP;10"
--input=run1.sca
--values="UDP;20"
--input=run2.sca
EOF

run_expect_ok "${CMD_PLAIN}" "${TMP_DIR}/plain.out" "${TMP_DIR}/plain.err" \
    --batch --format plain --compress 0 --separator ";" --line-numbers --split --quiet

if ! grep -q "Wrote 26 lines into 8 files" "${TMP_DIR}/plain.out"; then
    echo "FAIL: Expected 'Wrote 26 lines into 8 files' in stdout." >&2
    cat "${TMP_DIR}/plain.out" >&2
    exit 1
fi

if [ -e "${RES_PLAIN}/client.app-IgnoredMetric.data" ]; then
    echo "FAIL: Skipped scalar 'client.app-IgnoredMetric.data' should not have been created." >&2
    exit 1
fi

for _expected_file in \
    "${RES_PLAIN}/client.app-TotalBytesSent.data" \
    "${RES_PLAIN}/client.app-Throughput.data" \
    "${RES_PLAIN}/client.app-CountFrameDelay.data" \
    "${RES_PLAIN}/client.app-MeanFrameDelay.data" \
    "${RES_PLAIN}/client.app-StdDevFrameDelay.data" \
    "${RES_PLAIN}/client.app-SqrSumFrameDelay.data" \
    "${RES_PLAIN}/client.app-MinFrameDelay.data" \
    "${RES_PLAIN}/client.app-MaxFrameDelay.data"; do
    if [ ! -s "${_expected_file}" ]; then
        echo "FAIL: Expected summary file '${_expected_file}' is missing or empty." >&2
        exit 1
    fi
done

# Validate contents of split Throughput table, MeanFrameDelay, and SqrSumFrameDelay
perl -e '
    use strict;
    use warnings;

    my ($dir) = @ARGV;

    # 1. Check client.app-Throughput.data (4 data rows: 2 splits x 2 runs)
    open(my $fh1, "<", "$dir/client.app-Throughput.data") or die "Cannot open Throughput.data: $!\n";
    my @t_lines = <$fh1>;
    close($fh1);
    chomp @t_lines;

    if (@t_lines != 5) {
        warn "FAIL: Expected 5 lines in Throughput.data, got " . scalar(@t_lines) . "\n";
        exit 1;
    }
    if ($t_lines[0] ne "RunNo;ValueNo;Split;client;Protocol;Rate;client.app-Throughput") {
        warn "FAIL: Unexpected Throughput header: \x27$t_lines[0]\x27\n";
        exit 1;
    }

    my @r1 = split(/;/, $t_lines[1], -1);
    if (@r1 != 8 ||
        $r1[0] ne "1" || $r1[1] ne "1" || $r1[2] ne "1" ||
        $r1[3] ne "\"1\"" || $r1[4] ne "0" ||
        $r1[5] ne "TCP" || $r1[6] ne "10" ||
        abs($r1[7] - 96.5) > 1e-6) {
        warn "FAIL: Unexpected Throughput row 1: \x27$t_lines[1]\x27\n";
        exit 1;
    }

    my @r4 = split(/;/, $t_lines[4], -1);
    if (@r4 != 8 ||
        $r4[0] ne "4" || $r4[1] ne "1" || $r4[2] ne "1" ||
        $r4[3] ne "\"2\"" || $r4[4] ne "0" ||
        $r4[5] ne "UDP" || $r4[6] ne "20" ||
        abs($r4[7] - 96.5) > 1e-6) {
        warn "FAIL: Unexpected Throughput row 4: \x27$t_lines[4]\x27\n";
        exit 1;
    }

    # 2. Check client.app-MeanFrameDelay.data
    open(my $fh2, "<", "$dir/client.app-MeanFrameDelay.data") or die "Cannot open MeanFrameDelay.data: $!\n";
    my @m_lines = <$fh2>;
    close($fh2);
    chomp @m_lines;

    if (@m_lines != 3) {
        warn "FAIL: Expected 3 lines in MeanFrameDelay.data, got " . scalar(@m_lines) . "\n";
        exit 1;
    }
    my @m2 = split(/;/, $m_lines[2], -1);
    if (@m2 != 8 ||
        $m2[0] ne "2" || $m2[5] ne "UDP" || $m2[6] ne "20" ||
        abs($m2[7] - 0.0150) > 1e-6) {
        warn "FAIL: Unexpected MeanFrameDelay row 2: \x27$m_lines[2]\x27\n";
        exit 1;
    }

    # 3. Check client.app-SqrSumFrameDelay.data
    open(my $fh3, "<", "$dir/client.app-SqrSumFrameDelay.data") or die "Cannot open SqrSumFrameDelay.data: $!\n";
    my @s_lines = <$fh3>;
    close($fh3);
    chomp @s_lines;

    if (@s_lines != 3) {
        warn "FAIL: Expected 3 lines in SqrSumFrameDelay.data, got " . scalar(@s_lines) . "\n";
        exit 1;
    }
    my @s1 = split(/;/, $s_lines[1], -1);
    if (@s1 != 8 || abs($s1[7] - 0.001625) > 1e-7) {
        warn "FAIL: Unexpected SqrSumFrameDelay row 1: \x27$s_lines[1]\x27\n";
        exit 1;
    }

    exit 0;
' "${RES_PLAIN}"

# ====== 3. Multi-level aggregates, #<id> scalars, slash replacement, multi-run & repeated values
SCA_COMPLEX="${SIM_DIR}/complex.sca"
cat << 'EOF' > "${SCA_COMPLEX}"
version 2
run MultiRun-0-20261113-12:05:00-2001
scalar NetPerfMeterScenario.rack[1].node[2].app "Sent/Received" 10.5
scalar NetPerfMeterScenario.rack[1].node[2].app "Sent/Received" 20.5
scalar NetPerfMeterScenario.rack[1].node[2].app "queueLength#3" 7.0
scalar NetPerfMeterScenario.rack[1].node[2].app "FlowMetric 4" 55.0
run MultiRun-1-20261113-12:06:00-2002
scalar NetPerfMeterScenario.rack[1].node[2].app "Sent/Received" 30.5
scalar NetPerfMeterScenario.rack[1].node[2].app "queueLength#5" 9.0
scalar NetPerfMeterScenario.rack[1].node[2].app "FlowMetric 4" 66.0
EOF

RES_COMPLEX="${TMP_DIR}/results_complex"
mkdir -p "${RES_COMPLEX}"

CMD_COMPLEX="${TMP_DIR}/cmd_complex.txt"
cat << EOF > "${CMD_COMPLEX}"
--simulationsdirectory=${SIM_DIR}
--resultsdirectory=${RES_COMPLEX}
--splitall
--level=5
--values=ExpA
--input=complex.sca
EOF

run_expect_ok "${CMD_COMPLEX}" "${TMP_DIR}/complex.out" "${TMP_DIR}/complex.err" \
    -b -F plain -a -n -q "Experiment"

# Verify:
# 1) '/' replaced by ':' -> rack.node.app-Sent:Received.data (with RunNo=1 ValueNo=1,2 and RunNo=2 ValueNo=1)
# 2) '#3' and '#5' extracted into aggregate column queueLength -> rack.node.app-queueLength.data
# 3) --splitall on stdin split "FlowMetric 4" -> rack.node.app-FlowMetric.data with Split="4"
perl -e '
    use strict;
    use warnings;

    my ($dir) = @ARGV;

    # Check rack.node.app-Sent:Received.data
    open(my $fh1, "<", "$dir/rack.node.app-Sent:Received.data") or die "Cannot open Sent:Received.data: $!\n";
    my @sr = <$fh1>;
    close($fh1);
    chomp @sr;

    if (@sr != 4) {
        warn "FAIL: Expected 4 lines in Sent:Received.data, got " . scalar(@sr) . "\n";
        exit 1;
    }
    if ($sr[0] ne "RunNo\tValueNo\tSplit\track rack.node\tExperiment\track.node.app-Sent:Received") {
        warn "FAIL: Unexpected multi-level aggregate header: \x27$sr[0]\x27\n";
        exit 1;
    }
    if ($sr[1] ne "1\t1\t\"\"\t1 2\tExpA\t10.500000" ||
        $sr[2] ne "1\t2\t\"\"\t1 2\tExpA\t20.500000" ||
        $sr[3] ne "2\t1\t\"\"\t1 2\tExpA\t30.500000") {
        warn "FAIL: Unexpected multi-run / multi-value rows in Sent:Received.data:\n" . join("\n", @sr) . "\n";
        exit 1;
    }

    # Check rack.node.app-queueLength.data (#3 and #5 aggregate levels)
    open(my $fh2, "<", "$dir/rack.node.app-queueLength.data") or die "Cannot open queueLength.data: $!\n";
    my @ql = <$fh2>;
    close($fh2);
    chomp @ql;

    if (@ql != 3) {
        warn "FAIL: Expected 3 lines in queueLength.data, got " . scalar(@ql) . "\n";
        exit 1;
    }
    if ($ql[0] ne "RunNo\tValueNo\tSplit\track rack.node queueLength\tExperiment\track.node.app-queueLength") {
        warn "FAIL: Unexpected #id aggregate header: \x27$ql[0]\x27\n";
        exit 1;
    }
    if ($ql[1] ne "1\t1\t\"\"\t1 2 3\tExpA\t7.000000" ||
        $ql[2] ne "2\t1\t\"\"\t1 2 5\tExpA\t9.000000") {
        warn "FAIL: Unexpected #id aggregate rows:\n" . join("\n", @ql) . "\n";
        exit 1;
    }

    # Check rack.node.app-FlowMetric.data (--splitall via stdin)
    open(my $fh3, "<", "$dir/rack.node.app-FlowMetric.data") or die "Cannot open FlowMetric.data: $!\n";
    my @fm = <$fh3>;
    close($fh3);
    chomp @fm;

    if (@fm != 3 ||
        $fm[1] ne "1\t1\t\"4\"\t1 2\tExpA\t55.000000" ||
        $fm[2] ne "2\t1\t\"4\"\t1 2\tExpA\t66.000000") {
        warn "FAIL: --splitall via stdin failed:\n" . join("\n", @fm) . "\n";
        exit 1;
    }

    exit 0;
' "${RES_COMPLEX}"

# ====== 4. Compression formats round-trip (.bz2, .gz, .xz, .zst) & compressed inputs
for _pair in "bz2:.bz2:bzip2" "gz:.gz:gzip" "xz:.xz:xz" "zst:.zst:zstd"; do
    _fmt="${_pair%%:*}"
    _rest="${_pair#*:}"
    _ext="${_rest%%:*}"
    _tool="${_rest#*:}"

    _res_fmt="${TMP_DIR}/results_${_fmt}"
    mkdir -p "${_res_fmt}"
    _cmd_fmt="${TMP_DIR}/cmd_${_fmt}.txt"
    printf -- '--simulationsdirectory=%s\n--resultsdirectory=%s\n--values=TCP\t10\n--input=run1.sca\n' \
        "${SIM_DIR}" "${_res_fmt}" > "${_cmd_fmt}"
    run_expect_ok "${_cmd_fmt}" "${TMP_DIR}/fmt_${_fmt}.out" "${TMP_DIR}/fmt_${_fmt}.err" \
        -b -F "${_fmt}" -c 1 -q "$(printf 'Protocol\tRate')"

    _out_file="${_res_fmt}/client.app-TotalBytesSent.data${_ext}"
    if [ ! -s "${_out_file}" ]; then
        echo "FAIL: Expected compressed output '${_out_file}' is missing or empty." >&2
        exit 1
    fi

    if command -v "${_tool}" >/dev/null 2>&1; then
        "${_tool}" -dc "${_out_file}" >"${TMP_DIR}/decomp_${_fmt}.data"
        if ! grep -q '^RunNo	ValueNo	Split	client	Protocol	Rate	client.app-TotalBytesSent$' "${TMP_DIR}/decomp_${_fmt}.data"; then
            echo "FAIL: Decompressed ${_fmt} header mismatch." >&2
            exit 1
        fi
        if ! grep -q '^1	1	""	0	TCP	10	12000\.000000$' "${TMP_DIR}/decomp_${_fmt}.data"; then
            echo "FAIL: Decompressed ${_fmt} data row mismatch." >&2
            exit 1
        fi
    fi
done

# Test reading a compressed .sca.bz2 input file
if command -v bzip2 >/dev/null 2>&1; then
    bzip2 -c "${SCA1}" >"${SIM_DIR}/run1.sca.bz2"
    RES_COMP_IN="${TMP_DIR}/results_comp_in"
    mkdir -p "${RES_COMP_IN}"
    CMD_COMP_IN="${TMP_DIR}/cmd_comp_in.txt"
    cat << EOF > "${CMD_COMP_IN}"
--simulationsdirectory=${SIM_DIR}
--resultsdirectory=${RES_COMP_IN}
--values=TCP
--input=run1.sca.bz2
EOF
    run_expect_ok "${CMD_COMP_IN}" "${TMP_DIR}/compin.out" "${TMP_DIR}/compin.err" \
        -b -F plain -q "Protocol"
    if [ ! -s "${RES_COMP_IN}/client.app-TotalBytesSent.data" ]; then
        echo "FAIL: Failed to read compressed .sca.bz2 input file." >&2
        exit 1
    fi
fi

# ====== 5. Interactive mode (-i) resilience, skipping & unrecognized .sca lines
RES_INTER="${TMP_DIR}/results_inter"
mkdir -p "${RES_INTER}"

SCA_NOTE="${SIM_DIR}/with_note.sca"
cat << 'EOF' > "${SCA_NOTE}"
version 2
run NoteRun-0-20261113-12:10:00-3001
unrecognized_directive some_extra_tokens
scalar NetPerfMeterScenario.client[0].app "Total Bytes Sent" 42000
scalar NetPerfMeterScenario.client[0].app "IgnoredMetric" 123
EOF

CMD_INTER="${TMP_DIR}/cmd_inter.txt"
printf -- '--simulationsdirectory=%s\r\n\r\n--resultsdirectory=%s\r\n--skip=client.app-IgnoredMetric\r\n--bogus-interactive-command\r\n--values=INTER\r\n--input=with_note.sca\r\n' \
    "${SIM_DIR}" "${RES_INTER}" > "${CMD_INTER}"

if ! "${CREATESUMMARY}" -i -F plain "Mode" <"${CMD_INTER}" \
    >"${TMP_DIR}/inter.out" 2>"${TMP_DIR}/inter.err"; then
    echo "FAIL: Interactive mode (-i) should survive invalid stdin command and unrecognized .sca line." >&2
    cat "${TMP_DIR}/inter.err" >&2
    exit 1
fi

if ! grep -q "Ready> " "${TMP_DIR}/inter.out"; then
    echo "FAIL: Expected 'Ready> ' prompt in interactive mode." >&2
    exit 1
fi
if ! grep -q "Skipping entry client.app-IgnoredMetric" "${TMP_DIR}/inter.out"; then
    echo "FAIL: Expected 'Skipping entry client.app-IgnoredMetric' in interactive stdout." >&2
    exit 1
fi
if ! grep -q 'ERROR: Invalid command "--bogus-interactive-command"!' "${TMP_DIR}/inter.err"; then
    echo "FAIL: Expected invalid command error on stderr in interactive mode." >&2
    exit 1
fi
if ! grep -q 'Ignoring line "unrecognized_directive some_extra_tokens"' "${TMP_DIR}/inter.err"; then
    echo "FAIL: Expected NOTE about unrecognized .sca line on stderr." >&2
    exit 1
fi
if [ ! -s "${RES_INTER}/client.app-TotalBytesSent.data" ]; then
    echo "FAIL: Interactive run did not write output summary file." >&2
    exit 1
fi

# ====== 6. Error recovery (--ignore-scalar-file-errors via stdin + status/log files)
RES_ERR_IGN="${TMP_DIR}/results_err_ign"
mkdir -p "${RES_ERR_IGN}"
STATUS_FILE="${TMP_DIR}/sim.status"
LOG_FILE="${TMP_DIR}/sim.log"
printf "running\n" >"${STATUS_FILE}"
printf "simulation log\n" >"${LOG_FILE}"

CMD_ERR_IGN="${TMP_DIR}/cmd_err_ign.txt"
cat << EOF > "${CMD_ERR_IGN}"
--simulationsdirectory=${SIM_DIR}
--resultsdirectory=${RES_ERR_IGN}
--ignore-scalar-file-errors
--logfile=${LOG_FILE}
--statusfile=${STATUS_FILE}
--values=BAD
--input=non_existent.sca
--values=GOOD
--input=run1.sca
EOF

"${CREATESUMMARY}" -b -F plain -q "Tag" <"${CMD_ERR_IGN}" \
    >"${TMP_DIR}/errign.out" 2>"${TMP_DIR}/errign.err"

if [ -e "${STATUS_FILE}" ]; then
    echo "FAIL: Expected status file '${STATUS_FILE}' to be removed on scalar file error." >&2
    exit 1
fi
if ! grep -q "see logfile ${LOG_FILE}" "${TMP_DIR}/errign.err"; then
    echo "FAIL: Expected logfile reference in stderr." >&2
    cat "${TMP_DIR}/errign.err" >&2
    exit 1
fi
if ! grep -q "WARNING: Not all scalar files have been read -> continuing!" "${TMP_DIR}/errign.err"; then
    echo "FAIL: Expected continuation warning in stderr." >&2
    cat "${TMP_DIR}/errign.err" >&2
    exit 1
fi
if [ ! -s "${RES_ERR_IGN}/client.app-TotalBytesSent.data" ]; then
    echo "FAIL: Valid scalar file after ignored error was not written." >&2
    exit 1
fi

# ====== 7. Negative / error handling tests ================================
EMPTY_STDIN="${TMP_DIR}/empty_stdin.txt"
: >"${EMPTY_STDIN}"

# Invalid options / extra positional args / invalid format / invalid compression level / bad quotes
run_expect_fail "${EMPTY_STDIN}" "Usage:" --non-existent-option
run_expect_fail "${EMPTY_STDIN}" "Usage:" -Z
run_expect_fail "${EMPTY_STDIN}" "ERROR: Invalid option extra_arg!" "Var1" extra_arg
run_expect_fail "${EMPTY_STDIN}" 'ERROR: Invalid output format "invalidfmt"!' -F invalidfmt
run_expect_fail "${EMPTY_STDIN}" "ERROR: Compression level 1 is invalid for format Plain!" -F plain -c 1
run_expect_fail "${EMPTY_STDIN}" "ERROR: Compression level 99 is invalid for format BZip2!" -F bz2 -c 99
run_expect_fail "${EMPTY_STDIN}" "ERROR: Invalid quotation in variable names string!" 'Unclosed"Quote'

CMD_BAD_VAL_QUOTE="${TMP_DIR}/cmd_bad_val_quote.txt"
printf -- '--values="Unclosed\n' >"${CMD_BAD_VAL_QUOTE}"
run_expect_fail "${CMD_BAD_VAL_QUOTE}" "ERROR: Invalid quotation in variable values string!" -b

# Missing --values= before --input=
CMD_NO_VALS="${TMP_DIR}/cmd_no_vals.txt"
printf -- "--input=run1.sca\n" >"${CMD_NO_VALS}"
run_expect_fail "${CMD_NO_VALS}" 'ERROR: No values given \(parameter --values=\.\.\.\)!' -b

# Invalid stdin command in batch mode
CMD_BAD_CMD="${TMP_DIR}/cmd_bad_cmd.txt"
printf -- "--unknown-command=123\n" >"${CMD_BAD_CMD}"
run_expect_fail "${CMD_BAD_CMD}" 'ERROR: Invalid command "--unknown-command=123"!' -b

# Missing scalar file without -r / --ignore-scalar-file-errors must abort
CMD_MISSING_SCA="${TMP_DIR}/cmd_missing_sca.txt"
cat << EOF > "${CMD_MISSING_SCA}"
--simulationsdirectory=${SIM_DIR}
--values=TCP
--input=missing_file.sca
EOF
run_expect_fail "${CMD_MISSING_SCA}" "ERROR: Not all scalar files have been read -> aborting!" -b

# Malformed .sca file diagnostics
check_bad_sca() {
    _sca_name="$1"
    _sca_body="$2"
    _err_regex="$3"

    printf '%s\n' "${_sca_body}" >"${SIM_DIR}/${_sca_name}"
    _cmd="${TMP_DIR}/cmd_${_sca_name}.txt"
    cat << EOF > "${_cmd}"
--simulationsdirectory=${SIM_DIR}
--values=X
--input=${_sca_name}
EOF
    run_expect_fail "${_cmd}" "${_err_regex}" -b -q "Var"
}

check_bad_sca "bad_scalar_noobj.sca"  "scalar "                   'Object name expected for "scalar"!'
check_bad_sca "bad_scalar_nostat.sca" "scalar Net.app"            'Statistics name expected for "scalar"!'
check_bad_sca "bad_scalar_noval.sca"  "scalar Net.app Stat badval" 'Value expected!'
check_bad_sca "bad_field_nostat.sca"  "field count 10"            '"field" without "statistic"!'
check_bad_sca "bad_stat_noobj.sca"    "statistic "                'Object name expected for "statistic"!'
check_bad_sca "bad_stat_nostat.sca"   "statistic Net.app"         'Statistics name expected for "statistic"!'
check_bad_sca "bad_field_noval.sca"   "statistic Net.app S
field count badval" 'Value expected!'

echo "PASS: createsummary test passed."
exit 0
