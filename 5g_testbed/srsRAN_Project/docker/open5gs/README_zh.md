这是一个用于 Open5GS 的 all-in-one Docker container。在构建时，container 会使用指定版本的 open5gs repository（默认 `v2.6.6`）。
如果要运行 open5gs repository 的最新 tag（<https://github.com/open5gs/open5gs/tags>），需要取消 `.Dockerfile` 第 51 和 52 行的注释：
```
# get latest open5gs tag (must be stored in a file, because docker does not allow to use the return value directly)
# RUN git ls-remote --tags https://github.com/open5gs/open5gs | sort -t '/' -k 3 -V | awk -F/ '{ print $3 }' | awk '!/\^\{\}/' | tail -n 1 > ./open5gsversion
```


# Container Parameters

在 [open5gs.env](open5gs.env) 中，可以设置以下参数：

- `MONGODB_IP`（默认：127.0.0.1）：要使用的 mongodb 的 IP。`127.0.0.1` 表示使用在该 container 内运行的 mongodb。
- `SUBSCRIBER_DB`（默认：`"001010123456780,00112233445566778899aabbccddeeff,opc,63bfa50ee6523365ff14c1f45f88737d,8000,10.45.1.2"`）：向 Open5GS mongodb 中添加一个或多个 subscriber 的数据。它可以是以下两种形式之一：
  - 用逗号分隔的字符串，包含定义一个 subscriber 所需的信息
  - `subscriber_db.csv`。这是一个包含条目的 csv 文件，用于添加到 open5gs mongodb。每个条目代表一个 subscriber。该文件必须存放在 `docker/open5gs/` 中
- `OPEN5GS_IP`：必须设置为该 container 的 IP（此处为：`10.53.1.2`）。
- `UE_IP_BASE`：定义已连接 UE 使用的 IP 基址（此处为：`10.45.0`）。
- `DEBUG`（默认：false）：可设置为 true，以 debug mode 运行 Open5GS。

```
# Kept in the following format: "Name,IMSI,Key,OP_Type,OP/OPc,AMF,QCI,IP_alloc"
#
# Name:     Human readable name to help distinguish UE's. Ignored by the HSS
# IMSI:     UE's IMSI value
# Key:      UE's key, where other keys are derived from. Stored in hexadecimal
# OP_Type:  Operator's code type, either OP or OPc
# OP/OPc:   Operator Code/Cyphered Operator Code, stored in hexadecimal
# AMF:      Authentication management field, stored in hexadecimal
# QCI:      QoS Class Identifier for the UE's default bearer.
# IP_alloc: IP allocation stratagy for the SPGW.
#           With 'dynamic' the SPGW will automatically allocate IPs
#           With a valid IPv4 (e.g. '10.45.0.2') the UE will have a statically assigned IP.
#
# Note: Lines starting by '#' are ignored and will be overwritten
# List of UEs with IMSI, and key increasing by one for each new UE. Useful for testing with AmariUE simulator and ue_count option
ue01,001010123456789,00112233445566778899aabbccddeeff,opc,63bfa50ee6523365ff14c1f45f88737d,9001,9,10.45.1.2
ue02,001010123456790,00112233445566778899aabbccddef00,opc,63bfa50ee6523365ff14c1f45f88737d,9001,9,10.45.2.2
ue03,001010123456791,00112233445566778899aabbccddef01,opc,63bfa50ee6523365ff14c1f45f88737d,9001,9,10.45.3.2
```

# Open5GS Parameters

可以通过 [open5gs-5gc.yml](open5gs-5gc.yml) 配置 Open5GS。

# Usage

创建一个 Docker network，以便为 Open5GS conainer 分配指定 IP（此处为：`10.53.1.2`）：

`docker network create --subnet=10.53.1.0/16 open5gsnet`

使用以下命令构建 Docker container：

`docker build --target open5gs -t open5gs-docker .`

你可以通过添加 `--build-arg OPEN5GS_VERSION=v2.6.6` 来覆盖 open5gs 版本。

然后使用以下命令运行 Docker container：

`docker run --net open5gsnet --ip 10.53.1.2 --env-file open5gs.env --privileged --publish 9999:9999 open5gs-docker ./build/tests/app/5gc -c open5gs-5gc.yml`

若要将该 container 与 srsgnb 一起使用，需要将 gnb 配置中 `amf` 部分下的 `addr` 选项设置为 `OPEN5GS_IP`（此处为：`10.53.1.2`）。
还可能需要将 gnb 配置中 `amf` 部分下的 `bind_addr` 选项修改为运行 gnb 的主机或 container 的本地 ethernet/wifi IP 地址，而不是 localhost IP。

若要 ping 已连接的 UE，需要将到 `UE_IP_BASE + ".0/24"`（此处为：`10.45.0`）的必要 route 通过 `OPEN5GS_IP`（此处为：`10.53.1.2`）进行设置，命令如下：

`sudo ip ro add 10.45.0.0/16 via 10.53.1.2`

## Note

Open5GS WebUI 用于手动向 mongodb 添加/修改 UE，其访问地址为 [localhost:9999](localhost:9999)。
