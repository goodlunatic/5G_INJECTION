# 5G SA Testbed Usage Guide

这个仓库里和自建 5G SA 实验网直接相关的目录现在是 [testbed](testbed)。

这套环境的目标是：

- 用官方 `Open5GS` 搭 5G SA 核心网
- 用官方 `srsRAN_Project` 跑 `gNB`
- 用官方 `srsRAN_4G` 跑 `srsUE`
- 通过 Docker 在同一台主机上统一构建、启动、查看日志和排障

当前默认硬件假设：

- 宿主机已经连接 `USRP X310`
- 宿主机已经连接 `USRP B210`
- 宿主机有 `/dev/net/tun`
- 宿主机已经安装 Docker 和 Docker Compose 插件

## 1. 目录说明

- [testbed](testbed)
  这是实验网主目录。
- [testbed/docker-compose.5g-testbed.yml](testbed/docker-compose.5g-testbed.yml)
  这是整套实验网的编排入口。
- [testbed/docker](testbed/docker)
  这里放 3 个镜像的 Dockerfile。
- [testbed/scripts](testbed/scripts)
  这里放启动脚本和订阅用户管理脚本。
- [testbed/configs/open5gs/open5gs.yml](testbed/configs/open5gs/open5gs.yml)
  这是 Open5GS 配置。
- [testbed/configs/srsran/gnb.yml.tpl](testbed/configs/srsran/gnb.yml.tpl)
  这是 gNB 配置模板。
- [testbed/configs/srsue/ue.conf.tpl](testbed/configs/srsue/ue.conf.tpl)
  这是 UE 配置模板。
- [testbed/subscribers/test-ue.env](testbed/subscribers/test-ue.env)
  这是默认测试 UE 的 IMSI、K、OPC、APN。
- [testbed/runtime](testbed/runtime)
  这里是容器启动时渲染出来的实际运行配置。
- [testbed/logs](testbed/logs)
  这里是核心网、gNB、UE 的本地日志目录。

## 2. 组件版本

- `Open5GS`: `v2.7.7`
- `srsRAN_Project`: `release_24_10_1`
- `srsRAN_4G`: `release_23_11`
- `UHD`: `4.8.0.0`

## 3. 默认网络与无线参数

当前模板里的默认参数是：

- `PLMN`: `00101`
- `TAC`: `1`
- `SST`: `1`
- `Band`: `n3`
- `DL ARFCN`: `368500`
- `SSB ARFCN`: `368410`
- `PCI`: `1`

当前默认空口组合是偏向 `srsUE` 兼容的组合，但你可以通过环境变量覆盖：

- `GNB` 默认采样率来自 [start-srsran-gnb.sh](testbed/scripts/start-srsran-gnb.sh)
- `UE` 默认采样率来自 [start-srsue.sh](testbed/scripts/start-srsue.sh)

## 4. 启动前准备

### 4.1 检查 X310 和 B210

建议先在宿主机执行：

```bash
uhd_find_devices
uhd_usrp_probe --args 'type=x300,addr=192.168.40.2'
uhd_usrp_probe --args 'type=b200'
ls -l /dev/net/tun
```

如果 `X310` 不是 `192.168.40.2`，后面启动时要覆盖 `X310_ADDR`。

### 4.2 检查 Docker

```bash
docker --version
docker compose version
```

### 4.3 明确你的射频连接方式

你要先确认自己是哪一种：

- 空口天线互通
- 同轴直连加衰减器

如果是同轴直连，必须有合适衰减，不能裸直连。

## 5. 默认订阅用户

默认 UE 参数保存在 [testbed/subscribers/test-ue.env](testbed/subscribers/test-ue.env)。

当前默认值：

```env
IMSI=001010123456789
K=00112233445566778899aabbccddeeff
OPC=63BFA50EE6523365FF14C1F45F88737D
APN=internet
MCC=001
MNC=01
SST=1
SD=000001
TAC=1
```

`Open5GS` 启动时会自动把这组用户写进数据库。

手工再次写入：

```bash
./testbed/scripts/add-open5gs-subscriber.sh add-default
```

查看当前订阅用户：

```bash
./testbed/scripts/add-open5gs-subscriber.sh showfiltered
```

## 6. 如何构建镜像

在仓库根目录执行：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml build
```

如果你只想重建某一个组件：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml build open5gs
docker compose -f testbed/docker-compose.5g-testbed.yml build srsran_gnb
docker compose -f testbed/docker-compose.5g-testbed.yml build srsue_b210
```

先检查 compose 展开结果也很有用：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml config
```

## 7. 如何启动整套实验网

### 7.1 一次性启动全部组件

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml up -d
```

这会启动：

- `MongoDB`
- `Open5GS`
- `srsRAN gNB`
- `srsUE`

### 7.2 分阶段启动

如果你想更稳地观察启动过程，建议按下面顺序：

先启动数据库和核心网：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml up -d mongodb open5gs
```

再启动基站：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml up -d srsran_gnb
```

最后启动 UE：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml up -d srsue_b210
```

### 7.3 查看容器状态

```bash
docker ps --format 'table {{.Names}}\t{{.Status}}\t{{.Image}}' | grep '^testbed-'
```

正常情况下你应该看到：

- `testbed-mongodb`
- `testbed-open5gs`
- `testbed-srsran-gnb`
- `testbed-srsue-b210`

## 8. 如何确认核心网和基站已经起来

### 8.1 看 Open5GS 是否启动

```bash
docker logs --tail 100 testbed-open5gs
```

本地文件日志：

- [open5gs.log](testbed/logs/open5gs/open5gs.log)

### 8.2 看 gNB 是否连上 AMF

```bash
docker logs --tail 100 testbed-srsran-gnb
rg -n "SCTP connection to AMF|NG Setup|Connected to AMF" testbed/logs/srsran/gnb.log
```

如果成功，你会看到类似：

- `SCTP connection to AMF ... established`
- `NG Setup Procedure finished successfully`
- `Connected to AMF`

关键文件：

- [gnb.log](testbed/logs/srsran/gnb.log)

## 9. 如何确认 UE 是否开始接入

查看 UE 容器日志：

```bash
docker logs --tail 100 testbed-srsue-b210
```

查看 UE 文件日志：

```bash
tail -f testbed/logs/srsue/ue.log
```

关键文件：

- [ue.log](testbed/logs/srsue/ue.log)
- [ue-console.log](testbed/logs/srsue/ue-console.log)

你通常会看到这些阶段：

- `Sending Registration Request`
- `Cell Search`
- `Cell Selection`
- 如果真的接入成功，后面才会看到更进一步的接入过程

## 10. 如何重启单个组件

重启核心网：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml restart open5gs
```

重启基站：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml restart srsran_gnb
```

重启 UE：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml restart srsue_b210
```

## 11. 如何停止实验网

停止但保留容器：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml stop
```

完全停止并移除容器：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml down
```

如果你还想连卷一起删：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml down -v
```

## 12. 运行时可覆盖的关键环境变量

### 12.1 gNB

```bash
X310_ADDR=192.168.40.2
X310_CLOCK=internal
X310_SYNC=internal
GNB_SRATE=23.04
GNB_TX_GAIN=45
GNB_RX_GAIN=35
NR_DL_ARFCN=368500
NR_BW_MHZ=20
```

### 12.2 UE

```bash
B210_SERIAL=
B210_MASTER_CLOCK=23.04e6
B210_TX_GAIN=80
B210_RX_GAIN=20
UE_SRATE=23.04e6
UE_NOF_PRB=106
SSB_NR_ARFCN=368410
```

### 12.3 Open5GS NAT

如果你要让 UE 出口走宿主机网卡：

```bash
ENABLE_NAT=true
WAN_IFACE=eth0
```

### 12.4 覆盖方式示例

例如：

```bash
X310_ADDR=192.168.40.2 \
GNB_TX_GAIN=50 \
B210_RX_GAIN=15 \
docker compose -f testbed/docker-compose.5g-testbed.yml up -d
```

## 13. 日志和抓包文件在哪

### 13.1 Open5GS

- [testbed/logs/open5gs](testbed/logs/open5gs)

### 13.2 gNB

- [testbed/logs/srsran/gnb.log](testbed/logs/srsran/gnb.log)
- [testbed/logs/srsran/gnb-console.log](testbed/logs/srsran/gnb-console.log)
- [testbed/logs/srsran/gnb_mac.pcap](testbed/logs/srsran/gnb_mac.pcap)
- [testbed/logs/srsran/gnb_ngap.pcap](testbed/logs/srsran/gnb_ngap.pcap)

### 13.3 UE

- [testbed/logs/srsue/ue.log](testbed/logs/srsue/ue.log)
- [testbed/logs/srsue/ue-console.log](testbed/logs/srsue/ue-console.log)
- [testbed/logs/srsue/ue_mac_nr.pcap](testbed/logs/srsue/ue_mac_nr.pcap)
- [testbed/logs/srsue/ue_nas.pcap](testbed/logs/srsue/ue_nas.pcap)

## 14. 建议的最小使用流程

在根目录依次执行：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml build
docker compose -f testbed/docker-compose.5g-testbed.yml up -d mongodb open5gs
docker compose -f testbed/docker-compose.5g-testbed.yml up -d srsran_gnb
docker compose -f testbed/docker-compose.5g-testbed.yml up -d srsue_b210
```

然后开 3 个窗口分别看：

```bash
tail -f testbed/logs/open5gs/open5gs.log
tail -f testbed/logs/srsran/gnb.log
tail -f testbed/logs/srsue/ue.log
```

你要优先观察：

- `gNB` 是否成功连上 `AMF`
- `UE` 是否只停在 `Cell Selection`
- `Open5GS` 是否收到真正的 UE 注册信令

## 15. 常见问题定位思路

### 15.1 `gNB` 没连上 `AMF`

先查：

- `testbed/logs/open5gs/open5gs.log`
- `testbed/logs/srsran/gnb.log`

重点看：

- AMF 监听地址
- `127.0.0.5:38412` 是否一致
- `NG Setup` 是否成功

### 15.2 `UE` 一启动就失败

先查：

- `testbed/logs/srsue/ue.log`

重点看：

- `Cell Search`
- `Cell Selection`
- `Overflow`
- `out-of-sync`
- `PBCH-MIB`

### 15.3 `gNB` 和 `Open5GS` 正常，但 UE 不注册

这通常不是核心网脚本问题，而是无线链路问题。重点查：

- 天线还是同轴直连
- 是否加衰减
- `B210` 接收是否过载
- 时钟和采样率是否匹配
- 当前 `ARFCN / SSB / PRB / BW / coreset0` 是否和 `srsUE` 兼容

## 16. 你最常用的几个文件

- [testbed/docker-compose.5g-testbed.yml](testbed/docker-compose.5g-testbed.yml)
- [testbed/scripts/start-open5gs.sh](testbed/scripts/start-open5gs.sh)
- [testbed/scripts/start-srsran-gnb.sh](testbed/scripts/start-srsran-gnb.sh)
- [testbed/scripts/start-srsue.sh](testbed/scripts/start-srsue.sh)
- [testbed/configs/open5gs/open5gs.yml](testbed/configs/open5gs/open5gs.yml)
- [testbed/configs/srsran/gnb.yml.tpl](testbed/configs/srsran/gnb.yml.tpl)
- [testbed/configs/srsue/ue.conf.tpl](testbed/configs/srsue/ue.conf.tpl)
- [testbed/subscribers/test-ue.env](testbed/subscribers/test-ue.env)
