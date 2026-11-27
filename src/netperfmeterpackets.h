/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2026 by Thomas Dreibholz
 * ==========================================================================
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#ifndef NETPERFMETERPACKETS_H
#define NETPERFMETERPACKETS_H

#include "tools.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#if defined(__GNUC__) || defined(__clang__)
#define NPM_PACKED [[gnu::packed]]
#elif defined(_MSC_VER)
#define NPM_PACKED
#pragma pack(push, 1)
#else
#define NPM_PACKED
#endif

// ====== Protocol IDs and ALPN Identifiers =================================
inline constexpr uint32_t PPID_NETPERFMETER_CONTROL = 36;          /* Old value: 0x29097605 */
inline constexpr uint32_t PPID_NETPERFMETER_DATA    = 37;          /* Old value: 0x29097606 */
inline constexpr uint32_t SC_NETPERFMETER_DATA      = 1852861808;  /* Old value: 0x29097606 */

inline constexpr std::string_view ALPN_NETPERFMETER_CONTROL_VIEW = "netperfmeter/control";
inline constexpr std::string_view ALPN_NETPERFMETER_DATA_VIEW    = "netperfmeter/data";

#define ALPN_NETPERFMETER_CONTROL   "netperfmeter/control"
#define ALPN_NETPERFMETER_DATA      "netperfmeter/data"

// ====== Message Types =====================================================
enum class NetPerfMeterMessageType : uint8_t
{
   Acknowledge  = 0x01,
   AddFlow      = 0x02,
   RemoveFlow   = 0x03,
   IdentifyFlow = 0x04,
   Data         = 0x05,
   Start        = 0x06,
   Stop         = 0x07,
   Results      = 0x08
};

inline constexpr uint8_t NETPERFMETER_ACKNOWLEDGE   = static_cast<uint8_t>(NetPerfMeterMessageType::Acknowledge);
inline constexpr uint8_t NETPERFMETER_ADD_FLOW      = static_cast<uint8_t>(NetPerfMeterMessageType::AddFlow);
inline constexpr uint8_t NETPERFMETER_REMOVE_FLOW   = static_cast<uint8_t>(NetPerfMeterMessageType::RemoveFlow);
inline constexpr uint8_t NETPERFMETER_IDENTIFY_FLOW = static_cast<uint8_t>(NetPerfMeterMessageType::IdentifyFlow);
inline constexpr uint8_t NETPERFMETER_DATA          = static_cast<uint8_t>(NetPerfMeterMessageType::Data);
inline constexpr uint8_t NETPERFMETER_START         = static_cast<uint8_t>(NetPerfMeterMessageType::Start);
inline constexpr uint8_t NETPERFMETER_STOP          = static_cast<uint8_t>(NetPerfMeterMessageType::Stop);
inline constexpr uint8_t NETPERFMETER_RESULTS       = static_cast<uint8_t>(NetPerfMeterMessageType::Results);


// ====== Common Message Header =============================================
struct NPM_PACKED NetPerfMeterHeader
{
   uint8_t  Type;
   uint8_t  Flags;
   uint16_t Length;
};


// ====== Acknowledge Message ===============================================
enum class NetPerfMeterStatus : uint32_t
{
   Okay  = 0,
   Error = 1
};

inline constexpr uint32_t NETPERFMETER_STATUS_OKAY  = static_cast<uint32_t>(NetPerfMeterStatus::Okay);
inline constexpr uint32_t NETPERFMETER_STATUS_ERROR = static_cast<uint32_t>(NetPerfMeterStatus::Error);

struct NPM_PACKED NetPerfMeterAcknowledgeMessage
{
   NetPerfMeterHeader Header;

   uint32_t           FlowID;
   uint64_t           MeasurementID;
   uint16_t           StreamID;
   uint16_t           Padding;

   uint32_t           Status;
};


// ====== On/Off Pattern Event ==============================================
inline constexpr size_t NETPERFMETER_RNG_INPUT_PARAMETERS = 4;
inline constexpr uint8_t NPOOEF_RELTIME                   = (1 << 0);

struct NPM_PACKED NetPerfMeterOnOffEvent
{
   uint8_t          RandNumGen;
   uint8_t          Flags;
   uint16_t         pad;
   network_double_t ValueArray[NETPERFMETER_RNG_INPUT_PARAMETERS];
};


// ====== Add Flow Message ==================================================
inline constexpr size_t NETPERFMETER_DESCRIPTION_SIZE = 32;
inline constexpr size_t NETPERFMETER_CC_LENGTH        = 16;

inline constexpr uint8_t NPMAFF_DEBUG       = (1 << 0);
inline constexpr uint8_t NPMAFF_NODELAY     = (1 << 1);
inline constexpr uint8_t NPMAFF_REPEATONOFF = (1 << 2);

// RetransmissionTrials in milliseconds (highest bit of 32-bit value set)
inline constexpr uint32_t NPMAF_RTX_TRIALS_IN_MILLISECONDS = (1U << 31);   // Use ms instead of number of trials
inline constexpr uint32_t NPMAF_RTX_DEFAULT                = 0x7fffffffU;  // Use protocol default

// TrafficClassSpec wire encoding (big-endian uint16_t):
// - 0x0000: unset (use OS default)
// - (val & 0xff00) == 0xff00: explicit Traffic Class / TOS in (val & 0x00fc),
//   leaving the lower 2 bits zero so ECN remains controlled by the transport stack.
inline constexpr uint16_t NPMAF_TC_NOT_SET    = 0x0000U;
inline constexpr uint16_t NPMAF_TC_VALID_MASK = 0xff00U;
inline constexpr uint16_t NPMAF_TC_VALUE_MASK = 0x00fcU;

// FlowLabelSpec wire encoding (big-endian uint32_t):
// - 0x00000000: unset (use OS default)
// - Bit 31 (NPMAF_FL_VALID) must be set when present (avoids collision with
//   7-bit ASCII legacy PathMgr strings):
//   - NPMAF_FL_VALID | (label & 0x000fffff): explicit 20-bit IPv6 Flow Label
//   - NPMAF_FL_VALID | NPMAF_FL_AUTO:        enable automatic IPv6 Flow Label
//   - NPMAF_FL_VALID | NPMAF_FL_REFLECT:     request Flow Label reflection
inline constexpr uint32_t NPMAF_FL_NOT_SET    = 0x00000000U;
inline constexpr uint32_t NPMAF_FL_VALID      = (1U << 31);
inline constexpr uint32_t NPMAF_FL_AUTO       = (1U << 30);
inline constexpr uint32_t NPMAF_FL_REFLECT    = (1U << 29);
inline constexpr uint32_t NPMAF_FL_VALUE_MASK = 0x000fffffU;

// Per-flow heartbeat / keep-alive wire encoding (replaces obsolete Scheduler[16]):
// - HBMagic == 0xff indicates the HB* fields are valid (avoids collision with
//   7-bit ASCII legacy Scheduler strings).
// - HBFlags bitmask selects which parameters are explicitly configured.
inline constexpr uint8_t NPMAF_HB_MAGIC        = 0xffU;
inline constexpr uint8_t NPMAF_HBF_INTERVAL    = (1U << 0);
inline constexpr uint8_t NPMAF_HBF_PATHMAXRTX  = (1U << 1);
inline constexpr uint8_t NPMAF_HBF_ASSOCMAXRTX = (1U << 2);

inline constexpr uint8_t NPAF_PRIMARY_PATH = 0x00;
inline constexpr uint8_t NPAF_CMT          = 0x01;
inline constexpr uint8_t NPAF_CMTRPv1      = 0x02;
inline constexpr uint8_t NPAF_CMTRPv2      = 0x03;
inline constexpr uint8_t NPAF_LikeMPTCP    = 0x04;

struct NPM_PACKED NetPerfMeterAddFlowMessage
{
   NetPerfMeterHeader     Header;

   uint32_t               FlowID;
   uint64_t               MeasurementID;
   uint16_t               StreamID;
   uint16_t               Protocol;   // Little Endian, due to compatibility!

   char                   Description[NETPERFMETER_DESCRIPTION_SIZE];

   uint32_t               OrderedMode;
   uint32_t               ReliableMode;
   uint32_t               RetransmissionTrials;

   network_double_t       FrameRate[NETPERFMETER_RNG_INPUT_PARAMETERS];
   network_double_t       FrameSize[NETPERFMETER_RNG_INPUT_PARAMETERS];
   uint8_t                FrameRateRng;
   uint8_t                FrameSizeRng;

   uint32_t               RcvBufferSize;
   uint32_t               SndBufferSize;

   uint16_t               MaxMsgSize;
   uint8_t                CMT;
   uint8_t                CCID;

   uint16_t               TrafficClassSpec;
   uint32_t               FlowLabelSpec;
   uint8_t                Padding1[12];
   char                   CongestionControl[NETPERFMETER_CC_LENGTH];
   uint8_t                HBMagic;
   uint8_t                HBFlags;
   uint16_t               Padding2;
   uint32_t               HBInterval;
   uint32_t               HBPathMaxRtx;
   uint32_t               HBAssocMaxRtx;

   uint16_t               OnOffEvents;
   NetPerfMeterOnOffEvent OnOffEvent[];
};


// ====== Remove Flow Message ===============================================
struct NPM_PACKED NetPerfMeterRemoveFlowMessage
{
   NetPerfMeterHeader Header;

   uint32_t           FlowID;
   uint64_t           MeasurementID;
   uint16_t           StreamID;
};


// ====== Identify Message ==================================================
inline constexpr uint64_t NETPERFMETER_IDENTIFY_FLOW_MAGIC_NUMBER = 0x4bcdf3aa303c6774ULL;

inline constexpr uint8_t NPMIF_COMPRESS_VECTORS = (1 << 0);
inline constexpr uint8_t NPMIF_NO_VECTORS       = (1 << 1);

struct NPM_PACKED NetPerfMeterIdentifyMessage
{
   NetPerfMeterHeader Header;
   uint32_t           FlowID;
   uint64_t           MagicNumber;
   uint64_t           MeasurementID;
   uint16_t           StreamID;
};


// ====== Data Message ======================================================
inline constexpr uint8_t NPMDF_FRAME_BEGIN = (1 << 0);
inline constexpr uint8_t NPMDF_FRAME_END   = (1 << 1);

struct NPM_PACKED NetPerfMeterDataMessage
{
   NetPerfMeterHeader Header;

   uint32_t           FlowID;
   uint64_t           MeasurementID;
   uint16_t           StreamID;
   uint16_t           Padding;

   uint32_t           FrameID;
   uint64_t           SeqNumber;
   uint64_t           ByteSeqNumber;
   uint64_t           TimeStamp;

   uint8_t            Payload[];
};


// ====== Start Message =====================================================
inline constexpr uint8_t NPMSF_COMPRESS_VECTORS = (1 << 0);
inline constexpr uint8_t NPMSF_COMPRESS_SCALARS = (1 << 1);
inline constexpr uint8_t NPMSF_NO_VECTORS       = (1 << 2);
inline constexpr uint8_t NPMSF_NO_SCALARS       = (1 << 3);

struct NPM_PACKED NetPerfMeterStartMessage
{
   NetPerfMeterHeader Header;

   uint8_t            VectorFormat;
   uint8_t            ScalarFormat;
   uint16_t           Padding;
   uint64_t           MeasurementID;
};


// ====== Stop Message ======================================================
struct NPM_PACKED NetPerfMeterStopMessage
{
   NetPerfMeterHeader Header;

   uint32_t           Padding;
   uint64_t           MeasurementID;
};


// ====== Results Message ===================================================
inline constexpr uint8_t NPMRF_EOF                            = (1 << 0);
inline constexpr size_t  NETPERFMETER_RESULTS_MAX_DATA_LENGTH = 1400;

struct NPM_PACKED NetPerfMeterResults
{
   NetPerfMeterHeader Header;
   char               Data[];
};

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#undef NPM_PACKED


// ====== Static Assertions for Protocol Wire Sizes =========================
static_assert(std::is_standard_layout_v<NetPerfMeterHeader>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterHeader>);
static_assert(sizeof(NetPerfMeterHeader) == 4);

static_assert(std::is_standard_layout_v<NetPerfMeterAcknowledgeMessage>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterAcknowledgeMessage>);
static_assert(sizeof(NetPerfMeterAcknowledgeMessage) == 24);

static_assert(std::is_standard_layout_v<NetPerfMeterOnOffEvent>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterOnOffEvent>);
static_assert(sizeof(NetPerfMeterOnOffEvent) == 36);

static_assert(std::is_standard_layout_v<NetPerfMeterAddFlowMessage>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterAddFlowMessage>);
static_assert(sizeof(NetPerfMeterAddFlowMessage) == 194);

static_assert(std::is_standard_layout_v<NetPerfMeterRemoveFlowMessage>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterRemoveFlowMessage>);
static_assert(sizeof(NetPerfMeterRemoveFlowMessage) == 18);

static_assert(std::is_standard_layout_v<NetPerfMeterIdentifyMessage>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterIdentifyMessage>);
static_assert(sizeof(NetPerfMeterIdentifyMessage) == 26);

static_assert(std::is_standard_layout_v<NetPerfMeterDataMessage>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterDataMessage>);
static_assert(sizeof(NetPerfMeterDataMessage) == 48);

static_assert(std::is_standard_layout_v<NetPerfMeterStartMessage>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterStartMessage>);
static_assert(sizeof(NetPerfMeterStartMessage) == 16);

static_assert(std::is_standard_layout_v<NetPerfMeterStopMessage>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterStopMessage>);
static_assert(sizeof(NetPerfMeterStopMessage) == 16);

static_assert(std::is_standard_layout_v<NetPerfMeterResults>);
static_assert(std::is_trivially_copyable_v<NetPerfMeterResults>);
static_assert(sizeof(NetPerfMeterResults) == 4);

#endif
