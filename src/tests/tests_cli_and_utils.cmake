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

#############################################################################
#### BASIC CLI OPTIONS                                                   ####
#############################################################################

# ====== 1. Compiled Binaries ===============================================
ADD_TOOL_CLI_TESTS(netperfmeter)
ADD_TOOL_CLI_TESTS(combinesummaries)
ADD_TOOL_CLI_TESTS(createsummary)
ADD_TOOL_CLI_TESTS(extractvectors)
ADD_TOOL_CLI_TESTS(getabstime)
ADD_TOOL_CLI_TESTS(runtimeestimator)

# ====== 2. Companion Scripts (.versioned) ==================================
ADD_SCRIPT_CLI_TESTS(netperfmeter-module-loader)
ADD_SCRIPT_CLI_TESTS(pdfembedfonts)
ADD_SCRIPT_CLI_TESTS(plot-netperfmeter-results)
ADD_SCRIPT_CLI_TESTS(setpdfmetadata)


#############################################################################
#### NEGATIVE / ERROR HANDLING TESTS                                     ####
#############################################################################

# Invocation without arguments must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_no_args)

# Unknown or malformed options must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_option    --non-existent-option)
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_short_opt -U)

# Invalid control Traffic Class (-Q / --control-tc) and IPv6 Flow Label (-F / --control-fl)
ADD_NETPERFMETER_FAIL_TEST(fail_control_tc_missing_arg      9000 -Q)
ADD_NETPERFMETER_FAIL_TEST(fail_control_tc_invalid_name     9000 --control-tc INVALID_DSCP)
ADD_NETPERFMETER_FAIL_TEST(fail_control_tc_out_of_range     9000 -Q 256)
ADD_NETPERFMETER_FAIL_TEST(fail_control_tc_negative         9000 -Q -1)
ADD_NETPERFMETER_FAIL_TEST(fail_control_tc_invalid_hex      9000 --control-tc 0x100)
ADD_NETPERFMETER_FAIL_TEST(fail_control_fl_missing_arg      9000 -F)
ADD_NETPERFMETER_FAIL_TEST(fail_control_fl_invalid_name     9000 --control-fl bogus)
ADD_NETPERFMETER_FAIL_TEST(fail_control_fl_out_of_range     9000 -F 1048576)
ADD_NETPERFMETER_FAIL_TEST(fail_control_fl_hex_out_of_range 9000 --control-fl 0x100000)
ADD_NETPERFMETER_FAIL_TEST(fail_control_fl_negative         9000 -F -1)

# Active mode without any flow specification must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_active_no_flows 127.0.0.1:9000)

# Passive mode with all control channel protocols disabled must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_all_control_disabled 9000 -X -Y -Z)

# Out-of-range flow multiplier (-N / --count) must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_count_zero      127.0.0.1:9000 -N 0   -tcp default)
ADD_NETPERFMETER_FAIL_TEST(fail_count_too_large 127.0.0.1:9000 -N 257 -tcp default)

# Invalid local / control-local bind addresses must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_local_addr         9000 -L 999.999.999.999)
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_control_local_addr 9000 -l 999.999.999.999)

# Invalid remote endpoint / port must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_remote_port 127.0.0.1:0 -tcp default)
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_remote_host invalid..hostname.example:9000 -tcp default)

# Unwritable log file paths must exit with code 1
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_logfile_path   127.0.0.1:9000 --logfile /nonexistent_dir_xyz/test.log -tcp default)
ADD_NETPERFMETER_FAIL_TEST(fail_invalid_logappend_path 127.0.0.1:9000 --logappend /nonexistent_dir_xyz/test.log -tcp default)

# Invalid FLOWSPEC syntax, unknown options, probabilities, onoff=...,repeat, and duplicate IDs
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_syntax          127.0.0.1:9000 -tcp invalid_spec)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_unknown_option        127.0.0.1:9000 -tcp const10:const1000:const10:const1000:unknownopt=1)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_ordered       127.0.0.1:9000 -tcp const10:const1000:const10:const1000:ordered=1.5)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_reliable      127.0.0.1:9000 -tcp const10:const1000:const10:const1000:reliable=-0.1)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_onoff_repeat    127.0.0.1:9000 -tcp const10:const1000:const10:const1000:onoff=1,2,3,repeat)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_onoff_repeat_nonrel     127.0.0.1:9000 -tcp const10:const1000:const10:const1000:onoff=1,2,repeat)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_duplicate_id          127.0.0.1:9000 -tcp const10:const1000:const10:const1000:id=1 -tcp const10:const1000:const10:const1000:id=1)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_cmt             127.0.0.1:9000 -tcp const10:const1000:const10:const1000:cmt=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_ccid            127.0.0.1:9000 -tcp const10:const1000:const10:const1000:ccid=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_nodelay         127.0.0.1:9000 -tcp const10:const1000:const10:const1000:nodelay=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_debug           127.0.0.1:9000 -tcp const10:const1000:const10:const1000:debug=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_error_on_abort  127.0.0.1:9000 -tcp const10:const1000:const10:const1000:error_on_abort=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_cc_too_long             127.0.0.1:9000 -tcp const10:const1000:const10:const1000:cc=abcdefghijklmnopqrstuvwxyz0123456789)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_tc_name         127.0.0.1:9000 -tcp const10:const1000:const10:const1000:tc=INVALID)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_tc_range        127.0.0.1:9000 -tcp const10:const1000:const10:const1000:tc=256)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_tc_hex          127.0.0.1:9000 -tcp const10:const1000:const10:const1000:tc=0x100)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_fl_name         127.0.0.1:9000 -tcp const10:const1000:const10:const1000:fl=invalid)
ADD_NETPERFMETER_FAIL_TEST(fail_flowspec_invalid_fl_range        127.0.0.1:9000 -tcp const10:const1000:const10:const1000:fl=0x100000)

IF (HAVE_SCTP_FOR_TESTS)
   # Conflicting socket-level TC, Flow Label, or heartbeat settings across SCTP streams must exit with code 1
   ADD_NETPERFMETER_FAIL_TEST(fail_sctp_multistream_tc_mismatch 127.0.0.1:9000 -sctp default:tc=EF default:tc=AF21)
   ADD_NETPERFMETER_FAIL_TEST(fail_sctp_multistream_fl_mismatch [::1]:9000     -sctp default:fl=0x12345 default:fl=auto)
   ADD_NETPERFMETER_FAIL_TEST(fail_sctp_multistream_hb_mismatch 127.0.0.1:9000 -sctp default:hbinterval=5000 default:hbinterval=10000)
ENDIF()

IF (HAVE_QUIC_FOR_TESTS)
   # Mismatched/unreadable TLS files, unsupported QUIC CC, and conflicting QUIC stream settings must exit with code 1
   ADD_NETPERFMETER_FAIL_TEST(fail_tls_key_without_cert           9000 --tls-key nonexistent.key)
   ADD_NETPERFMETER_FAIL_TEST(fail_tls_cert_without_key           9000 --tls-cert nonexistent.crt)
   ADD_NETPERFMETER_FAIL_TEST(fail_tls_nonexistent_cert_and_key   9000 --tls-cert nonexistent_cert_xyz.crt --tls-key nonexistent_key_xyz.key)
   ADD_NETPERFMETER_FAIL_TEST(fail_tls_nonexistent_ca             9000 --tls-ca nonexistent_ca_xyz.crt)
   ADD_NETPERFMETER_FAIL_TEST(fail_tls_dir_as_ca                  9000 --tls-ca ${CMAKE_CURRENT_SOURCE_DIR})
   ADD_NETPERFMETER_FAIL_TEST(fail_quic_invalid_cc                127.0.0.1:9000 -quic default:cc=bbr)
   ADD_NETPERFMETER_FAIL_TEST(fail_quic_multistream_tc_mismatch   127.0.0.1:9000 -quic default:tc=EF default:tc=AF21)
   ADD_NETPERFMETER_FAIL_TEST(fail_quic_multistream_hb_mismatch   127.0.0.1:9000 -quic default:hbinterval=5000 default:hbinterval=0)
ENDIF()


#############################################################################
#### UTILITY TOOL TESTS                                                  ####
#############################################################################

IF(TARGET createsummary)
   ADD_TEST(
      NAME util_createsummary_summarize
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_createsummary.sh
         $<TARGET_FILE:createsummary>
   )
   SET_TESTS_PROPERTIES(util_createsummary_summarize PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;utility;createsummary"
      TIMEOUT 15
   )
ENDIF()

IF(TARGET combinesummaries)
   ADD_TEST(
      NAME util_combinesummaries_combine
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_combinesummaries.sh
         $<TARGET_FILE:combinesummaries>
   )
   SET_TESTS_PROPERTIES(util_combinesummaries_combine PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;utility;combinesummaries"
      TIMEOUT 15
   )
ENDIF()

IF(TARGET extractvectors)
   ADD_TEST(
      NAME util_extractvectors_extract
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_extractvectors.sh
         $<TARGET_FILE:extractvectors>
   )
   SET_TESTS_PROPERTIES(util_extractvectors_extract PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;utility;extractvectors"
      TIMEOUT 15
   )
ENDIF()

IF(TARGET getabstime)
   ADD_TEST(
      NAME util_getabstime_timestamp
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_getabstime.sh
         $<TARGET_FILE:getabstime>
   )
   SET_TESTS_PROPERTIES(util_getabstime_timestamp PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;utility;getabstime"
      TIMEOUT 15
   )
ENDIF()

IF(TARGET runtimeestimator)
   ADD_TEST(
      NAME util_runtimeestimator_estimate
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_runtimeestimator.sh
         $<TARGET_FILE:runtimeestimator>
   )
   SET_TESTS_PROPERTIES(util_runtimeestimator_estimate PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;utility;runtimeestimator"
      TIMEOUT 15
   )
ENDIF()

IF(WITH_PLOT_PROGRAMS)
   SET(PDFEMBEDFONTS_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/../pdfembedfonts.versioned")
   IF(NOT EXISTS "${PDFEMBEDFONTS_SCRIPT}")
      SET(PDFEMBEDFONTS_SCRIPT "${PROJECT_BINARY_DIR}/src/pdfembedfonts.versioned")
   ENDIF()

   ADD_TEST(
      NAME util_pdfembedfonts_embed
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_pdfembedfonts.sh
         "${PDFEMBEDFONTS_SCRIPT}"
   )
   SET_TESTS_PROPERTIES(util_pdfembedfonts_embed PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;script;utility;pdfembedfonts"
      TIMEOUT 60
   )

   SET(SETPDFMETADATA_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/../setpdfmetadata.versioned")
   IF(NOT EXISTS "${SETPDFMETADATA_SCRIPT}")
      SET(SETPDFMETADATA_SCRIPT "${PROJECT_BINARY_DIR}/src/setpdfmetadata.versioned")
   ENDIF()

   ADD_TEST(
      NAME util_setpdfmetadata_update
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_setpdfmetadata.sh
         "${SETPDFMETADATA_SCRIPT}"
   )
   SET_TESTS_PROPERTIES(util_setpdfmetadata_update PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;script;utility;setpdfmetadata"
      TIMEOUT 15
   )
ENDIF()


IF(TARGET combinesummaries AND TARGET netperfmeter)
   GET_NEXT_TEST_PORT(PLOT_TEST_PORT)
   SET(PLOT_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/../plot-netperfmeter-results.versioned")
   IF(NOT EXISTS "${PLOT_SCRIPT}")
      SET(PLOT_SCRIPT "${PROJECT_BINARY_DIR}/src/plot-netperfmeter-results.versioned")
   ENDIF()

   ADD_TEST(
      NAME util_plot_netperfmeter_results
      COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/test_plot_netperfmeter_results.sh
         "${PLOT_SCRIPT}"
         $<TARGET_FILE:combinesummaries>
         "${CMAKE_CURRENT_SOURCE_DIR}/.."
         $<TARGET_FILE:netperfmeter>
         ${PLOT_TEST_PORT}
   )
   SET_TESTS_PROPERTIES(util_plot_netperfmeter_results PROPERTIES
      ENVIRONMENT "LC_ALL=C.UTF-8;TEXTDOMAINDIR=${TEST_LOCALE_DIR}"
      FAIL_REGULAR_EXPRESSION "INTERNAL ERROR;ERROR:;FAIL:"
      LABELS "cli;script;utility;plot-netperfmeter-results"
      TIMEOUT 300
   )
ENDIF()
