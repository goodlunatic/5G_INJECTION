# Reference Configs

This directory contains reference configuration files copied from:

- `sni5gect_100mhz_n41/configs/base_station`

They are kept here only for study and comparison. The default testbed startup
path does not read anything from this directory.

The active files used by the testbed are:

- `testbed/configs/open5gs/open5gs.yml`
- `testbed/configs/srsran/gnb.yml.tpl`
- `testbed/configs/srsue/ue.conf.tpl`

If you want to switch the testbed to another band or cell profile, use these
reference files as parameter guides, then update the active templates or the
environment variables consumed by the launch scripts.
