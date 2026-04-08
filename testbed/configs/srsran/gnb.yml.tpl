# This is the gNB template used by testbed/scripts/start-srsran-gnb.sh.
# The launcher replaces the __PLACEHOLDER__ values at container start and writes
# the rendered runtime file to testbed/runtime/gnb.yml.

cu_cp:
  amf:
    # Open5GS AMF address inside the host-networked testbed.
    addr: 127.0.0.5
    port: 38412
    # Local bind address for NGAP toward the AMF.
    bind_addr: 127.0.0.1
    supported_tracking_areas:
      # TAC/PLMN/SST here must match Open5GS and the UE profile.
      - tac: 1
        plmn_list:
          - plmn: "00101"
            tai_slice_support_list:
              - sst: 1

ru_sdr:
  # The testbed uses an Ettus USRP via UHD.
  device_driver: uhd
  # Usually includes type=x300, addr=<x310 ip>, master_clock_rate, and time_source.
  device_args: __X310_DEVICE_ARGS__
  # clock controls the 10 MHz reference source seen by srsRAN.
  clock: __X310_CLOCK__
  # sync controls the time/PPS source seen by srsRAN.
  sync: __X310_SYNC__
  # 23.04 Msps is the common sample rate used here for a 20 MHz NR carrier.
  srate: __GNB_SRATE__
  tx_gain: __GNB_TX_GAIN__
  rx_gain: __GNB_RX_GAIN__

cell_cfg:
  # Downlink carrier center frequency in NR-ARFCN.
  dl_arfcn: __NR_DL_ARFCN__
  # Current template is pinned to band n3, which is an FDD band.
  band: 3
  channel_bandwidth_MHz: __NR_BW_MHZ__
  # 15 kHz SCS is what the paired srsUE template expects.
  common_scs: 15
  plmn: "00101"
  tac: 1
  pci: 1
  pdcch:
    dedicated:
      # This matches the simple/common search-space setup used by this testbed.
      ss2_type: common
      dci_format_0_1_and_1_1: false
    common:
      ss0_index: 0
      coreset0_index: 12
  prach:
    # PRACH settings also need to remain compatible with the selected band/SCS.
    prach_config_index: 1

log:
  filename: /workspace/testbed/logs/srsran/gnb.log
  all_level: info

pcap:
  # Turn these on only when debugging; they generate extra files.
  mac_enable: false
  mac_filename: /workspace/testbed/logs/srsran/gnb_mac.pcap
  ngap_enable: false
  ngap_filename: /workspace/testbed/logs/srsran/gnb_ngap.pcap
