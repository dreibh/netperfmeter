/* packet-npmp.c
 * Routines for the NetPerfMeter Protocol used by the Open Source
 * network performance meter application NetPerfMeter:
 * http://www.exp-math.uni-essen.de/~dreibh/netperfmeter/
 *
 * Copyright 2009 by Thomas Dreibholz <dreibh [AT] iem.uni-due.de>
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * Copied from README.developer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <epan/packet.h>


#define PPID_NETPERFMETER_CONTROL   0x29097605
#define PPID_NETPERFMETER_DATA      0x29097606

// struct NetPerfMeterDataMessage
// {
//    NetPerfMeterHeader Header;
// 
//    uint32_t           FlowID;
//    uint64_t           MeasurementID;
//    uint16_t           StreamID;
//    uint16_t           Padding;
//    
//    uint32_t           FrameID;
//    uint64_t           SeqNumber;
//    uint64_t           ByteSeqNumber;
//    uint64_t           TimeStamp;
// 
//    char               Payload[];
// } __attribute__((packed));
// 


/* Initialize the protocol and registered fields */
#define INIT_FIELD(variable, offset, length) \
   static int hf_##variable           = -1;        \
   static const int offset_##variable = offset;    \
   static const int length_##variable = length;
   
INIT_FIELD(message_type,   0, 1)
INIT_FIELD(message_flags,  1, 1)
INIT_FIELD(message_length, 2, 2)

#define NETPERFMETER_ACKNOWLEDGE    0x01
#define NETPERFMETER_ADD_FLOW       0x02
#define NETPERFMETER_REMOVE_FLOW    0x03
#define NETPERFMETER_IDENTIFY_FLOW  0x04
#define NETPERFMETER_DATA           0x05
#define NETPERFMETER_START          0x06
#define NETPERFMETER_STOP           0x07
#define NETPERFMETER_RESULTS        0x08

static const value_string message_type_values[] = {
  { FRACTALGENERATOR_PARAMETER_MESSAGE_TYPE,        "FractalGenerator Parameter" },
  { FRACTALGENERATOR_DATA_MESSAGE_TYPE,             "FractalGenerator Data" },
  { 0, NULL }
};


INIT_FIELD(addflow_flowid,          4,  4)
INIT_FIELD(addflow_measurementid,   8,  8)
INIT_FIELD(addflow_streamid,       16,  2)



// INIT_FIELD(data_flowid,        4, sizeof(uint32_t))
// INIT_FIELD(data_measurementid, 8, sizeof(uint32_t))

static int proto_npmp = -1;
// static int hf_message_type                = -1;
// static int hf_message_flags               = -1;
// static int hf_message_length              = -1;
static int hf_data_start_x                = -1;
static int hf_data_start_y                = -1;
static int hf_data_points                 = -1;
static int hf_parameter_width             = -1;
static int hf_parameter_height            = -1;
static int hf_parameter_maxiterations     = -1;
static int hf_parameter_algorithmid       = -1;
static int hf_parameter_c1real            = -1;
static int hf_parameter_c1imag            = -1;
static int hf_parameter_c2real            = -1;
static int hf_parameter_c2imag            = -1;
static int hf_parameter_n                 = -1;
static int hf_buffer                      = -1;



  /* Setup list of header fields */
  static hf_register_info hf[] = {
    { &hf_message_type,            { "Type",          "npmp.message_type",            FT_UINT8,  BASE_DEC, VALS(message_type_values), 0x0, NULL, HFILL } },
    { &hf_message_flags,           { "Flags",         "npmp.message_flags",           FT_UINT8,  BASE_DEC, NULL,                      0x0, NULL, HFILL } },
    { &hf_message_length,          { "Length",        "npmp.message_length",          FT_UINT16, BASE_DEC, NULL,                      0x0, NULL, HFILL } },
    { &hf_parameter_algorithmid,   { "AlgorithmID",   "npmp.parameter_algorithmid",   FT_UINT32, BASE_DEC, NULL,                      0x0, NULL, HFILL } },
    { &hf_parameter_n,             { "N",             "npmp.parameter_n",             FT_DOUBLE, BASE_NONE, NULL,                      0x0, NULL, HFILL } },
    { &hf_buffer,                  { "Buffer",        "npmp.buffer",                  FT_BYTES,  BASE_NONE, NULL,                      0x0, NULL, HFILL } },
  };





/* Initialize the subtree pointers */
static gint ett_npmp = -1;

static void
dissect_npmp_message(tvbuff_t *, packet_info *, proto_tree *);


/* Dissectors for messages. This is specific to NetPerfMeterProtocol */
#define MESSAGE_TYPE_LENGTH    1
#define MESSAGE_FLAGS_LENGTH   1
#define MESSAGE_LENGTH_LENGTH  2

#define MESSAGE_TYPE_OFFSET    0
#define MESSAGE_FLAGS_OFFSET   (MESSAGE_TYPE_OFFSET    + MESSAGE_TYPE_LENGTH)
#define MESSAGE_LENGTH_OFFSET  (MESSAGE_FLAGS_OFFSET   + MESSAGE_FLAGS_OFFSET)
#define MESSAGE_VALUE_OFFSET   (MESSAGE_LENGTH_OFFSET  + MESSAGE_LENGTH_LENGTH)


#define DATA_STARTX_LENGTH 4
#define DATA_STARTY_LENGTH 4
#define DATA_POINTS_LENGTH 4
#define DATA_BUFFER_LENGTH 4

#define DATA_STARTX_OFFSET MESSAGE_VALUE_OFFSET
#define DATA_STARTY_OFFSET (DATA_STARTX_OFFSET + DATA_STARTX_LENGTH)
#define DATA_POINTS_OFFSET (DATA_STARTY_OFFSET + DATA_STARTY_LENGTH)
#define DATA_BUFFER_OFFSET (DATA_POINTS_OFFSET + DATA_POINTS_LENGTH)


#define PARAMETER_WIDTH_LENGTH         4
#define PARAMETER_HEIGHT_LENGTH        4
#define PARAMETER_MAXITERATIONS_LENGTH 4
#define PARAMETER_ALGORITHMID_LENGTH   4
#define PARAMETER_C1REAL_LENGTH        8
#define PARAMETER_C1IMAG_LENGTH        8
#define PARAMETER_C2REAL_LENGTH        8
#define PARAMETER_C2IMAG_LENGTH        8
#define PARAMETER_N_LENGTH             8

#define PARAMETER_WIDTH_OFFSET         MESSAGE_VALUE_OFFSET
#define PARAMETER_HEIGHT_OFFSET        (PARAMETER_WIDTH_OFFSET + PARAMETER_WIDTH_LENGTH)
#define PARAMETER_MAXITERATIONS_OFFSET (PARAMETER_HEIGHT_OFFSET + PARAMETER_HEIGHT_LENGTH)
#define PARAMETER_ALGORITHMID_OFFSET   (PARAMETER_MAXITERATIONS_OFFSET + PARAMETER_MAXITERATIONS_LENGTH)
#define PARAMETER_C1REAL_OFFSET        (PARAMETER_ALGORITHMID_OFFSET + PARAMETER_ALGORITHMID_LENGTH)
#define PARAMETER_C1IMAG_OFFSET        (PARAMETER_C1REAL_OFFSET + PARAMETER_C1REAL_LENGTH)
#define PARAMETER_C2REAL_OFFSET        (PARAMETER_C1IMAG_OFFSET + PARAMETER_C1IMAG_LENGTH)
#define PARAMETER_C2IMAG_OFFSET        (PARAMETER_C2REAL_OFFSET + PARAMETER_C2REAL_LENGTH)
#define PARAMETER_N_OFFSET             (PARAMETER_C2IMAG_OFFSET + PARAMETER_C2IMAG_LENGTH)



static void
dissect_npmp_parameter_message(tvbuff_t *message_tvb, proto_tree *message_tree)
{
  proto_tree_add_item(message_tree, hf_parameter_width,         message_tvb, PARAMETER_WIDTH_OFFSET,         PARAMETER_WIDTH_LENGTH,         FALSE);
  proto_tree_add_item(message_tree, hf_parameter_height,        message_tvb, PARAMETER_HEIGHT_OFFSET,        PARAMETER_HEIGHT_LENGTH,        FALSE);
  proto_tree_add_item(message_tree, hf_parameter_maxiterations, message_tvb, PARAMETER_MAXITERATIONS_OFFSET, PARAMETER_MAXITERATIONS_LENGTH, FALSE);
  proto_tree_add_item(message_tree, hf_parameter_algorithmid,   message_tvb, PARAMETER_ALGORITHMID_OFFSET,   PARAMETER_ALGORITHMID_LENGTH,   FALSE);
  proto_tree_add_item(message_tree, hf_parameter_c1real,        message_tvb, PARAMETER_C1REAL_OFFSET,        PARAMETER_C1REAL_LENGTH,        FALSE);
  proto_tree_add_item(message_tree, hf_parameter_c1imag,        message_tvb, PARAMETER_C1IMAG_OFFSET,        PARAMETER_C1IMAG_LENGTH,        FALSE);
  proto_tree_add_item(message_tree, hf_parameter_c2real,        message_tvb, PARAMETER_C2REAL_OFFSET,        PARAMETER_C2REAL_LENGTH,        FALSE);
  proto_tree_add_item(message_tree, hf_parameter_c2imag,        message_tvb, PARAMETER_C2IMAG_OFFSET,        PARAMETER_C2IMAG_LENGTH,        FALSE);
  proto_tree_add_item(message_tree, hf_parameter_n,             message_tvb, PARAMETER_N_OFFSET,             PARAMETER_N_LENGTH,             FALSE);
}


static void
dissect_npmp_data_message(tvbuff_t *message_tvb, proto_tree *message_tree)
{
  guint16 buffer_length;

  proto_tree_add_item(message_tree, hf_data_start_x, message_tvb, DATA_STARTX_OFFSET, DATA_STARTX_LENGTH, FALSE);
  proto_tree_add_item(message_tree, hf_data_start_y, message_tvb, DATA_STARTY_OFFSET, DATA_STARTY_LENGTH, FALSE);
  proto_tree_add_item(message_tree, hf_data_points,  message_tvb, DATA_POINTS_OFFSET, DATA_POINTS_LENGTH, FALSE);

  buffer_length = tvb_get_ntohl(message_tvb, DATA_POINTS_OFFSET)*4;
  if (buffer_length > 0) {
    proto_tree_add_item(message_tree, hf_buffer, message_tvb, DATA_BUFFER_OFFSET, buffer_length, FALSE);
  }
}


static void
dissect_npmp_message(tvbuff_t *message_tvb, packet_info *pinfo, proto_tree *npmp_tree)
{
  guint8 type;

  type = tvb_get_guint8(message_tvb, MESSAGE_TYPE_OFFSET);
  if (pinfo && (check_col(pinfo->cinfo, COL_INFO))) {
    col_add_fstr(pinfo->cinfo, COL_INFO, "%s ", val_to_str(type, message_type_values, "Unknown NetPerfMeterProtocol type"));
  }
  proto_tree_add_item(npmp_tree, hf_message_type,   message_tvb, MESSAGE_TYPE_OFFSET,   MESSAGE_TYPE_LENGTH,   FALSE);
  proto_tree_add_item(npmp_tree, hf_message_flags,  message_tvb, MESSAGE_FLAGS_OFFSET,  MESSAGE_FLAGS_LENGTH,  FALSE);
  proto_tree_add_item(npmp_tree, hf_message_length, message_tvb, MESSAGE_LENGTH_OFFSET, MESSAGE_LENGTH_LENGTH, FALSE);
  switch (type) {
    case FRACTALGENERATOR_PARAMETER_MESSAGE_TYPE:
      dissect_npmp_parameter_message(message_tvb, npmp_tree);
     break;
    case FRACTALGENERATOR_DATA_MESSAGE_TYPE:
      dissect_npmp_data_message(message_tvb, npmp_tree);
     break;
  }
}


static int
dissect_npmp(tvbuff_t *message_tvb, packet_info *pinfo, proto_tree *tree)
{
  proto_item *npmp_item;
  proto_tree *npmp_tree;

  /* pinfo is NULL only if dissect_npmp_message is called from dissect_error cause */
  if (pinfo && (check_col(pinfo->cinfo, COL_PROTOCOL)))
    col_set_str(pinfo->cinfo, COL_PROTOCOL, "NetPerfMeterProtocol");

  /* In the interest of speed, if "tree" is NULL, don't do any work not
     necessary to generate protocol tree items. */
  if (tree) {
    /* create the npmp protocol tree */
    npmp_item = proto_tree_add_item(tree, proto_npmp, message_tvb, 0, -1, FALSE);
    npmp_tree = proto_item_add_subtree(npmp_item, ett_npmp);
  } else {
    npmp_tree = NULL;
  };
  /* dissect the message */
  dissect_npmp_message(message_tvb, pinfo, npmp_tree);
  return(TRUE);
}


/* Register the protocol with Wireshark */
void
proto_register_npmp(void)
{
  /* Setup protocol subtree array */
  static gint *ett[] = {
    &ett_npmp
  };

  /* Register the protocol name and description */
  proto_npmp = proto_register_protocol("NetPerfMeter Protocol", "NetPerfMeterProtocol",  "npmp");

  /* Required function calls to register the header fields and subtrees used */
  proto_register_field_array(proto_npmp, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));
}


void
proto_reg_handoff_npmp(void)
{
  dissector_handle_t npmp_handle;

  npmp_handle = new_create_dissector_handle(dissect_npmp, proto_npmp);
  dissector_add("sctp.ppi", PPID_NETPERFMETER_CONTROL, npmp_handle);
}
