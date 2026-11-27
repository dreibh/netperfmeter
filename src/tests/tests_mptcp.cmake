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
#### MPTCP FLOW INTEGRATION TESTS (RFC 8684)                             ####
#############################################################################

IF (HAVE_MPTCP_FOR_TESTS)

# ====== 1. Bidirectional Flows =============================================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_bidi_ipv4 127.0.0.1 mptcp const10:const1000:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_bidi_ipv6 ::1       mptcp const10:const1000:const10:const1000 plain)

# ====== 2. Upload-Only Flows (Active -> Passive) ===========================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_upload_ipv4 127.0.0.1 mptcp const10:const1000:const0:const0 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_upload_ipv6 ::1       mptcp const10:const1000:const0:const0 plain)

# ====== 3. Download-Only Flows (Passive -> Active) =========================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_download_ipv4 127.0.0.1 mptcp const0:const0:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_download_ipv6 ::1       mptcp const0:const0:const10:const1000 plain)

# ====== 4. Default Shorthand, Control Channel Over MPTCP & Local Bind ======
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_default_shorthand
   127.0.0.1 mptcp default plain 2)
IF (HAVE_TCP_CONGESTION)
   ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_cc_cubic
      127.0.0.1 mptcp const10:const1000:const10:const1000:cc=cubic plain 2)
ENDIF()
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_local_bind_ipv4
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2
   -L 127.0.0.1 -l 127.0.0.1
   --server-arg -L --server-arg 127.0.0.1 --server-arg -l --server-arg 127.0.0.1)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_local_bind_ipv6
   ::1 mptcp const10:const1000:const10:const1000 plain 2
   -L ::1 -l ::1
   --server-arg -L --server-arg ::1 --server-arg -l --server-arg ::1)

# Control channel using MPTCP with MPTCP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_ctrl_mptcp_bidi_ipv4
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2 --control-over-mptcp)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_ctrl_mptcp_bidi_ipv6
   ::1       mptcp const10:const1000:const10:const1000 plain 2 --control-over-mptcp)

# Control channel using MPTCP with TCP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_tcp_ctrl_mptcp_bidi_ipv4
   127.0.0.1 tcp   const10:const1000:const10:const1000 plain 2 --control-over-mptcp)

# Control channel using MPTCP with UDP data flow
ADD_NETPERFMETER_FLOW_TEST(flow_udp_ctrl_mptcp_bidi_ipv4
   127.0.0.1 udp   const10:const1000:const10:const1000 plain 2 --control-over-mptcp)

# Backwards compatibility alias: -tcp ...:cmt=mptcp creates an MPTCP connection
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_cmt_alias_ipv4
   127.0.0.1 tcp   const10:const1000:const10:const1000:cmt=mptcp plain 2)

# Traffic Class, IPv6 Flow Label, and per-flow keep-alive (enabled and disabled)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_tc_and_keepalive_ipv4
   127.0.0.1 mptcp const10:const1000:const10:const1000:tc=AF21:hbinterval=5000:hbpathmaxrtx=3 plain 2
   --expect-config "FLOW0_TRAFFIC_CLASS=\"0x48\"")
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_tc_fl_keepalive_disabled_ipv6
   ::1       mptcp const10:const1000:const10:const1000:tc=CS4:fl=auto:hbinterval=0 plain 2
   --expect-config "FLOW0_HBINTERVAL=\"0\"")

# ====== 5. Compression Formats & Output Suppression ========================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_format_bz2 127.0.0.1 mptcp const10:const1000:const10:const1000 bz2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_no_scalar  127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2 --no-scalar)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_no_vector  127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2 --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_no_outputs 127.0.0.1 mptcp const10:const1000:const10:const1000 plain 2 --no-scalar --no-vector)

# ====== 6. Framing and Defragmentation (maxmsgsize & frame sizes) ==========
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_framing_small      127.0.0.1 mptcp const10:const1000:const10:const1000:maxmsgsize=250  plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_framing_medium     127.0.0.1 mptcp const10:const4000:const10:const4000:maxmsgsize=1000 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_framing_large      127.0.0.1 mptcp const10:const8000:const10:const8000:maxmsgsize=1400 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_framing_asymmetric 127.0.0.1 mptcp const10:const4000:const10:const1000:maxmsgsize=500  plain 2)

# ====== 7. On-Off Flow Schedule Tests ======================================
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_single_burst_ipv4
   127.0.0.1 mptcp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_single_burst_ipv6
   ::1       mptcp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_multiburst_ipv4
   127.0.0.1 mptcp const20:const1000:const0:const0:onoff=1,2,3,4 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_delayed_start
   127.0.0.1 mptcp const20:const1000:const0:const0:onoff=2 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_fragmented
   127.0.0.1 mptcp const20:const3000:const0:const0:onoff=1,3:maxmsgsize=1000 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_mptcp_onoff_exp_size
   127.0.0.1 mptcp const20:exp1000:const0:const0:onoff=1,4 plain 6)

# ====== 8. Random Distribution Tests (VBR Flows) ===========================
ADD_NETPERFMETER_FLOW_TEST(dist_mptcp_uniform     127.0.0.1 mptcp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_mptcp_exp         127.0.0.1 mptcp const20:exp1000:const20:exp1000 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_mptcp_truncnormal 127.0.0.1 mptcp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_mptcp_pareto      127.0.0.1 mptcp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)

# ====== 9. MPTCP Multi-Connection & Cross-Protocol Concurrency =============
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_2connections_ipv4
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -mptcp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_4connections_ipv4
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 5
   -mptcp const10:const1000:const10:const1000
   -mptcp const10:const1000:const10:const1000
   -mptcp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_2connections_ipv6
   ::1       mptcp const10:const1000:const10:const1000 plain 3
   -mptcp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_4connections_ipv6
   ::1       mptcp const10:const1000:const10:const1000 plain 5
   -mptcp const10:const1000:const10:const1000
   -mptcp const10:const1000:const10:const1000
   -mptcp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_heterogeneous_rates_sizes
   127.0.0.1 mptcp const25:const100:const25:const100:nodelay=on plain 5
   -mptcp const10:const1400:const10:const1400)

# Asymmetric split: Flow 0 upload-only MPTCP, Flow 1 download-only MPTCP
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_split_upload_download
   127.0.0.1 mptcp const15:const1000:const0:const0 plain 3
   -mptcp const0:const0:const15:const1000)

# Interleaved On-Off bursts across 2 MPTCP connections
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_interleaved_onoff
   127.0.0.1 mptcp const20:const1000:const0:const0:onoff=1,2 plain 4
   -mptcp const20:const1000:const0:const0:onoff=2,3)

# Simultaneous MPTCP (port-1) and regular TCP (port) connections
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_tcp_cross_traffic
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000)

# Competing MPTCP + UDP flows
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_udp_cross_traffic
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -udp const10:const1000:const10:const1000)

# Concurrent MPTCP + TCP + UDP competing flows
ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_tcp_udp_cross
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_2connections_ctrl_mptcp
   127.0.0.1 mptcp const10:const1000:const10:const1000 plain 3
   -mptcp const10:const1000:const10:const1000 --control-over-mptcp)

ADD_NETPERFMETER_FLOW_TEST(flow_multi_mptcp_2connections_fragmented
   127.0.0.1 mptcp const10:const1000:const10:const1000:maxmsgsize=250 plain 3
   -mptcp const10:const1000:const10:const1000:maxmsgsize=250)


# ===========================================================================
# 50 REALISTIC MPTCP APPLICATION DISTRIBUTION PROFILES (Profiles 201 - 250)
# (RFC 8684 Multipath TCP: Mobile Handover, Hybrid Access & Cloud Bonding)
# ===========================================================================

# ====== 30. Mobile 5G + Wi-Fi Handover & Interactive Voice/Assistants ======
# 201. Mobile Voice Assistant (Siri/Alexa style) Interactive Stream (45 fps, 380 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_voice_assistant
   127.0.0.1 mptcp uniform40,50:uniform280,480:uniform40,50:uniform280,480 plain 8)

# 202. 5G-to-Wi-Fi Seamless VoWiFi/VoNR Handover Stream over IPv6 (50 fps, 280 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_vowifi_handover_v6
   ::1       mptcp uniform48,52:truncnormal280,25:uniform48,52:truncnormal280,25 plain 8)

# 203. Lossless Hi-Res Music Streaming over Dual-Path Cellular+Wi-Fi (55 fps, 1200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_hires_audio_stream
   127.0.0.1 mptcp uniform50,60:uniform1000,1400:uniform50,60:uniform1000,1400 plain 8)

# 204. Mobile Push Notification & Presence Keepalive Channel (10 fps, 160 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_push_keepalive
   127.0.0.1 mptcp uniform8,12:uniform100,220:uniform8,12:uniform100,220 plain 8)

# 205. Dual-Radio Smartphone RCS Rich Messaging Sync over IPv6 (15 fps, Exp mean 700 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_rcs_messaging_v6
   ::1       mptcp uniform12,18:exp700:uniform12,18:exp700 plain 8)

# 206. Mobile Cloud Photo Backup Chunked Upload with Segmentation (12 fps, 3600 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_cloud_backup_frag
   127.0.0.1 mptcp const12:truncnormal3600,250:const12:truncnormal3600,250:maxmsgsize=1200 plain 8)

# 207. Connected Car Infotainment & OTA Map Tile Stream (20 fps, Pareto mean 1500 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_car_infotainment
   127.0.0.1 mptcp const20:pareto1000,3:const20:pareto1000,3 plain 8)

# 208. Mobile Banking Mutual-Auth API over IPv6 (20 fps, 300 B req -> Exp 950 B resp)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mobile_banking_api_v6
   ::1       mptcp uniform15,25:uniform200,400:uniform15,25:exp950 plain 8)


# ====== 31. Hybrid Access Bonding (DSL/Fiber + 5G FWA CPE Gateways) ======
# 209. Hybrid CPE Residential Web & HTTP/2 Browsing Aggregate (35 fps, 350 B -> Exp 1100 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_hybrid_cpe_web_browsing
   127.0.0.1 mptcp uniform30,40:uniform200,500:uniform30,40:exp1100 plain 8)

# 210. Hybrid Access Adaptive ABR Video Streaming over IPv6 (16 fps, Pareto mean 2400 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_hybrid_cpe_abr_video_v6
   ::1       mptcp const16:pareto1600,3:const16:pareto1600,3:maxmsgsize=1200 plain 8)

# 211. SD-WAN Branch Office SaaS ERP/CRM Tunnel Bonding (40 fps, 600 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_sdwan_branch_saas_bond
   127.0.0.1 mptcp uniform35,45:truncnormal600,50:uniform35,45:truncnormal600,50 plain 8)

# 212. SD-WAN Site-to-Site Encrypted Overlay Keepalive & Control (50 fps, 96 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_sdwan_overlay_control
   127.0.0.1 mptcp const50:uniform64,128:const50:uniform64,128 plain 8)

# 213. Remote Work VDI / RDP Virtual Desktop Stream over IPv6 (30 fps, Pareto mean 900 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_hybrid_vdi_desktop_v6
   ::1       mptcp uniform25,35:pareto600,3:uniform25,35:pareto600,3 plain 8)

# 214. Enterprise Cloud Print & Document Spooler Transfer (10 fps, 3000 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_hybrid_cloud_print_frag
   127.0.0.1 mptcp const10:truncnormal3000,200:const10:truncnormal3000,200:maxmsgsize=1000 plain 8)

# 215. Multi-ISP SOCKS/HTTP Proxy Trunking Gateway (45 fps, 750 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_socks_proxy_trunk
   127.0.0.1 mptcp uniform40,50:uniform500,1000:uniform40,50:uniform500,1000 plain 8)


# ====== 32. Data Center Multi-NIC Aggregation & Cloud Storage ==============
# 216. Multi-NIC Distributed Object Storage Replication (25 fps, 3200 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_object_replication_frag
   127.0.0.1 mptcp const25:truncnormal3200,200:const25:truncnormal3200,200:maxmsgsize=1000 plain 8)

# 217. Dual-ToR Leaf-Spine East-West RPC Microservice Mesh over IPv6 (60 fps, 320 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_east_west_rpc_v6
   ::1       mptcp uniform50,70:truncnormal320,30:uniform50,70:truncnormal320,30 plain 8)

# 218. Distributed SQL Read-Replica Binlog Stream (30 fps, 1100 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_sql_binlog_sync
   127.0.0.1 mptcp const30:uniform900,1300:const30:uniform900,1300 plain 8)

# 219. Container Registry Layer Pull Distribution with Chunking (14 fps, Pareto mean 2700 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_container_image_frag
   127.0.0.1 mptcp const14:pareto1800,3:const14:pareto1800,3:maxmsgsize=1200 plain 8)

# 220. Multi-Path Distributed Cache (Redis/KeyDB) Cluster Sync over IPv6 (70 fps, 140 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_cache_cluster_v6
   ::1       mptcp uniform60,80:uniform90,190:uniform60,80:uniform90,190 plain 8)

# 221. Cross-AZ Kafka Partition MirrorMaker Stream (25 fps, 1250 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_kafka_mirrormaker
   127.0.0.1 mptcp uniform20,30:truncnormal1250,80:uniform20,30:truncnormal1250,80 plain 8)

# 222. Dual-Homed Load Balancer Health & Session State Sync (50 fps, 120 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_dc_lb_state_sync
   127.0.0.1 mptcp const50:uniform80,160:const50:uniform80,160 plain 8)


# ====== 33. High-Speed Rail, Maritime & Connected Transport ================
# 223. High-Speed Train Multi-Modem Roof Antenna Wi-Fi Backhaul (30 fps, 900 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_rail_train_wifi_backhaul
   127.0.0.1 mptcp uniform25,35:uniform700,1100:uniform25,35:uniform700,1100 plain 8)

# 224. FRMCS / Railway Wayside Telemetry & Diagnostics over IPv6 (20 fps, 260 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_rail_wayside_telemetry_v6
   ::1       mptcp const20:truncnormal260,25:const20:truncnormal260,25 plain 8)

# 225. Maritime VSAT + 5G Coastal Vessel Hybrid Link (15 fps, 800 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_marine_vsat_5g_hybrid
   127.0.0.1 mptcp uniform12,18:uniform600,1000:uniform12,18:uniform600,1000 plain 8)

# 226. Connected Ambulance Dual-LTE/5G Live Ultrasound & Vitals Stream (25 fps, 1150 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ambulance_telemedicine
   127.0.0.1 mptcp const25:truncnormal1150,90:const25:truncnormal1150,90 plain 8)

# 227. Autonomous Shuttle Multi-Carrier Fleet Telemetry over IPv6 (30 fps, 300 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_shuttle_fleet_telemetry_v6
   ::1       mptcp uniform25,35:uniform200,400:uniform25,35:uniform200,400 plain 8)

# 228. Public Transit Bus CCTV & Passenger Counter Offload (12 fps, Pareto mean 2100 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_transit_cctv_offload_frag
   127.0.0.1 mptcp const12:pareto1400,3:const12:pareto1400,3:maxmsgsize=1000 plain 8)


# ====== 34. Financial Trading, Multi-Homed Exchanges & FinTech =============
# 229. Multi-Homed Algorithmic Order Routing with TCP_NODELAY (80 fps, 140 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_algo_order_routing
   127.0.0.1 mptcp truncnormal80,4:uniform100,180:truncnormal80,4:uniform100,180:nodelay=on plain 8)

# 230. Dual-Carrier FIX Execution Drop-Copy Feed over IPv6 (40 fps, 320 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_fix_drop_copy_v6
   ::1       mptcp uniform35,45:uniform240,400:uniform35,45:uniform240,400 plain 8)

# 231. Retail POS Terminal Dual-WAN (Ethernet + Cellular Failover) Auth (20 fps, 240 B -> Exp 750 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_pos_terminal_auth
   127.0.0.1 mptcp uniform15,25:uniform180,300:uniform15,25:exp750 plain 8)

# 232. ATM Fleet Dual-Path Encrypted Transaction Tunnel (15 fps, 300 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_atm_transaction_tunnel
   127.0.0.1 mptcp const15:truncnormal300,25:const15:truncnormal300,25 plain 8)

# 233. Multi-Exchange Market Data Replay Stream over IPv6 (50 fps, Pareto mean 600 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_fin_market_replay_v6
   ::1       mptcp const50:pareto400,3:const50:pareto400,3 plain 8)


# ====== 35. Industrial Automation, Smart Grid & Mission-Critical IoT =======
# 234. Redundant Dual-LAN PRP/HSR-Style Factory PLC Supervision (40 fps, 200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ind_redundant_plc_bus
   127.0.0.1 mptcp const40:uniform140,260:const40:uniform140,260 plain 8)

# 235. Smart Grid Substation Dual-Uplink IEC 61850 MMS Control over IPv6 (25 fps, 280 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_grid_iec61850_mms_v6
   ::1       mptcp uniform20,30:truncnormal280,25:uniform20,30:truncnormal280,25 plain 8)

# 236. Wind Farm Offshore SCADA Multi-Link Telemetry (15 fps, 450 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_energy_windfarm_scada
   127.0.0.1 mptcp uniform12,18:uniform350,550:uniform12,18:uniform350,550 plain 8)

# 237. Autonomous Mining Haul Truck Dual-Wi-Fi/LTE Command Stream (30 fps, 220 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_mining_haul_truck_ctrl
   127.0.0.1 mptcp const30:truncnormal220,20:const30:truncnormal220,20 plain 8)

# 238. High-Resolution Machine Vision Defect Image Upload (8 fps, 3600 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ind_defect_imaging_frag
   127.0.0.1 mptcp const8:truncnormal3600,250:const8:truncnormal3600,250:maxmsgsize=1200 plain 8)


# ====== 36. Edge AI, Cloud Gaming & Interactive Streaming ==================
# 239. Multi-Path Cloud Gaming Video & Input Stream (60 fps, 950 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_cloud_gaming_stream
   127.0.0.1 mptcp truncnormal60,3:uniform800,1100:truncnormal60,3:uniform800,1100:nodelay=on plain 8)

# 240. Mobile AR/VR Edge Offload Split-Rendering Stream over IPv6 (60 fps, 700 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_edge_ar_split_render_v6
   ::1       mptcp uniform55,65:uniform550,850:uniform55,65:uniform550,850 plain 8)

# 241. Distributed AI Model Checkpoint Shard Sync (10 fps, Pareto mean 3000 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ai_checkpoint_sync_frag
   127.0.0.1 mptcp const10:pareto2000,3:const10:pareto2000,3:maxmsgsize=1000 plain 8)

# 242. Edge LLM Token Streaming Inference Gateway over IPv6 (40 fps, 180 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_edge_llm_token_stream_v6
   ::1       mptcp uniform35,45:uniform120,240:uniform35,45:uniform120,240 plain 8)

# 243. Multi-Camera Live Broadcast Bonding Backpack Stream (30 fps, 1250 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_broadcast_bonding_uplink
   127.0.0.1 mptcp const30:truncnormal1250,90:const30:truncnormal1250,90 plain 8)


# ====== 37. Control-Over-MPTCP Realistic Application Profiles ==============
# 244. Control-Over-MPTCP: Mobile Interactive RPC Service (25 fps, 300 B -> Exp 900 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_mobile_rpc
   127.0.0.1 mptcp uniform20,30:uniform200,400:uniform20,30:exp900 plain 8 --control-over-mptcp)

# 245. Control-Over-MPTCP: Dual-Stack IPv6 Cloud Microservice Stream (35 fps, 450 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_cloud_svc_v6
   ::1       mptcp uniform30,40:truncnormal450,40:uniform30,40:truncnormal450,40 plain 8 --control-over-mptcp)

# 246. Control-Over-MPTCP: Hybrid Access Segmented Bulk Sync (12 fps, 3200 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_bulk_frag
   127.0.0.1 mptcp const12:truncnormal3200,200:const12:truncnormal3200,200:maxmsgsize=1000 plain 8 --control-over-mptcp)

# 247. Control-Over-MPTCP: Connected Vehicle Multi-Radio Telemetry (15 fps, 350 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_v2x_telemetry
   127.0.0.1 mptcp uniform12,18:uniform250,450:uniform12,18:uniform250,450 plain 8 --control-over-mptcp)

# 248. Control-Over-MPTCP: Financial Low-Latency Order Channel over IPv6 (60 fps, 140 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_fin_orders_v6
   ::1       mptcp truncnormal60,3:uniform100,180:truncnormal60,3:uniform100,180:nodelay=on plain 8 --control-over-mptcp)

# 249. Control-Over-MPTCP: Media Streaming Pareto Chunk Delivery (15 fps, Pareto mean 1500 B)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_media_pareto
   127.0.0.1 mptcp const15:pareto1000,3:const15:pareto1000,3 plain 8 --control-over-mptcp)

# 250. Control-Over-MPTCP: Distributed Consensus & Cluster Heartbeat (50 fps, 96 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_mptcp_ctrl_mptcp_cluster_hb
   127.0.0.1 mptcp const50:uniform64,128:const50:uniform64,128 plain 8 --control-over-mptcp)

ENDIF()
