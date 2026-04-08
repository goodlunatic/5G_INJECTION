## 启动Open5GS、gNB和srsUE

```bash
git clone https://github.com/goodlunatic/5G_INJECTION.git
# 构建容器
cd 5G_INJECTION
docker compose up -d

# 启动srsRAN_Project中自带的Open5GS
cd srsRAN_Project/docker
docker compose up --build 5gc

# 编译并启动gNB
cd srsRAN_Project
mkdir build && cd build
cmake .. && make -j $(nproc)
./apps/gnb/gnb -c /home/workspace/srsRAN_Project/configs/gnb_rf_b210_fdd_srsUE.yml

# 编译并启动srsUE
cd srsRAN_4G/
mkdir build && cd build
cmake .. && make -j $(nproc)
```