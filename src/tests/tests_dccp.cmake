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
#### DCCP FLOW INTEGRATION TESTS (RFC 4340)                              ####
#############################################################################

IF (HAVE_DCCP_FOR_TESTS)

# ====== 1. Bidirectional Flows =============================================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_bidi_ipv4 127.0.0.1 dccp const10:const1000:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_bidi_ipv6 ::1       dccp const10:const1000:const10:const1000 plain)

# ====== 2. Upload-Only Flows (Active -> Passive) ===========================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_upload_ipv4 127.0.0.1 dccp const10:const1000:const0:const0 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_upload_ipv6 ::1       dccp const10:const1000:const0:const0 plain)

# ====== 3. Download-Only Flows (Passive -> Active) =========================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_download_ipv4 127.0.0.1 dccp const0:const0:const10:const1000 plain)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_download_ipv6 ::1       dccp const0:const0:const10:const1000 plain)

# ====== 4. Default Shorthand, CCID Options & Local Bind ====================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_default_shorthand
   127.0.0.1 dccp default plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ccid2
   127.0.0.1 dccp const10:const1000:const10:const1000:ccid=2 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ccid3
   127.0.0.1 dccp const10:const1000:const10:const1000:ccid=3 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_local_bind_ipv4
   127.0.0.1 dccp const10:const1000:const10:const1000 plain 2
   -L 127.0.0.1 --server-arg -L --server-arg 127.0.0.1)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_tc_ef_ipv4
   127.0.0.1 dccp const10:const1000:const10:const1000:tc=EF plain 2
   --expect-config "FLOW0_TRAFFIC_CLASS=\"0xb8\"")
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_tc_fl_ipv6
   ::1       dccp const10:const1000:const10:const1000:tc=AF31:fl=auto plain 2
   --expect-config "FLOW0_TRAFFIC_CLASS_NAME=\"AF31\"")

IF (HAVE_SCTP_FOR_TESTS)
   ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ctrl_sctp_bidi_ipv4
      127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --control-over-sctp)
ENDIF()

IF (HAVE_MPTCP_FOR_TESTS)
   ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ctrl_mptcp_bidi_ipv4
      127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --control-over-mptcp)
ENDIF()

IF (HAVE_QUIC_FOR_TESTS)
   ADD_NETPERFMETER_FLOW_TEST(flow_dccp_ctrl_quic_bidi_ipv4
      127.0.0.1 dccp const10:const1000:const10:const1000 plain 3 --control-over-quic)
ENDIF()

# ====== 5. Compression Formats & Output Suppression ========================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_format_gz  127.0.0.1 dccp const10:const1000:const10:const1000 gz)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_format_bz2 127.0.0.1 dccp const10:const1000:const10:const1000 bz2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_format_xz  127.0.0.1 dccp const10:const1000:const10:const1000 xz)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_format_zst 127.0.0.1 dccp const10:const1000:const10:const1000 zst)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_no_scalar  127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --no-scalar)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_no_vector  127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --no-vector)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_no_outputs 127.0.0.1 dccp const10:const1000:const10:const1000 plain 2 --no-scalar --no-vector)

# ====== 6. Framing and Defragmentation (maxmsgsize & defragtimeout) ========
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_framing_small      127.0.0.1 dccp const10:const1000:const10:const1000:maxmsgsize=250  plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_framing_medium     127.0.0.1 dccp const10:const4000:const10:const4000:maxmsgsize=1000 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_framing_large      127.0.0.1 dccp const10:const8000:const10:const8000:maxmsgsize=1200 plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_framing_asymmetric 127.0.0.1 dccp const10:const4000:const10:const1000:maxmsgsize=500  plain 2)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_defragtimeout      127.0.0.1 dccp const10:const4000:const10:const4000:maxmsgsize=1000:defragtimeout=2500 plain 2)

# ====== 7. On-Off Flow Schedule Tests ======================================
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_single_burst_ipv4
   127.0.0.1 dccp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_single_burst_ipv6
   ::1       dccp const20:const1000:const0:const0:onoff=1,3 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_multiburst_ipv4
   127.0.0.1 dccp const20:const1000:const0:const0:onoff=1,2,3,4 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_delayed_start
   127.0.0.1 dccp const20:const1000:const0:const0:onoff=2 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_fragmented
   127.0.0.1 dccp const20:const3000:const0:const0:onoff=1,3:maxmsgsize=1000 plain 5)
ADD_NETPERFMETER_FLOW_TEST(flow_dccp_onoff_exp_size
   127.0.0.1 dccp const20:exp1000:const0:const0:onoff=1,4 plain 6)

# ====== 8. Random Distribution Tests (VBR Flows) ===========================
ADD_NETPERFMETER_FLOW_TEST(dist_dccp_uniform     127.0.0.1 dccp uniform15,25:uniform800,1200:uniform15,25:uniform800,1200 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_dccp_exp         127.0.0.1 dccp const20:exp1000:const20:exp1000 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_dccp_truncnormal 127.0.0.1 dccp truncnormal20,2:truncnormal1000,100:truncnormal20,2:truncnormal1000,100 plain 8)
ADD_NETPERFMETER_FLOW_TEST(dist_dccp_pareto      127.0.0.1 dccp pareto15,3:pareto667,3:pareto15,3:pareto667,3 plain 8)

# ====== 9. Multi-Connection & Cross-Protocol Concurrency ===================
ADD_NETPERFMETER_FLOW_TEST(flow_multi_dccp_2connections_ipv4
   127.0.0.1 dccp const10:const1000:const10:const1000 plain 3
   -dccp const10:const1000:const10:const1000)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_dccp_2connections_ipv6
   ::1       dccp const10:const1000:const10:const1000 plain 3
   -dccp const10:const1000:const10:const1000)
ADD_NETPERFMETER_FLOW_TEST(flow_multi_dccp_tcp_udp_cross
   127.0.0.1 dccp const10:const1000:const10:const1000 plain 3
   -tcp const10:const1000:const10:const1000
   -udp const10:const1000:const10:const1000)


# ===========================================================================
# 50 REALISTIC DCCP APPLICATION DISTRIBUTION PROFILES (Profiles 251 - 300)
# (RFC 4340 DCCP, RFC 4341 CCID-2 TCP-like, RFC 4342 CCID-3 TFRC)
# ===========================================================================

# ====== 38. Real-Time Audio, VoIP & Interactive Voice ======================
# 251. VoIP G.711 Narrowband Speech over DCCP CCID-3 (~50 fps, 200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_g711_narrowband
   127.0.0.1 dccp uniform45,55:uniform160,240:uniform45,55:uniform160,240:ccid=3 plain 8)

# 252. VoIP G.729 Low-Bitrate Voice over IPv6 (~50 fps, 60 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_g729_lowbitrate_v6
   ::1       dccp uniform45,55:uniform40,80:uniform45,55:uniform40,80:ccid=3 plain 8)

# 253. High-Fidelity Wideband Opus Voice with TFRC CCID-3 (50 fps, 300 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_opus_wideband
   127.0.0.1 dccp uniform48,52:truncnormal300,30:uniform48,52:truncnormal300,30:ccid=3 plain 8)

# 254. Fullband Stereo Audio Contribution Stream (50 fps, 500 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_audio_opus_stereo_vbr
   127.0.0.1 dccp const50:uniform400,600:const50:uniform400,600:ccid=3 plain 8)

# 255. Live Radio Broadcast AAC-ELD Audio Feed (40 fps, 850 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_audio_radio_aac_eld
   127.0.0.1 dccp uniform35,45:truncnormal850,70:uniform35,45:truncnormal850,70:ccid=3 plain 8)

# 256. Multi-Party Voice Conference Mixer Uplink over IPv6 (50 fps, 420 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_conference_mixer_v6
   ::1       dccp const50:truncnormal420,35:const50:truncnormal420,35:ccid=3 plain 8)

# 257. Push-to-Talk Tactical Dispatch Voice Channel (30 fps, 240 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voip_ptt_dispatch
   127.0.0.1 dccp uniform25,35:uniform180,300:uniform25,35:uniform180,300 plain 8)

# 258. Smart Speaker Cloud Voice Assistant Audio Stream over IPv6 (45 fps, 380 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_voice_assistant_uplink_v6
   ::1       dccp uniform40,50:uniform280,480:uniform40,50:uniform280,480:ccid=3 plain 8)


# ====== 39. Live Video Streaming, Surveillance & Media Ingest ==============
# 259. Adaptive Low-Latency Live Video Stream CCID-3 (30 fps, Pareto mean 1500 B, maxmsgsize=1200)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_video_live_tfrc_frag
   127.0.0.1 dccp const30:pareto1000,3:const30:pareto1000,3:ccid=3:maxmsgsize=1200 plain 8)

# 260. CCTV Security Camera RTSP/DCCP Stream (25 fps, 1150 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cctv_rtsp_stream
   127.0.0.1 dccp truncnormal25,2:uniform1000,1300:truncnormal25,2:uniform1000,1300:ccid=3 plain 8)

# 261. H.265 IP Surveillance Feed over IPv6 (30 fps, Pareto mean 1200 B)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cctv_h265_surveillance_v6
   ::1       dccp const30:pareto800,3:const30:pareto800,3:ccid=3 plain 8)

# 262. WebRTC Simulcast Video Layer over DCCP CCID-3 (30 fps, 950 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_webrtc_video_layer
   127.0.0.1 dccp uniform25,35:uniform750,1150:uniform25,35:uniform750,1150:ccid=3 plain 8)

# 263. Drone FPV Low-Latency Video Downlink (60 fps, 750 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_drone_fpv_video_feed
   127.0.0.1 dccp truncnormal60,3:uniform650,850:truncnormal60,3:uniform650,850:ccid=3 plain 8)

# 264. Interactive Telehealth Video Consultation over IPv6 (30 fps, 1100 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_med_telehealth_video_v6
   ::1       dccp const30:uniform950,1250:const30:uniform950,1250:ccid=3 plain 8)

# 265. Video Doorbell Bidirectional Media Stream (24 fps, 900 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_home_doorbell_media
   127.0.0.1 dccp const24:uniform750,1050:const24:uniform750,1050 plain 8)

# 266. Remote Desktop Screen Delta Tile Stream CCID-2 (20 fps, Pareto mean 750 B)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_screen_sharing_vbr
   127.0.0.1 dccp uniform15,25:pareto500,3:uniform15,25:pareto500,3:ccid=2 plain 8)


# ====== 40. Multiplayer Gaming, Cloud Gaming & AR/VR Pose Telemetry ========
# 267. 128-Tick Competitive FPS Game State Updates (128 fps, 130 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_tick_128hz
   127.0.0.1 dccp truncnormal128,4:uniform80,180:truncnormal128,4:uniform80,180:ccid=2 plain 8)

# 268. Standard 64-Tick Multiplayer Game Server (64 fps, 150 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_tick_64hz
   127.0.0.1 dccp truncnormal64,3:uniform100,200:truncnormal64,3:uniform100,200 plain 8)

# 269. Console Cross-Platform State Sync over IPv6 (30 fps, 180 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_tick_30hz_v6
   ::1       dccp truncnormal30,2:uniform120,240:truncnormal30,2:uniform120,240 plain 8)

# 270. MMO Virtual World Entity Position Updates (20 fps, 400 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_mmo_worldsync
   127.0.0.1 dccp uniform15,25:uniform200,600:uniform15,25:uniform200,600:ccid=2 plain 8)

# 271. Rigid-Body Physics Synchronization Stream (60 fps, 150 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_gaming_physics_sync
   127.0.0.1 dccp const60:truncnormal150,15:const60:truncnormal150,15 plain 8)

# 272. Cloud Gaming Frame & Controller Stream CCID-3 (60 fps, 900 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cloud_gaming_stream
   127.0.0.1 dccp truncnormal60,3:uniform750,1050:truncnormal60,3:uniform750,1050:ccid=3 plain 8)

# 273. AR/VR 6-DoF Head/Hand Spatial Pose Tracking over IPv6 (90 fps, 120 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ar_pose_tracking_v6
   ::1       dccp truncnormal90,4:uniform90,150:truncnormal90,4:uniform90,150:ccid=3 plain 8)

# 274. Bilateral Telepresence Haptic Force Feedback Loop (~100 fps, ~100 B, realistic input jitter)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_haptic_feedback_loop
   127.0.0.1 dccp truncnormal100,3:uniform90,110:truncnormal100,3:uniform90,110 plain 8)


# ====== 41. Connected Vehicles (C-V2X), Aerospace & Autonomous Drones ======
# 275. Connected Vehicle CAM Safety Beacons over IPv6 (10 fps, 350 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_v2x_cam_safety_v6
   ::1       dccp truncnormal10,1:uniform200,500:truncnormal10,1:uniform200,500 plain 8)

# 276. Road Hazard DENM Decentralized Notification (8 fps, 450 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_v2x_denm_hazard
   127.0.0.1 dccp const8:uniform300,600:const8:uniform300,600 plain 8)

# 277. Satellite CCSDS Telemetry Downlink CCID-3 (20 fps, 750 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_aero_satellite_downlink
   127.0.0.1 dccp const20:uniform500,1000:const20:uniform500,1000:ccid=3 plain 8)

# 278. LEO Inter-Satellite Link Routing Beacon over IPv6 (15 fps, 150 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_aero_isl_routing_v6
   ::1       dccp uniform10,20:uniform100,200:uniform10,20:uniform100,200 plain 8)

# 279. ADS-B Mode-S Aircraft Position Transponder (15 fps, 170 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_aero_adsb_transponder
   127.0.0.1 dccp const15:uniform120,220:const15:uniform120,220 plain 8)

# 280. Primary Radar Target Plot Correlation Feed (50 fps, 400 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_aero_radar_track_sync
   127.0.0.1 dccp truncnormal50,4:uniform300,500:truncnormal50,4:uniform300,500:ccid=2 plain 8)

# 281. Autonomous Drone Swarm Attitude & Telemetry Stream (40 fps, 240 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_uav_swarm_telemetry
   127.0.0.1 dccp uniform35,45:truncnormal240,25:uniform35,45:truncnormal240,25:ccid=3 plain 8)


# ====== 42. Financial Market Data Feeds, Order Books & Risk Telemetry ======
# 282. ITCH/OUCH Low-Latency Market Depth Feed CCID-2 (80 fps, 96 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_itch_feed
   127.0.0.1 dccp truncnormal80,4:uniform64,128:truncnormal80,4:uniform64,128:ccid=2 plain 8)

# 283. Crypto Perpetual Futures Order Book Stream over IPv6 (40 fps, Pareto mean 600 B)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_crypto_l2_book_v6
   ::1       dccp const40:pareto400,3:const40:pareto400,3:ccid=2 plain 8)

# 284. Multi-Bank FX ECN Spot Quotation Pipeline (70 fps, 130 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_forex_spot_quotes
   127.0.0.1 dccp truncnormal70,5:uniform100,160:truncnormal70,5:uniform100,160:ccid=2 plain 8)

# 285. Centralized Pre-Trade Risk Heartbeat (60 fps, 64 B, const)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_risk_heartbeat
   127.0.0.1 dccp const60:const64:const60:const64 plain 8)

# 286. Trade Allocation Drop Copy Execution Feed over IPv6 (25 fps, 500 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_drop_copy_feed_v6
   ::1       dccp uniform20,30:uniform350,650:uniform20,30:uniform350,650:ccid=2 plain 8)

# 287. Derivatives Volatility Surface & Greeks Feed (25 fps, Exp mean 550 B)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_options_greeks_feed
   127.0.0.1 dccp const25:exp550:const25:exp550:ccid=3 plain 8)

# 288. Exchange Market Data Replay Multicast-Replacement Feed (50 fps, 320 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_fin_market_replay
   127.0.0.1 dccp const50:truncnormal320,30:const50:truncnormal320,30:ccid=3 plain 8)


# ====== 43. Industrial IoT, Smart Grid & Healthcare Telemetry ==============
# 289. Robot Arm 6-DoF Kinematics Telemetry over IPv6 (100 fps, 150 B, truncnormal/uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ind_robot_kinematics_v6
   ::1       dccp truncnormal100,5:uniform120,180:truncnormal100,5:uniform120,180 plain 8)

# 290. Automated Guided Vehicle (AGV) LIDAR Point Cloud (40 fps, 1150 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ind_agv_lidar_stream
   127.0.0.1 dccp uniform35,45:uniform1000,1300:uniform35,45:uniform1000,1300:ccid=3 plain 8)

# 291. IEEE C37.118 Synchrophasor PMU Grid Frequency Stream (50 fps, 140 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_smartgrid_pmu_c37_118
   127.0.0.1 dccp const50:truncnormal140,10:const50:truncnormal140,10:ccid=3 plain 8)

# 292. Smart Grid Substation Telemetry over IPv6 (15 fps, 240 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_smartgrid_substation_v6
   ::1       dccp uniform12,18:uniform180,300:uniform12,18:uniform180,300 plain 8)

# 293. Continuous Bedside Multi-Lead ECG Waveform Telemetry (50 fps, 200 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_med_ecg_telemetry
   127.0.0.1 dccp const50:uniform150,250:const50:uniform150,250:ccid=3 plain 8)

# 294. Critical Care ICU Central Alarm & Vitals Broadcast (15 fps, 300 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_med_icu_vitals_stream
   127.0.0.1 dccp const15:truncnormal300,25:const15:truncnormal300,25 plain 8)


# ====== 44. Cloud Observability, Edge AI & Segmented Datagram Framing ======
# 295. Linux eBPF Continuous Kernel Observability Trace Feed (65 fps, 300 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cloud_ebpf_observability
   127.0.0.1 dccp uniform50,80:uniform200,400:uniform50,80:uniform200,400:ccid=2 plain 8)

# 296. Enterprise SIEM Syslog Event Pipeline (50 fps, 400 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_cloud_siem_syslog
   127.0.0.1 dccp uniform40,60:uniform250,550:uniform40,60:uniform250,550:ccid=2 plain 8)

# 297. Edge AI Object Detection Bounding-Box Metadata Stream over IPv6 (30 fps, 360 B, truncnormal)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ai_edge_detections_v6
   ::1       dccp const30:truncnormal360,30:const30:truncnormal360,30:ccid=3 plain 8)

# 298. Volumetric 3D Point-Cloud Frame Streaming with Segmentation (12 fps, 3000 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_volumetric_pointcloud_frag
   127.0.0.1 dccp const12:truncnormal3000,200:const12:truncnormal3000,200:ccid=3:maxmsgsize=1000 plain 8)

# 299. Predictive Maintenance FFT Vibration Spectra with Segmentation (10 fps, Pareto mean 2100 B, maxmsgsize=1000)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_ind_fft_spectra_frag
   127.0.0.1 dccp const10:pareto1400,3:const10:pareto1400,3:ccid=2:maxmsgsize=1000:defragtimeout=2500 plain 8)

# 300. Encrypted Datagram Tunnel Encapsulation Flow over IPv6 (45 fps, 550 B, uniform)
ADD_NETPERFMETER_FLOW_TEST(profile_dccp_datagram_tunnel_encap_v6
   ::1       dccp uniform40,50:uniform400,700:uniform40,50:uniform400,700:ccid=3 plain 8)

ENDIF()
