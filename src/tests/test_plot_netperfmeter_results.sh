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
#   test_plot_netperfmeter_results.sh <plot_script> <combinesummaries_bin> <plot_r_dir> <netperfmeter_bin> <port>

set -eu

PLOT_SCRIPT="${1:-plot-netperfmeter-results}"
COMBINESUMMARIES_BIN="${2:-combinesummaries}"
PLOT_R_DIR="${3:-.}"
NETPERFMETER_BIN="${4:-netperfmeter}"
PORT="${5:-25100}"

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_plot_test')
SERVER_PID=""
CLIENT_PID=""

cleanup_processes() {
    for _pid in ${CLIENT_PID} ${SERVER_PID}; do
        if [ -n "${_pid}" ] && kill -0 "${_pid}" 2>/dev/null; then
            kill -TERM "${_pid}" 2>/dev/null || true
        fi
    done
    for _pid in ${CLIENT_PID} ${SERVER_PID}; do
        if [ -n "${_pid}" ]; then
            wait "${_pid}" 2>/dev/null || true
        fi
    done
    CLIENT_PID=""
    SERVER_PID=""
}

cleanup() {
    trap - EXIT INT TERM
    cleanup_processes
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

for _req in "${PLOT_SCRIPT}" "${COMBINESUMMARIES_BIN}" "${NETPERFMETER_BIN}"; do
    if [ ! -f "${_req}" ]; then
        echo "FAIL: Required executable/script '${_req}' not found." >&2
        exit 1
    fi
done

if [ ! -f "${PLOT_R_DIR}/plot-netperfmeter-results.R" ] || [ ! -f "${PLOT_R_DIR}/plotter.R" ]; then
    echo "FAIL: R plotting scripts not found in '${PLOT_R_DIR}'." >&2
    exit 1
fi

if ! command -v bash >/dev/null 2>&1; then
    echo "FAIL: 'bash' is required to run '${PLOT_SCRIPT}'." >&2
    exit 1
fi

# ====== 1. Prepare isolated working directory with local tools & R scripts ==
WORK_DIR="${TMP_DIR}/work"
mkdir -p "${WORK_DIR}"
ln -s "${COMBINESUMMARIES_BIN}" "${WORK_DIR}/combinesummaries"
ln -s "${PLOT_R_DIR}/plot-netperfmeter-results.R" "${WORK_DIR}/plot-netperfmeter-results.R"
ln -s "${PLOT_R_DIR}/plotter.R" "${WORK_DIR}/plotter.R"

PLOT_SCRIPT_DIR=$(dirname "${PLOT_SCRIPT}")
for _helper in pdfembedfonts setpdfmetadata; do
    if [ -f "${PLOT_SCRIPT_DIR}/${_helper}.versioned" ]; then
        ln -s "${PLOT_SCRIPT_DIR}/${_helper}.versioned" "${WORK_DIR}/${_helper}"
    fi
done

PATH="${WORK_DIR}:${PATH}"
export PATH

# ====== 2. Generate authentic NetPerfMeter .config and .vec files =========
# Run inside WORK_DIR with relative filenames so measurement.config records
# relative vector paths, matching combinesummaries' default "--simulationsdirectory=."
CONTROL_PORT=$((PORT + 1))

"${NETPERFMETER_BIN}" "${PORT}" -control-over-tcp -no-control-over-sctp >"${TMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

_elapsed=0
while [ "${_elapsed}" -lt 5 ]; do
    if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
        echo "FAIL: Passive netperfmeter server terminated early." >&2
        cat "${TMP_DIR}/server.log" >&2
        exit 1
    fi
    if perl -MIO::Socket::IP -e \
       'exit(!IO::Socket::IP->new(PeerHost => "127.0.0.1", PeerPort => $ARGV[0], Proto => "tcp", GetAddrInfoFlags => 0, Timeout => 1))' \
       "${CONTROL_PORT}" 2>/dev/null; then
        break
    fi
    sleep 1
    _elapsed=$((_elapsed + 1))
done

(
    cd "${WORK_DIR}"
    "${NETPERFMETER_BIN}" "127.0.0.1:${PORT}" \
        -control-over-tcp \
        -runtime=2 \
        --config="measurement.config" \
        --scalar="measurement.sca" \
        --vector="measurement.vec" \
        -udp const10:const1000:const10:const1000 \
        >"${TMP_DIR}/client.log" 2>&1
) &
CLIENT_PID=$!
wait "${CLIENT_PID}"
CLIENT_PID=""

if kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill -INT "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
fi
SERVER_PID=""

if [ ! -s "${WORK_DIR}/measurement.config" ]; then
    echo "FAIL: Failed to generate measurement.config via netperfmeter." >&2
    cat "${TMP_DIR}/client.log" >&2
    exit 1
fi

# If R is not installed on a minimal build host, provide a fallback R stub that
# validates arguments and emits minimal valid PDFs so shell & combinesummaries
# logic are still exercised end-to-end.
HAS_REAL_R=1
if ! command -v R >/dev/null 2>&1; then
    HAS_REAL_R=0
    MOCK_BIN="${TMP_DIR}/mock_bin"
    mkdir -p "${MOCK_BIN}"
    cat << 'EOF' > "${MOCK_BIN}/R"
#!/bin/sh
set -eu
if [ "${1:-}" != "CMD" ] || [ "${2:-}" != "BATCH" ]; then
    echo "Mock R expected 'CMD BATCH', got: $*" >&2
    exit 1
fi
ARGS_BLOCK=""
for _arg in "$@"; do
    case "${_arg}" in
        --args*) ARGS_BLOCK="${_arg}" ;;
    esac
done
PDF_PREFIX=$(printf '%s\n' "${ARGS_BLOCK}" | sed -n 's/.*pdfFilePrefix="\([^"]*\)".*/\1/p')
OWN_FILE=$(printf '%s\n' "${ARGS_BLOCK}" | sed -n 's/.*plotOwnFile=\([A-Z]*\).*/\1/p')
if [ -z "${PDF_PREFIX}" ]; then
    echo "Mock R missing pdfFilePrefix in args: ${ARGS_BLOCK}" >&2
    exit 1
fi
if [ "${OWN_FILE}" = "TRUE" ]; then
    printf '%%PDF-1.4\n1 0 obj<<>>endobj\ntrailer<<>>\n%%%%EOF\n' > "${PDF_PREFIX}-Plot1.pdf"
    printf '%%PDF-1.4\n1 0 obj<<>>endobj\ntrailer<<>>\n%%%%EOF\n' > "${PDF_PREFIX}-Plot2.pdf"
else
    printf '%%PDF-1.4\n1 0 obj<<>>endobj\ntrailer<<>>\n%%%%EOF\n' > "${PDF_PREFIX}.pdf"
fi
EOF
    chmod +x "${MOCK_BIN}/R"
    PATH="${MOCK_BIN}:${PATH}"
    export PATH
fi

verify_pdf_file() {
    _pdf="$1"
    if [ ! -s "${_pdf}" ]; then
        echo "FAIL: Expected PDF output '${_pdf}' was not created or is empty." >&2
        exit 1
    fi
    perl -e '
        use strict;
        use warnings;
        my ($path) = @ARGV;
        open(my $fh, "<:raw", $path) or die "Cannot open $path: $!\n";
        local $/;
        my $data = <$fh>;
        close($fh);
        if ($data !~ /\A%PDF-1\./) {
            warn "FAIL: $path does not start with %PDF-1.x header\n";
            exit 1;
        }
        if ($data !~ /%%EOF\s*\z/) {
            warn "FAIL: $path does not end with %%EOF\n";
            exit 1;
        }
        exit 0;
    ' "${_pdf}"
}

verify_own_pdf_files() {
    _prefix="$1"
    if [ -e "${_prefix}.pdf" ]; then
        echo "FAIL: Combined PDF '${_prefix}.pdf' should not be created when -w / --own-file is enabled." >&2
        exit 1
    fi

    _count=0
    for _f in "${_prefix}"-*.pdf; do
        if [ ! -e "${_f}" ]; then
            echo "FAIL: No individual per-plot PDF files (${_prefix}-*.pdf) were created." >&2
            exit 1
        fi
        verify_pdf_file "${_f}"
        _count=$((_count + 1))
    done

    if [ "${_count}" -lt 2 ]; then
        echo "FAIL: Expected multiple individual per-plot PDF files, found ${_count}." >&2
        exit 1
    fi
}

# ====== 3. Default plotting run (cleans up intermediate .data.bz2 files) ===
(
    cd "${WORK_DIR}"
    if ! "${PLOT_SCRIPT}" "measurement.config" >"${TMP_DIR}/plot_default.out" 2>"${TMP_DIR}/plot_default.err"; then
        echo "FAIL: Default plot-netperfmeter-results run failed." >&2
        cat "${TMP_DIR}/plot_default.out" >&2
        cat "${TMP_DIR}/plot_default.err" >&2
        exit 1
    fi
)

verify_pdf_file "${WORK_DIR}/measurement.pdf"

if [ -e "${DATA_DIR:-${WORK_DIR}}/measurement-summary.data.bz2" ] || [ -e "${WORK_DIR}/measurement-flows.data.bz2" ]; then
    echo "FAIL: Temporary .data.bz2 summary files were not cleaned up when -k was omitted." >&2
    exit 1
fi

# ====== 4. Run with -k -p -w (kept summaries + per-flow + own PDF files) ==
rm -f "${WORK_DIR}"/measurement*.pdf
(
    cd "${WORK_DIR}"
    if ! "${PLOT_SCRIPT}" -k -p -w "measurement.config" >"${TMP_DIR}/plot_opts.out" 2>"${TMP_DIR}/plot_opts.err"; then
        echo "FAIL: plot-netperfmeter-results -k -p -w failed." >&2
        cat "${TMP_DIR}/plot_opts.out" >&2
        cat "${TMP_DIR}/plot_opts.err" >&2
        exit 1
    fi
)

verify_own_pdf_files "${WORK_DIR}/measurement"

# Verify intermediate BZip2 tables were kept and contain valid combined data
for _sum in "${WORK_DIR}/measurement-summary.data.bz2" "${WORK_DIR}/measurement-flows.data.bz2"; do
    if [ ! -s "${_sum}" ]; then
        echo "FAIL: Expected kept summary file '${_sum}' is missing or empty." >&2
        exit 1
    fi
done

if ! bzip2 -dc "${WORK_DIR}/measurement-summary.data.bz2" | grep -q "NodeName"; then
    echo "FAIL: measurement-summary.data.bz2 missing expected 'NodeName' header." >&2
    exit 1
fi

if ! bzip2 -dc "${WORK_DIR}/measurement-flows.data.bz2" | grep -q "FlowName"; then
    echo "FAIL: measurement-flows.data.bz2 missing expected 'FlowName' header." >&2
    exit 1
fi

# ====== 5. Long option aliases (--keep-summaries, --per-flow-plots, --own-file)
# Test --keep-summaries --per-flow-plots in combined PDF mode (exercises per-flow
# outline bookmarks in setpdfmetadata without duplicating 46 Ghostscript runs)
rm -f "${WORK_DIR}"/measurement*.pdf "${WORK_DIR}/measurement-summary.data.bz2" "${WORK_DIR}/measurement-flows.data.bz2"
(
    cd "${WORK_DIR}"
    if ! "${PLOT_SCRIPT}" --keep-summaries --per-flow-plots "measurement.config" >"${TMP_DIR}/plot_long_pf.out" 2>"${TMP_DIR}/plot_long_pf.err"; then
        echo "FAIL: plot-netperfmeter-results --keep-summaries --per-flow-plots failed." >&2
        cat "${TMP_DIR}/plot_long_pf.out" >&2
        cat "${TMP_DIR}/plot_long_pf.err" >&2
        exit 1
    fi
)
verify_pdf_file "${WORK_DIR}/measurement.pdf"
if [ ! -s "${WORK_DIR}/measurement-summary.data.bz2" ] || [ ! -s "${WORK_DIR}/measurement-flows.data.bz2" ]; then
    echo "FAIL: --keep-summaries did not preserve .data.bz2 files." >&2
    exit 1
fi

# Test --own-file long option alias (node-level own PDFs)
rm -f "${WORK_DIR}"/measurement*.pdf "${WORK_DIR}/measurement-summary.data.bz2" "${WORK_DIR}/measurement-flows.data.bz2"
(
    cd "${WORK_DIR}"
    if ! "${PLOT_SCRIPT}" --own-file "measurement.config" >"${TMP_DIR}/plot_long_own.out" 2>"${TMP_DIR}/plot_long_own.err"; then
        echo "FAIL: plot-netperfmeter-results --own-file failed." >&2
        cat "${TMP_DIR}/plot_long_own.out" >&2
        cat "${TMP_DIR}/plot_long_own.err" >&2
        exit 1
    fi
)
verify_own_pdf_files "${WORK_DIR}/measurement"

# ====== 6. Negative / error handling tests ================================
run_expect_fail() {
    _expected_regex="$1"
    shift

    _rc=0
    (
        cd "${WORK_DIR}"
        "${PLOT_SCRIPT}" "$@" >"${TMP_DIR}/fail_out.txt" 2>"${TMP_DIR}/fail_err.txt"
    ) || _rc=$?

    if [ "${_rc}" -eq 0 ]; then
        echo "FAIL: '${PLOT_SCRIPT} $*' unexpectedly succeeded (exit code 0)." >&2
        exit 1
    fi

    if ! grep -Eq "${_expected_regex}" "${TMP_DIR}/fail_err.txt"; then
        echo "FAIL: '${PLOT_SCRIPT} $*' stderr did not match '${_expected_regex}':" >&2
        cat "${TMP_DIR}/fail_err.txt" >&2
        exit 1
    fi
}

# Wrong argument counts and invalid options
run_expect_fail "Usage:"
run_expect_fail "Usage:" "measurement.config" extra_arg
run_expect_fail "unrecognized option|invalid option|Usage:" --non-existent-option
run_expect_fail "invalid option|unrecognized option|Usage:" -Z

# Non-existent config file
run_expect_fail "ERROR: Config file .* not found!" "does_not_exist.config"

# Bad vector configuration (NUM_FLOWS=0)
cat << 'EOF' > "${WORK_DIR}/bad.config"
NAME_ACTIVE_NODE="Active"
NAME_PASSIVE_NODE="Passive"
VECTOR_ACTIVE_NODE="measurement-active.vec"
VECTOR_PASSIVE_NODE="measurement-passive.vec"
NUM_FLOWS=0
EOF
run_expect_fail "ERROR: Bad vector file configuration!" "bad.config"

# Missing active vector file
cat << 'EOF' > "${WORK_DIR}/missing_act.config"
NAME_ACTIVE_NODE="Active"
NAME_PASSIVE_NODE="Passive"
VECTOR_ACTIVE_NODE="no_such_active.vec"
VECTOR_PASSIVE_NODE="measurement-passive.vec"
NUM_FLOWS=1
EOF
run_expect_fail "ERROR: Vector file .* of active node not found!" "missing_act.config"

# Missing passive vector file
cat << 'EOF' > "${WORK_DIR}/missing_pas.config"
NAME_ACTIVE_NODE="Active"
NAME_PASSIVE_NODE="Passive"
VECTOR_ACTIVE_NODE="measurement-active.vec"
VECTOR_PASSIVE_NODE="no_such_passive.vec"
NUM_FLOWS=1
EOF
run_expect_fail "ERROR: Vector file .* of passive node not found!" "missing_pas.config"

# Missing R plotting script (only if not installed system-wide)
if [ ! -e "/usr/share/netperfmeter/plot-netperfmeter-results.R" ] && \
   [ ! -e "/usr/local/share/netperfmeter/plot-netperfmeter-results.R" ]; then
    EMPTY_WORK="${TMP_DIR}/empty_work"
    mkdir -p "${EMPTY_WORK}"
    cp "${WORK_DIR}/measurement.config" "${WORK_DIR}"/measurement*.vec "${EMPTY_WORK}/"
    _rc=0
    (
        cd "${EMPTY_WORK}"
        "${PLOT_SCRIPT}" "measurement.config" >"${TMP_DIR}/nor_out.txt" 2>"${TMP_DIR}/nor_err.txt"
    ) || _rc=$?
    if [ "${_rc}" -eq 0 ] || ! grep -q "ERROR: Cannot find plot-netperfmeter-results.R!" "${TMP_DIR}/nor_err.txt"; then
        echo "FAIL: Expected missing R script error in empty directory." >&2
        cat "${TMP_DIR}/nor_err.txt" >&2 || true
        exit 1
    fi
fi

echo "PASS: plot-netperfmeter-results test passed (HAS_REAL_R=${HAS_REAL_R})."
exit 0
