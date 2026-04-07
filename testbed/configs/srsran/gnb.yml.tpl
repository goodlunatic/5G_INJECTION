cu_cp:
  amf:
    addr: 127.0.0.5
    port: 38412
    bind_addr: 127.0.0.1
    supported_tracking_areas:
      - tac: 1
        plmn_list:
          - plmn: "00101"
            tai_slice_support_list:
              - sst: 1

ru_sdr:
  device_driver: uhd
  device_args: __X310_DEVICE_ARGS__
  clock: __X310_CLOCK__
  sync: __X310_SYNC__
  srate: __GNB_SRATE__
  tx_gain: __GNB_TX_GAIN__
  rx_gain: __GNB_RX_GAIN__

cell_cfg:
  dl_arfcn: __NR_DL_ARFCN__
  band: 3
  channel_bandwidth_MHz: __NR_BW_MHZ__
  common_scs: 15
  plmn: "00101"
  tac: 1
  pci: 1
  pdcch:
    dedicated:
      ss2_type: common
      dci_format_0_1_and_1_1: false
    common:
      ss0_index: 0
      coreset0_index: 12
  prach:
    prach_config_index: 1

log:
  filename: /workspace/testbed/logs/srsran/gnb.log
  all_level: info

pcap:
  mac_enable: false
  mac_filename: /workspace/testbed/logs/srsran/gnb_mac.pcap
  ngap_enable: false
  ngap_filename: /workspace/testbed/logs/srsran/gnb_ngap.pcap
