# Local Source Mounts

`docker compose` will mount these host directories into the `5g_testbed` container:

- `testbed/sources/open5gs` -> `/opt/src/open5gs`
- `testbed/sources/srsRAN_Project` -> `/opt/src/srsRAN_Project`
- `testbed/sources/srsRAN_4G` -> `/opt/src/srsRAN_4G`

Clone the expected revisions here before starting the container, for example:

```bash
git clone --branch v2.7.7 https://github.com/open5gs/open5gs.git testbed/sources/open5gs
git clone --branch release_24_10_1 https://github.com/srsRAN/srsRAN_Project.git testbed/sources/srsRAN_Project
git clone --branch release_23_11 https://github.com/srsran/srsRAN_4G.git testbed/sources/srsRAN_4G
```
