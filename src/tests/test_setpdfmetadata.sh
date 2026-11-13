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
#   test_setpdfmetadata.sh <setpdfmetadata_script>

set -eu

SETPDFMETADATA="${1:-setpdfmetadata}"
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'npm_setpdfmetadata_test')

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

if [ ! -f "${SETPDFMETADATA}" ]; then
    echo "FAIL: Script '${SETPDFMETADATA}' not found." >&2
    exit 1
fi

if ! perl -MPDF::API2 -e 1 >/dev/null 2>&1; then
    echo "FAIL: Perl module 'PDF::API2' is required to run '${SETPDFMETADATA}'." >&2
    exit 1
fi

# Helper: run command expecting success (exit 0) and empty stdout/stderr
run_expect_ok() {
    _out_file="$1"
    _err_file="$2"
    shift 2

    if ! "${SETPDFMETADATA}" "$@" >"${_out_file}" 2>"${_err_file}"; then
        echo "FAIL: '${SETPDFMETADATA} $*' exited with non-zero status." >&2
        cat "${_err_file}" >&2
        exit 1
    fi

    if [ -s "${_out_file}" ]; then
        echo "FAIL: Unexpected stdout output from '${SETPDFMETADATA} $*':" >&2
        cat "${_out_file}" >&2
        exit 1
    fi

    if [ -s "${_err_file}" ]; then
        echo "FAIL: Unexpected stderr output from '${SETPDFMETADATA} $*':" >&2
        cat "${_err_file}" >&2
        exit 1
    fi
}

# Helper: run command expecting failure (non-zero exit), empty stdout, and matching stderr
run_expect_fail() {
    _expected_regex="$1"
    shift

    _rc=0
    "${SETPDFMETADATA}" "$@" >"${TMP_DIR}/fail_out.txt" 2>"${TMP_DIR}/fail_err.txt" || _rc=$?

    if [ "${_rc}" -eq 0 ]; then
        echo "FAIL: '${SETPDFMETADATA} $*' unexpectedly succeeded (exit code 0)." >&2
        exit 1
    fi

    if [ -s "${TMP_DIR}/fail_out.txt" ]; then
        echo "FAIL: '${SETPDFMETADATA} $*' wrote unexpected output to stdout:" >&2
        cat "${TMP_DIR}/fail_out.txt" >&2
        exit 1
    fi

    if ! grep -Eq "${_expected_regex}" "${TMP_DIR}/fail_err.txt"; then
        echo "FAIL: '${SETPDFMETADATA} $*' stderr did not match '${_expected_regex}':" >&2
        cat "${TMP_DIR}/fail_err.txt" >&2
        exit 1
    fi
}

# ====== 1. Setup minimal multi-page PDF & valid metadata file =============
INPUT_PDF="${TMP_DIR}/input.pdf"
perl -e '
    use strict;
    use warnings;
    use PDF::API2;

    my $pdf = PDF::API2->new();
    for (1..10) {
        my $page = $pdf->page();
        my $text = $page->text();
        $text->font($pdf->corefont("Helvetica"), 20);
        $text->translate(200, 700);
        $text->text("Page $_");
    }
    $pdf->saveas($ARGV[0]);
' "${INPUT_PDF}"

META_FILE="${TMP_DIR}/meta.dat"
cat << 'EOF' > "${META_FILE}"
# Comments should be ignored
// And C++ style comments too

title Test Document Title
subject Test Subject with utf8: äöü
author Test Author
creator Test Creator
producer Test Producer
keywords test, metadata, utf8, åäö

outline 1 1 Chapter 1
outline 2 2 Section 1.1
outline 2 4 Section 1.2
outline 3 5 Subsection 1.2.1
outline 1 6 Chapter 2
EOF

# ====== 2. Functional application of metadata =============================
OUTPUT_PDF="${TMP_DIR}/output.pdf"
run_expect_ok "${TMP_DIR}/ok.out" "${TMP_DIR}/ok.err" "${INPUT_PDF}" "${META_FILE}" "${OUTPUT_PDF}"

# Verify metadata and outlines were applied correctly via PDF::API2 inspection
perl -e '
    use strict;
    use warnings;
    use utf8;
    use Encode;
    use PDF::API2;

    my $pdf = PDF::API2->open($ARGV[0]) or die "Cannot open $ARGV[0]\n";

    # Verify info dictionary
    my %expected_info = (
        Title    => "Test Document Title",
        Subject  => "Test Subject with utf8: äöü",
        Author   => "Test Author",
        Creator  => "Test Creator",
        Producer => "Test Producer",
        Keywords => "test, metadata, utf8, åäö",
    );

    my %info = $pdf->info();
    if (!defined $info{ModDate} || $info{ModDate} !~ /\AD:\d{14}/) {
        warn "FAIL: Missing or malformed ModDate in Info dictionary: " . ($info{ModDate} // "undef") . "\n";
        exit 1;
    }

    for my $key (keys %expected_info) {
        my $val = $info{$key};
        if (!defined $val) {
            warn "FAIL: Missing Info dictionary entry for $key\n";
            exit 1;
        }
        my $expected_latin1 = Encode::encode("iso-8859-1", $expected_info{$key});
        if ($val ne $expected_latin1) {
            warn "FAIL: Info entry $key mismatch. Expected \x27$expected_latin1\x27, got \x27$val\x27\n";
            exit 1;
        }
    }

    # Verify outlines structure (hierarchical walk over PDF outline dictionaries)
    sub walk_outlines {
        my ($dict) = @_;
        my @list;
        my $child = $dict->{First};
        while ($child) {
            $child = $child->realise;
            my $title_obj = $child->{Title};
            if (!$title_obj) {
                warn "FAIL: Outline item missing /Title entry\n";
                exit 1;
            }
            $title_obj = $title_obj->realise if ref($title_obj) && $title_obj->can("realise");
            my $title = ref($title_obj) && $title_obj->can("val") ? $title_obj->val : "$title_obj";

            if (!$child->{Dest}) {
                warn "FAIL: Outline item \x27$title\x27 missing /Dest entry\n";
                exit 1;
            }

            push @list, $title;
            my @sub = walk_outlines($child);
            push @list, \@sub if @sub;
            $child = $child->{Next};
        }
        return @list;
    }

    my $catalog = $pdf->{pdf}->{Root}->realise;
    if (!$catalog->{Outlines}) {
        warn "FAIL: PDF catalog missing /Outlines dictionary\n";
        exit 1;
    }
    my $root = $catalog->{Outlines}->realise;
    my @tree = walk_outlines($root);

    # Expected hierarchy:
    # [
    #   "Chapter 1",
    #   [ "Section 1.1", "Section 1.2", ["Subsection 1.2.1"] ],
    #   "Chapter 2"
    # ]
    if (@tree != 3 ||
        $tree[0] ne "Chapter 1" ||
        ref($tree[1]) ne "ARRAY" ||
        @{$tree[1]} != 3 ||
        $tree[1][0] ne "Section 1.1" ||
        $tree[1][1] ne "Section 1.2" ||
        ref($tree[1][2]) ne "ARRAY" ||
        $tree[1][2][0] ne "Subsection 1.2.1" ||
        $tree[2] ne "Chapter 2") {
        warn "FAIL: Outline tree structure or bookmark title mismatch.\n";
        exit 1;
    }

    exit 0;
' "${OUTPUT_PDF}"

# ====== 3. Space preservation in arguments ================================
SPACE_DIR="${TMP_DIR}/dir with spaces"
mkdir -p "${SPACE_DIR}"
SPACE_IN="${SPACE_DIR}/in file.pdf"
SPACE_META="${SPACE_DIR}/meta file.dat"
SPACE_OUT="${SPACE_DIR}/out file.pdf"

cp "${INPUT_PDF}" "${SPACE_IN}"
cp "${META_FILE}" "${SPACE_META}"

run_expect_ok "${TMP_DIR}/space.out" "${TMP_DIR}/space.err" "${SPACE_IN}" "${SPACE_META}" "${SPACE_OUT}"

if [ ! -s "${SPACE_OUT}" ]; then
    echo "FAIL: Failed to create output file with spaces in path." >&2
    exit 1
fi

# ====== 4. Negative / error handling tests ================================
# Missing / wrong argument counts
run_expect_fail "Usage:"
run_expect_fail "Usage:" "${INPUT_PDF}"
run_expect_fail "Usage:" "${INPUT_PDF}" "${META_FILE}"
run_expect_fail "Usage:" "${INPUT_PDF}" "${META_FILE}" "${OUTPUT_PDF}" extra

# Invalid options
run_expect_fail "Unknown option|Usage:" --non-existent-option
run_expect_fail "Unknown option|Usage:" -Z

# Missing input files
run_expect_fail "does not exist|ERROR: Unable to open PDF file" "${TMP_DIR}/missing.pdf" "${META_FILE}" "${TMP_DIR}/bad_out.pdf"
run_expect_fail "ERROR: Unable to open metadata file" "${INPUT_PDF}" "${TMP_DIR}/missing.meta" "${TMP_DIR}/bad_out.pdf"

echo "PASS: setpdfmetadata test passed."
exit 0
