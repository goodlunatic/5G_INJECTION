# 5G SA Testbed Usage Guide

这个仓库当前和自建 5G SA 实验网直接相关的目录是 `testbed/`。

当前项目已经整理为一套以 Docker 为中心的 5G SA 实验网：

- `Open5GS` 在独立容器里运行
- `gNB` 在独立容器里运行
- `srsUE` 在独立容器里运行
- `gnb` 和 `srsUE` 的编译也在容器里完成

也就是说，正常使用流程不再依赖“先在宿主机编译，再在宿主机直接启动 gNB/UE”。

## 1. 当前目录结构

- `testbed/docker-compose.5g-testbed.yml`
  整套实验网的 Compose 文件，定义了 `radio_builder`、`5gc`、`srsran_gnb`、`srsue_b210` 四个服务。
- `testbed/docker/Dockerfile.allinone`
  `radio_builder`、`gNB`、`srsUE` 共用的镜像构建文件。
- `testbed/scripts/build-radio-binaries.sh`
  在 `radio_builder` 容器中编译 `srsRAN_Project gnb` 和 `srsRAN_4G srsUE`。
- `testbed/scripts/start-open5gs.sh`
  渲染 `runtime/open5gs.env`，启动 `Open5GS` 容器，并给宿主机补 UE 网段路由。
- `testbed/scripts/start-srsran-gnb.sh`
  在 `gNB` 容器里渲染 `runtime/gnb.yml` 并启动 `gnb`。
- `testbed/scripts/start-srsue.sh`
  在 `srsUE` 容器里渲染 `runtime/ue.conf` 并启动 `srsUE`。
- `testbed/scripts/start-testbed.sh`
  一键完成“容器内编译 + 启动 Open5GS + 启动 gNB + 启动 srsUE”。
- `testbed/scripts/add-open5gs-subscriber.sh`
  往运行中的 `Open5GS` 容器里写默认用户，或查看当前用户。
- `testbed/configs/srsran/gnb.yml.tpl`
  `gNB` 配置模板。
- `testbed/configs/srsue/ue.conf.tpl`
  `srsUE` 配置模板。
- `testbed/subscribers/test-ue.env`
  默认 UE 用户参数。
- `testbed/runtime/`
  运行时渲染出的配置文件目录。
- `testbed/logs/`
  运行日志目录。

## 2. 当前默认参数

- `PLMN`: `00101`
- `TAC`: `7`
- `SST`: `1`
- `Band`: `n3`
- `DL ARFCN`: `368500`
- `SSB NR ARFCN`: `368410`
- `PCI`: `1`
- `Open5GS IP`: `10.53.1.2`
- `RAN Docker subnet`: `10.53.1.0/24`
- `UE subnet`: `10.45.0.0/24`

这些值目前已经和项目里的 `Open5GS` 配置、`gNB` 模板和 UE 默认用户保持一致。

## 3. 硬件和环境要求

默认假设：

- 宿主机连接一台 `USRP X310`，供 `gNB` 使用
- 宿主机连接一台 `USRP B210`，供 `srsUE` 使用
- 宿主机存在 `/dev/net/tun`
- 宿主机可以访问 USRP 设备
- 宿主机安装了 `Docker` 和 `Docker Compose`
- 宿主机允许脚本执行 `sudo ip route replace ...`

如果你要先确认设备是否可见，可以在宿主机执行：

```bash
uhd_find_devices
```

## 4. 准备源码

如果 `testbed/sources/` 下还没有源码，在仓库根目录执行：

```bash
git clone --branch release_24_10_1 https://github.com/srsRAN/srsRAN_Project.git testbed/sources/srsRAN_Project
git clone --branch release_23_11 https://github.com/srsran/srsRAN_4G.git testbed/sources/srsRAN_4G
git clone --branch v2.7.7 https://github.com/open5gs/open5gs.git testbed/sources/open5gs
```

说明：

- `Open5GS` 运行容器实际使用的是 `testbed/sources/srsRAN_Project/docker/open5gs`
- `gNB` 用的是 `testbed/sources/srsRAN_Project`
- `srsUE` 用的是 `testbed/sources/srsRAN_4G`

## 5. 默认用户

默认测试用户在：

- `testbed/subscribers/test-ue.env`

当前默认值：

```bash
IMSI=001010123456789
K=00112233445566778899aabbccddeeff
OPC=63BFA50EE6523365FF14C1F45F88737D
APN=internet
MCC=001
MNC=01
TAC=7
SST=1
```

如果你换卡或者想改测试用户，先改这个文件，再重新启动核心网和 UE。

## 6. 容器内编译

项目现在推荐只用容器来编译：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml run --rm radio_builder
```

这一步会在以下目录生成二进制：

- `testbed/sources/srsRAN_Project/build-docker/apps/gnb/gnb`
- `testbed/sources/srsRAN_4G/build-docker/srsue/src/srsue`

如果你只改了配置，没有改源码，通常不需要重复编译。

## 7. 如何启动 Open5GS

只启动核心网：

```bash
./testbed/scripts/start-open5gs.sh
```

这一步会做三件事：

1. 根据模板生成 `testbed/runtime/open5gs.env`
2. 启动 `testbed-open5gs` 容器
3. 在宿主机上添加 UE 网段路由 `10.45.0.0/24 -> 10.53.1.2`

启动后查看状态：

```bash
docker ps
docker logs -f testbed-open5gs
```

当前 WebUI 默认映射到：

```text
http://127.0.0.1:9999
```

如果你只想重新写默认用户，可以执行：

```bash
./testbed/scripts/add-open5gs-subscriber.sh add-default
```

查看当前用户：

```bash
./testbed/scripts/add-open5gs-subscriber.sh showfiltered
```

## 8. 如何启动 gNB

在确保 `Open5GS` 已经启动且健康之后，启动 `gNB`：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml up -d srsran_gnb
```

查看 `gNB` 日志：

```bash
docker logs -f testbed-srsran-gnb
tail -f testbed/logs/srsran/gnb.log
```

`gNB` 运行时会自动生成：

- `testbed/runtime/gnb.yml`

当前关键环境变量来自 `testbed/docker-compose.5g-testbed.yml`，默认包括：

- `OPEN5GS_IP=10.53.1.2`
- `GNB_BIND_ADDR=10.53.1.1`
- `X310_ADDR=192.168.40.2`
- `GNB_TX_GAIN=31.5`
- `GNB_RX_GAIN=31.5`
- `NR_DL_ARFCN=368500`
- `NR_BW_MHZ=20`

如果你要调整 `gNB` 参数，优先改：

- `testbed/configs/srsran/gnb.yml.tpl`
- 或 `testbed/docker-compose.5g-testbed.yml` 里的环境变量默认值

## 9. 如何启动 srsUE

在确保 `Open5GS` 和 `gNB` 都已经起来之后，启动 `srsUE`：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml up -d srsue_b210
```

查看 `srsUE` 日志：

```bash
docker logs -f testbed-srsue-b210
tail -f testbed/logs/srsue/ue.log
```

`srsUE` 运行时会自动生成：

- `testbed/runtime/ue.conf`

当前关键环境变量来自 `testbed/docker-compose.5g-testbed.yml`，默认包括：

- `B210_CLOCK_SOURCE=external`
- `B210_TIME_SOURCE=external`
- `B210_TX_GAIN=80`
- `B210_RX_GAIN=40`
- `NR_DL_ARFCN=368500`
- `SSB_NR_ARFCN=368410`

如果你要调整 UE 参数，优先改：

- `testbed/configs/srsue/ue.conf.tpl`
- `testbed/subscribers/test-ue.env`
- 或 `testbed/docker-compose.5g-testbed.yml` 中 `srsue_b210` 的环境变量

## 10. 一键启动整套实验网

如果你希望一次完成“编译 + Open5GS + gNB + srsUE”，直接执行：

```bash
./testbed/scripts/start-testbed.sh
```

这个脚本等价于：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml run --rm radio_builder
./testbed/scripts/start-open5gs.sh
docker compose -f testbed/docker-compose.5g-testbed.yml up -d srsran_gnb srsue_b210
```

## 11. 常用检查命令

查看容器状态：

```bash
docker ps -a
```

查看最近日志：

```bash
docker logs --tail 200 testbed-open5gs
docker logs --tail 200 testbed-srsran-gnb
docker logs --tail 200 testbed-srsue-b210
```

查看运行时渲染配置：

```bash
sed -n '1,200p' testbed/runtime/open5gs.env
sed -n '1,200p' testbed/runtime/gnb.yml
sed -n '1,220p' testbed/runtime/ue.conf
```

查看宿主机 UE 路由：

```bash
ip route | grep 10.45.0.0/24
```

## 12. 停止、重启和清理

停止整套实验网：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml down
```

只重启 `gNB`：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml up -d --force-recreate srsran_gnb
```

只重启 `srsUE`：

```bash
docker compose -f testbed/docker-compose.5g-testbed.yml up -d --force-recreate srsue_b210
```

重新编译再启动：

```bash
./testbed/scripts/start-testbed.sh
```

## 13. 当前已知状态

当前这套容器化编排已经验证过以下几点：

- `radio_builder` 可正常容器内编译
- `Open5GS` 可正常启动
- `gNB` 可正常启动并连上 `AMF`
- `srsUE` 可正常启动并搜到小区、解出 `MIB/SIB1`

当前剩余问题不在 Docker 编排，而在空口上行链路：

- `PRACH` 能到达 `gNB`
- `RAR` 能正常下发
- 但 `Msg3/PUSCH` 仍可能因为上行链路质量问题而失败

因此，如果后续出现“核心网和 gNB 都起来了，但 UE 仍未完成注册”，优先检查：

- `B210/X310` 的射频连线
- 外部时钟和时间同步
- 天线、衰减器、隔离度
- `TX/RX gain`
- `time_source` / `clock_source`

## 14. 相关说明

- `testbed/README.md` 只是 `testbed/` 目录的简短入口说明
- 以当前仓库根目录这份 `README.md` 为准
