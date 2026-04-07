[rf]
device_name = UHD
device_args = __B210_DEVICE_ARGS__
srate = __UE_SRATE__
freq_offset = __UE_FREQ_OFFSET__
tx_gain = __B210_TX_GAIN__
rx_gain = __B210_RX_GAIN__
time_adv_nsamples = 100
continuous_tx = yes

[rat.eutra]
nof_carriers = 0

[rat.nr]
bands = 3
nof_carriers = 1
dl_nr_arfcn = __NR_DL_ARFCN__
ssb_nr_arfcn = __SSB_NR_ARFCN__
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
mode = soft
algo = milenage
opc = __UE_OPC__
k = __UE_K__
imsi = __UE_IMSI__
imei = 353490069873319

[nas]
apn = __UE_APN__
apn_protocol = ipv4

[slicing]
enable = false

[gw]
ip_devname = tun_srsue
ip_netmask = 255.255.255.0

[gui]
enable = false
