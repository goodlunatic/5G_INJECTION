# This is the UE template used by testbed/scripts/start-srsue.sh.
# The launcher fills in subscriber credentials, radio parameters, and B210
# device arguments, then writes the runtime file to testbed/runtime/ue.conf.

[rf]
device_name = UHD
# Filled from B210 serial/clock/time-source/frame-size related env vars.
device_args = __B210_DEVICE_ARGS__
# For the current 20 MHz setup this is typically 23.04e6.
srate = __UE_SRATE__
freq_offset = __UE_FREQ_OFFSET__
tx_gain = __B210_TX_GAIN__
rx_gain = __B210_RX_GAIN__
# Static timing advance used by this testbed to help uplink alignment.
time_adv_nsamples = __TIME_ADV_NSAMPLES__
continuous_tx = yes

[rat.eutra]
# Disable LTE and force the UE to use NR only in this testbed.
nof_carriers = 0

[rat.nr]
# Must match the gNB band configured in gnb.yml.tpl.
bands = 3
nof_carriers = 1
# Both ARFCNs must match the cell the gNB is transmitting.
dl_nr_arfcn = __NR_DL_ARFCN__
ssb_nr_arfcn = __SSB_NR_ARFCN__
# 106 PRBs corresponds to a 20 MHz NR carrier at 15 kHz SCS.
nof_prb = __UE_NOF_PRB__
max_nof_prb = __UE_NOF_PRB__
scs = 15

[pcap]
enable = none
mac_nr_filename = /workspace/testbed/logs/srsue/ue_mac_nr.pcap
nas_filename = /workspace/testbed/logs/srsue/ue_nas.pcap

[log]
all_level = info
phy_lib_level = warning
all_hex_limit = 32
filename = /workspace/testbed/logs/srsue/ue.log
file_max_size = -1

[usim]
# Soft USIM mode uses the IMSI/K/OPC values from testbed/subscribers/test-ue.env.
mode = soft
algo = milenage
opc = __UE_OPC__
k = __UE_K__
imsi = __UE_IMSI__
imei = 353490069873319

[nas]
# APN/DNN must match what Open5GS serves in open5gs.yml.
apn = __UE_APN__
apn_protocol = ipv4

[slicing]
enable = false

[gw]
# Local TUN interface created by srsUE for the UE data plane.
ip_devname = tun_srsue
ip_netmask = 255.255.255.0

[gui]
enable = false
