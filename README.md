# 5G SA Testbed Usage Guide

这个仓库里和自建 5G SA 实验网直接相关的目录现在是 [testbed](testbed)。

这套环境的目标是：

- 用官方 `Open5GS` 搭 5G SA 核心网
- 用官方 `srsRAN_Project` 跑 `gNB`
- 用官方 `srsRAN_4G` 跑 `srsUE`
- 通过 Docker 在同一台主机上统一构建、进入容器、查看日志和排障
- 当前 `testbed` 提供一个单容器实验环境，容器内已经具备 `MongoDB + Open5GS + gNB + srsUE` 的所有依赖，服务由你手动启动

当前默认硬件假设：

- 宿主机已经连接 `USRP X310`
- 宿主机已经连接 `USRP B210`
- 宿主机有 `/dev/net/tun`
- 宿主机已经安装 Docker 和 Docker Compose 插件

## 1. 目录说明

- [testbed](testbed)
  这是实验网主目录。
- [testbed/docker-compose.5g-testbed.yml](testbed/docker-compose.5g-testbed.yml)
  这是整套单容器实验网的编排入口。
- [testbed/docker](testbed/docker)
  这里放当前单容器方案使用的 Dockerfile。
- [testbed/scripts](testbed/scripts)
  这里放辅助脚本和可选的配置生成脚本。
- [testbed/configs/open5gs/open5gs.yml](testbed/configs/open5gs/open5gs.yml)
  这是 Open5GS 配置。
- [testbed/configs/srsran/gnb.yml.tpl](testbed/configs/srsran/gnb.yml.tpl)
  这是 gNB 配置模板。
- [testbed/configs/srsue/ue.conf.tpl](testbed/configs/srsue/ue.conf.tpl)
  这是 UE 配置模板。
- [testbed/subscribers/test-ue.env](testbed/subscribers/test-ue.env)
  这是默认测试 UE 的 IMSI、K、OPC、APN。
- [testbed/runtime](testbed/runtime)
  这里放手工生成或手工维护的实际运行配置。
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

当前默认空口组合是偏向 `srsUE` 兼容的组合。

当前版本的推荐使用方式是：

- `docker compose` 只负责启动一个带完整依赖的开发容器
- 你手动 `attach shell` 进入容器
- 你在命令行里直接指定 `Open5GS`、`gNB`、`srsUE` 要使用的配置文件路径

## 4. 启动前准备

### 4.1 检查 X310 和 B210

建议先在宿主机执行：

```bash
uhd_find_devices
uhd_usrp_probe --args 'type=x300,addr=192.168.40.2'
uhd_usrp_probe --args 'type=b200'
ls -l /dev/net/tun
```

如果 `X310` 不是 `192.168.40.2`，直接修改你准备传给 `gNB` 的配置文件即可，不需要改 compose。

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

当前单容器方案不会自动写入这组用户，需要你在容器里手动执行写库命令。

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

先检查 compose 展开结果也很有用：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml config
```

## 7. 如何启动单容器环境

### 7.1 启动容器

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml up -d
```

这会启动一个容器环境：

- `5g_testbed`

### 7.2 进入容器

```bash
docker exec -it 5g_testbed bash
```

### 7.3 查看容器状态

```bash
docker ps --format 'table {{.Names}}\t{{.Status}}\t{{.Image}}' | grep '^5g_testbed$'
```

正常情况下你应该看到：

- `5g_testbed`

## 8. 在容器里手动启动各组件

下面的命令都在容器内执行。

### 8.1 启动 MongoDB

```bash
mkdir -p /var/lib/mongodb /workspace/testbed/logs/open5gs /var/run/mongodb
mongod --dbpath /var/lib/mongodb \
  --bind_ip 127.0.0.1 \
  --port 27018 \
  --logpath /workspace/testbed/logs/open5gs/mongod.log \
  --logappend --fork
```

### 8.2 启动 Open5GS

如果你要用默认配置：

```bash
ip tuntap add name ogstun mode tun 2>/dev/null || true
ip addr replace 10.45.0.1/16 dev ogstun
ip link set ogstun up
ip -6 addr replace 2001:db8:cafe::1/48 dev ogstun nodad || true
sysctl -w net.ipv4.ip_forward=1
sysctl -w net.ipv6.conf.all.forwarding=1 || true

/opt/src/open5gs/misc/db/open5gs-dbctl --db_uri=mongodb://127.0.0.1:27018/open5gs \
  add_ue_with_apn 001010123456789 00112233445566778899aabbccddeeff 63BFA50EE6523365FF14C1F45F88737D internet

/opt/src/open5gs/build/src/nrf/open5gs-nrfd -c /workspace/testbed/configs/open5gs/open5gs.yml &
/opt/src/open5gs/build/src/amf/open5gs-amfd -c /workspace/testbed/configs/open5gs/open5gs.yml &
/opt/src/open5gs/build/src/smf/open5gs-smfd -c /workspace/testbed/configs/open5gs/open5gs.yml &
/opt/src/open5gs/build/src/upf/open5gs-upfd -c /workspace/testbed/configs/open5gs/open5gs.yml &
/opt/src/open5gs/build/src/ausf/open5gs-ausfd -c /workspace/testbed/configs/open5gs/open5gs.yml &
/opt/src/open5gs/build/src/udm/open5gs-udmd -c /workspace/testbed/configs/open5gs/open5gs.yml &
/opt/src/open5gs/build/src/udr/open5gs-udrd -c /workspace/testbed/configs/open5gs/open5gs.yml &
/opt/src/open5gs/build/src/pcf/open5gs-pcfd -c /workspace/testbed/configs/open5gs/open5gs.yml &
/opt/src/open5gs/build/src/nssf/open5gs-nssfd -c /workspace/testbed/configs/open5gs/open5gs.yml &
```

如果你要用别的 Open5GS 配置文件，直接把 `-c` 后面的路径改掉即可。

### 8.3 启动 gNB

你可以直接让 `gNB` 读取你指定的配置文件：

```bash
/opt/src/srsRAN_Project/build/apps/gnb/gnb \
  -c /workspace/testbed/runtime/gnb.yml
```

如果你想换别的配置文件：

```bash
/opt/src/srsRAN_Project/build/apps/gnb/gnb \
  -c /workspace/your/path/to/gnb.yml
```

### 8.4 启动 srsUE

```bash
chrt -f 20 taskset -c 2-5 \
  /opt/src/srsRAN_4G/build/srsue/src/srsue \
  /workspace/testbed/runtime/ue.conf
```

如果你想换别的 UE 配置文件：

```bash
chrt -f 20 taskset -c 2-5 \
  /opt/src/srsRAN_4G/build/srsue/src/srsue \
  /workspace/your/path/to/ue.conf
```

### 8.5 如何生成你自己的运行配置

如果你想继续沿用模板生成方式，也可以手工执行：

```bash
/workspace/testbed/scripts/start-srsran-gnb.sh
/workspace/testbed/scripts/start-srsue.sh
```

但更推荐你直接维护自己的配置文件，然后在命令里显式指定路径。

## 9. 如何确认核心网和基站已经起来

### 9.1 看 Open5GS 是否启动

```bash
tail -f /workspace/testbed/logs/open5gs/open5gs.log
```

本地文件日志：

- [open5gs.log](testbed/logs/open5gs/open5gs.log)

### 9.2 看 gNB 是否连上 AMF

```bash
rg -n "SCTP connection to AMF|NG Setup|Connected to AMF" testbed/logs/srsran/gnb.log
```

如果成功，你会看到类似：

- `SCTP connection to AMF ... established`
- `NG Setup Procedure finished successfully`
- `Connected to AMF`

关键文件：

- [gnb.log](testbed/logs/srsran/gnb.log)

## 10. 如何确认 UE 是否开始接入

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

## 12. 日志和抓包文件在哪

### 12.1 Open5GS

- [testbed/logs/open5gs](testbed/logs/open5gs)

### 12.2 gNB

- [testbed/logs/srsran/gnb.log](testbed/logs/srsran/gnb.log)
- [testbed/logs/srsran/gnb-console.log](testbed/logs/srsran/gnb-console.log)
- [testbed/logs/srsran/gnb_mac.pcap](testbed/logs/srsran/gnb_mac.pcap)
- [testbed/logs/srsran/gnb_ngap.pcap](testbed/logs/srsran/gnb_ngap.pcap)

### 12.3 UE

- [testbed/logs/srsue/ue.log](testbed/logs/srsue/ue.log)
- [testbed/logs/srsue/ue-console.log](testbed/logs/srsue/ue-console.log)
- [testbed/logs/srsue/ue_mac_nr.pcap](testbed/logs/srsue/ue_mac_nr.pcap)
- [testbed/logs/srsue/ue_nas.pcap](testbed/logs/srsue/ue_nas.pcap)

## 13. 建议的最小使用流程

在根目录依次执行：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml build
docker compose -f testbed/docker-compose.5g-testbed.yml up -d
```

然后进入容器并按需要手动启动 `MongoDB`、`Open5GS`、`gNB`、`srsUE`。你也可以另外开窗口看日志：

```bash
tail -f testbed/logs/open5gs/open5gs.log
tail -f testbed/logs/srsran/gnb.log
tail -f testbed/logs/srsue/ue.log
```

你要优先观察：

- `gNB` 是否成功连上 `AMF`
- `UE` 是否只停在 `Cell Selection`
- `Open5GS` 是否收到真正的 UE 注册信令

## 14. 常见问题定位思路

### 14.1 `gNB` 没连上 `AMF`

先查：

- `testbed/logs/open5gs/open5gs.log`
- `testbed/logs/srsran/gnb.log`

重点看：

- AMF 监听地址
- `127.0.0.5:38412` 是否一致
- `NG Setup` 是否成功

### 14.2 `UE` 一启动就失败

先查：

- `testbed/logs/srsue/ue.log`

重点看：

- `Cell Search`
- `Cell Selection`
- `Overflow`
- `out-of-sync`
- `PBCH-MIB`

### 14.3 `gNB` 和 `Open5GS` 正常，但 UE 不注册

这通常不是核心网脚本问题，而是无线链路问题。重点查：

- 天线还是同轴直连
- 是否加衰减
- `B210` 接收是否过载
- 时钟和采样率是否匹配
- 当前 `ARFCN / SSB / PRB / BW / coreset0` 是否和 `srsUE` 兼容

## 15. 你最常用的几个文件

- [testbed/docker-compose.5g-testbed.yml](testbed/docker-compose.5g-testbed.yml)
- [testbed/scripts/start-open5gs.sh](testbed/scripts/start-open5gs.sh)
- [testbed/scripts/start-srsran-gnb.sh](testbed/scripts/start-srsran-gnb.sh)
- [testbed/scripts/start-srsue.sh](testbed/scripts/start-srsue.sh)
- [testbed/configs/open5gs/open5gs.yml](testbed/configs/open5gs/open5gs.yml)
- [testbed/configs/srsran/gnb.yml.tpl](testbed/configs/srsran/gnb.yml.tpl)
- [testbed/configs/srsue/ue.conf.tpl](testbed/configs/srsue/ue.conf.tpl)
- [testbed/subscribers/test-ue.env](testbed/subscribers/test-ue.env)
