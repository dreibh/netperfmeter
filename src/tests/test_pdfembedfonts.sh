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
#   test_pdfembedfonts.sh <pdfembedfonts_script>

set -eu

PDFEMBEDFONTS="${1:-pdfembedfonts}"
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_pdfembedfonts_test')

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

if [ ! -f "${PDFEMBEDFONTS}" ]; then
    echo "FAIL: Script '${PDFEMBEDFONTS}' not found." >&2
    exit 1
fi

if ! command -v bash >/dev/null 2>&1; then
    echo "FAIL: 'bash' is required to run '${PDFEMBEDFONTS}'." >&2
    exit 1
fi

if ! command -v ps2pdf >/dev/null 2>&1; then
    echo "FAIL: 'ps2pdf' (Ghostscript) is required to run '${PDFEMBEDFONTS}'." >&2
    exit 1
fi

# Helper: generate a minimal valid PDF 1.4 with an unembedded standard font (/Helvetica)
create_sample_pdf() {
    _target_pdf="$1"
    perl -e '
        use strict;
        use warnings;

        my ($out_path) = @ARGV;
        open(my $fh, ">:raw", $out_path) or die "Cannot write $out_path: $!\n";

        my @offsets;
        my $buf = "%PDF-1.4\n";

        $offsets[1] = length($buf);
        $buf .= "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

        $offsets[2] = length($buf);
        $buf .= "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";

        $offsets[3] = length($buf);
        $buf .= "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              . "/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>\nendobj\n";

        $offsets[4] = length($buf);
        $buf .= "4 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n";

        my $stream = "BT /F1 14 Tf 72 720 Td (NetPerfMeter PDF Font Embedding Test) Tj ET\n";
        $offsets[5] = length($buf);
        $buf .= "5 0 obj\n<< /Length " . length($stream) . " >>\nstream\n"
              . $stream . "endstream\nendobj\n";

        my $xref_offset = length($buf);
        $buf .= "xref\n0 6\n";
        $buf .= "0000000000 65535 f \n";
        for my $i (1 .. 5) {
            $buf .= sprintf("%010d 00000 n \n", $offsets[$i]);
        }
        $buf .= "trailer\n<< /Size 6 /Root 1 0 R >>\n";
        $buf .= "startxref\n${xref_offset}\n%%EOF\n";

        print $fh $buf;
        close($fh);
    ' "${_target_pdf}"
}

# Helper: validate PDF structure, PDF 1.7 compatibility level, and embedded font stream
verify_output_pdf() {
    _pdf_file="$1"
    _expect_linearized="${2:-0}"

    if [ ! -s "${_pdf_file}" ]; then
        echo "FAIL: Output PDF '${_pdf_file}' is missing or empty." >&2
        exit 1
    fi

    if [ -e "${_pdf_file}-tmp.pdf" ]; then
        echo "FAIL: Temporary file '${_pdf_file}-tmp.pdf' was left behind." >&2
        exit 1
    fi

    perl -e '
        use strict;
        use warnings;

        my ($pdf_path, $expect_lin) = @ARGV;
        open(my $fh, "<:raw", $pdf_path) or die "Cannot open $pdf_path: $!\n";
        local $/;
        my $data = <$fh>;
        close($fh);

        if ($data !~ /\A%PDF-1\.7/) {
            warn "FAIL: Expected %PDF-1.7 header in $pdf_path\n";
            exit 1;
        }
        if ($data !~ /%%EOF\s*\z/) {
            warn "FAIL: Missing %%EOF trailer in $pdf_path\n";
            exit 1;
        }
        # Ghostscript -dPDFSETTINGS=/prepress embeds font programs as /FontFile, /FontFile2, or /FontFile3
        if ($data !~ /\/FontFile[23]?\b/) {
            warn "FAIL: No embedded font stream (/FontFile*) found in $pdf_path\n";
            exit 1;
        }
        if ($expect_lin) {
            my $head = substr($data, 0, 1024);
            if ($head !~ /\/Linearized\b/) {
                warn "FAIL: Expected /Linearized dictionary in first 1024 bytes of $pdf_path\n";
                exit 1;
            }
        }
        exit 0;
    ' "${_pdf_file}" "${_expect_linearized}"

    if command -v qpdf >/dev/null 2>&1; then
        if ! qpdf --check "${_pdf_file}" >/dev/null 2>&1; then
            echo "FAIL: 'qpdf --check' reported structural errors in '${_pdf_file}'." >&2
            qpdf --check "${_pdf_file}" >&2 || true
            exit 1
        fi
    fi
}

# Helper: run command expecting success (exit 0) and empty stdout/stderr
run_expect_ok() {
    _out_file="$1"
    _err_file="$2"
    shift 2

    if ! "${PDFEMBEDFONTS}" "$@" >"${_out_file}" 2>"${_err_file}"; then
        echo "FAIL: '${PDFEMBEDFONTS} $*' exited with non-zero status." >&2
        cat "${_err_file}" >&2
        exit 1
    fi

    if [ -s "${_out_file}" ]; then
        echo "FAIL: Unexpected stdout output from '${PDFEMBEDFONTS} $*':" >&2
        cat "${_out_file}" >&2
        exit 1
    fi

    if [ -s "${_err_file}" ]; then
        echo "FAIL: Unexpected stderr output from '${PDFEMBEDFONTS} $*':" >&2
        cat "${_err_file}" >&2
        exit 1
    fi
}

# Helper: run command expecting failure (non-zero exit), empty stdout, and matching stderr
run_expect_fail() {
    _expected_regex="$1"
    shift

    _rc=0
    "${PDFEMBEDFONTS}" "$@" >"${TMP_DIR}/fail_out.txt" 2>"${TMP_DIR}/fail_err.txt" || _rc=$?

    if [ "${_rc}" -eq 0 ]; then
        echo "FAIL: '${PDFEMBEDFONTS} $*' unexpectedly succeeded (exit code 0)." >&2
        exit 1
    fi

    if [ -s "${TMP_DIR}/fail_out.txt" ]; then
        echo "FAIL: '${PDFEMBEDFONTS} $*' wrote unexpected output to stdout:" >&2
        cat "${TMP_DIR}/fail_out.txt" >&2
        exit 1
    fi

    if ! grep -Eq "${_expected_regex}" "${TMP_DIR}/fail_err.txt"; then
        echo "FAIL: '${PDFEMBEDFONTS} $*' stderr did not match '${_expected_regex}':" >&2
        cat "${TMP_DIR}/fail_err.txt" >&2
        exit 1
    fi
}

INPUT_PDF="${TMP_DIR}/input.pdf"
create_sample_pdf "${INPUT_PDF}"
cp "${INPUT_PDF}" "${TMP_DIR}/input.bak"

HAS_QPDF=0
if command -v qpdf >/dev/null 2>&1; then
    HAS_QPDF=1
fi

# ====== 1. Standard font embedding (without optimisation) =================
OUT_PLAIN="${TMP_DIR}/out_plain.pdf"
run_expect_ok "${TMP_DIR}/plain.out" "${TMP_DIR}/plain.err" "${INPUT_PDF}" "${OUT_PLAIN}"
verify_output_pdf "${OUT_PLAIN}" 0

if ! cmp -s "${INPUT_PDF}" "${TMP_DIR}/input.bak"; then
    echo "FAIL: Input PDF was modified during conversion." >&2
    exit 1
fi

# ====== 2. Optimised / linearised conversion (-O, --optimise, --optimize) ==
OUT_OPT_SHORT="${TMP_DIR}/out_opt_short.pdf"
run_expect_ok "${TMP_DIR}/opt1.out" "${TMP_DIR}/opt1.err" -O "${INPUT_PDF}" "${OUT_OPT_SHORT}"
verify_output_pdf "${OUT_OPT_SHORT}" "${HAS_QPDF}"

OUT_OPT_UK="${TMP_DIR}/out_opt_uk.pdf"
run_expect_ok "${TMP_DIR}/opt2.out" "${TMP_DIR}/opt2.err" --optimise "${INPUT_PDF}" "${OUT_OPT_UK}"
verify_output_pdf "${OUT_OPT_UK}" "${HAS_QPDF}"

OUT_OPT_US="${TMP_DIR}/out_opt_us.pdf"
run_expect_ok "${TMP_DIR}/opt3.out" "${TMP_DIR}/opt3.err" "${INPUT_PDF}" "${OUT_OPT_US}" --optimize
verify_output_pdf "${OUT_OPT_US}" "${HAS_QPDF}"

# ====== 3. File paths containing spaces ===================================
SPACE_DIR="${TMP_DIR}/dir with spaces"
mkdir -p "${SPACE_DIR}"
SPACE_IN="${SPACE_DIR}/input file.pdf"
SPACE_OUT="${SPACE_DIR}/output file.pdf"
cp "${INPUT_PDF}" "${SPACE_IN}"
run_expect_ok "${TMP_DIR}/space.out" "${TMP_DIR}/space.err" -O "${SPACE_IN}" "${SPACE_OUT}"
verify_output_pdf "${SPACE_OUT}" "${HAS_QPDF}"

# ====== 4. Negative / error handling tests ================================
# Wrong argument counts
run_expect_fail "Usage:"
run_expect_fail "Usage:" "${INPUT_PDF}"
run_expect_fail "Usage:" "${INPUT_PDF}" "${TMP_DIR}/extra1.pdf" "${TMP_DIR}/extra2.pdf"

# Invalid options
run_expect_fail "unrecognized option|invalid option|Usage:" --non-existent-option
run_expect_fail "invalid option|unrecognized option|Usage:" -Z

# Non-existent input file
run_expect_fail "ERROR: PDF file .* does not exist!" "${TMP_DIR}/missing.pdf" "${TMP_DIR}/missing_out.pdf"

# Corrupt / non-PDF input without -O must propagate Ghostscript failure
BAD_PDF="${TMP_DIR}/corrupt.pdf"
printf "This is not a valid PDF file.\n" >"${BAD_PDF}"
run_expect_fail "." "${BAD_PDF}" "${TMP_DIR}/corrupt_out.pdf"

echo "PASS: pdfembedfonts test passed."
exit 0
