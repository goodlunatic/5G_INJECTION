# 5G SA Testbed

`testbed/` 是这套 5G SA 实验网的实现目录。

当前结构：

- `Open5GS` 独立容器
- `gNB` 独立容器
- `srsUE` 独立容器
- `radio_builder` 负责容器内编译

主要入口：

- `testbed/docker-compose.5g-testbed.yml`
- `testbed/scripts/start-open5gs.sh`
- `testbed/scripts/start-testbed.sh`

详细使用说明见仓库根目录 [README.md](../README.md)。
