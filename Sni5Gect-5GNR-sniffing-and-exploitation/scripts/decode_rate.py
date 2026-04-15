#!/usr/bin/env python3
"""
SNI5GECT vs gNB 解码成功率分析脚本

直接运行:
    python3 scripts/decode_rate.py

修改下方 "── 配置 ──" 区域中的路径即可切换数据集。
"""

import struct
import os
import re
import sys
import bisect
from collections import defaultdict
from prettytable import PrettyTable


# ══════════════════════════════════════════════════════════════════════════════
#  配置区：修改这里的路径即可，无需命令行参数
# ══════════════════════════════════════════════════════════════════════════════

# SNI5GECT 保存 pcap 的目录（含 UE-XXXXX.pcap 文件）
SNI5GECT_DIR = "logs/"

# gNB 侧导出的 MAC pcap 文件路径
GNB_PCAP = "/home/ubuntu/workarea/git_repo/5G_INJECTION/5g_testbed/srsRAN_Project/logs/gnb_mac.pcap"

# 报告输出文件路径（None 表示只打印到终端，不写文件）
OUTPUT_FILE = "logs/decode_rate_report.txt"

# 时槽匹配容差：允许 SNI5GECT 与 gNB 的时槽号相差 ±SLOT_WINDOW 个时槽
# 5G NR 30kHz SCS 下 1 个时槽 = 0.5ms，±2 = ±1ms 的时钟误差容忍
SLOT_WINDOW = 2

# ══════════════════════════════════════════════════════════════════════════════


# ─── MAC-NR pcap 协议常量 ─────────────────────────────────────────────────────
#
# srsRAN 将 MAC PDU 写入 pcap 时使用自定义的 "mac-nr" UDP 封装格式：
#   UDP src_port = 0xBEEF, dst_port = 0xDEAD  （用于识别这是 mac-nr 包）
#   UDP 载荷格式：
#     [6B] 魔数 "mac-nr"
#     [1B] radioType  （1 = NR）
#     [1B] direction  （0 = 上行 UL，1 = 下行 DL）
#     [1B] rntiType   （2 = RA-RNTI，3 = C-RNTI，...）
#     [TLV 字段...] 各字段由 1 字节 tag 标识，长度固定（见下方常量）
#     最后以 TAG_PAYLOAD(0x01) 结尾，之后是 MAC PDU 原始字节

MAC_NR_MAGIC    = b'mac-nr'  # UDP 载荷必须以此 6 字节开头，否则跳过

# TLV 字段 tag 值（来自 srsran/common/pcap.h）
TAG_RNTI        = 0x02  # 后跟 2 字节 RNTI（大端）
TAG_UEID        = 0x03  # 后跟 2 字节 UE ID（大端），通常等于 RNTI
TAG_FRAME_SF    = 0x04  # 后跟 2 字节，高 12 位 = SFN（系统帧号），低 4 位 = SF（子帧/时槽号）
TAG_PHR_TYPE2   = 0x05  # 后跟 1 字节，PHR 类型标志，本脚本不使用，直接跳过
TAG_HARQID      = 0x06  # 后跟 1 字节 HARQ 进程 ID
TAG_PAYLOAD     = 0x01  # 此 tag 之后是 MAC PDU 载荷，TLV 解析结束

# direction 字段的枚举值
DIRECTION_UL    = 0  # 上行（UE → gNB）
DIRECTION_DL    = 1  # 下行（gNB → UE）

# rntiType 字段的枚举值（只列出本脚本用到的）
RNTI_TYPE_RA    = 2  # RA-RNTI：随机接入过程中的临时标识，用于 RAR 响应
RNTI_TYPE_CRNTI = 3  # C-RNTI：UE 接入后分配的正式小区无线网络临时标识


# ─── pcap 文件读取 ────────────────────────────────────────────────────────────

def read_pcap_records(filepath):
    """
    生成器函数：逐包读取标准 pcap 文件（libpcap 格式），每次 yield (dlt, data)。

    pcap 文件结构：
      ┌─────────────────────────────────────────┐
      │  全局头 (24 字节)                        │
      │   magic(4B) version(4B) zone/accuracy(8B)│
      │   snaplen(4B) DLT 链路类型(4B)           │
      ├─────────────────────────────────────────┤
      │  数据包记录 × N                          │
      │   时间戳(8B) incl_len(4B) orig_len(4B)  │  ← 每包 16 字节头
      │   包体 (incl_len 字节)                   │
      └─────────────────────────────────────────┘

    DLT（Data Link Type）标识包体从哪一层开始：
      DLT=149  → 包体直接从 UDP 头开始（srsRAN 4G/5G MAC pcap 私有格式）
      DLT=252  → Wireshark Upper PDU（exported_pdu），包体先有一段 TLV 元数据头
      DLT=155  → 同上，部分旧版本 Wireshark 使用

    magic 字段用于判断文件字节序：
      0xa1b2c3d4 → 小端（little-endian，x86 常见）
      0xd4c3b2a1 → 大端（big-endian）
    """
    with open(filepath, 'rb') as f:
        # 读取并解析 24 字节全局头
        hdr = f.read(24)
        if len(hdr) < 24:
            return  # 文件过短，直接结束生成器

        magic = struct.unpack('<I', hdr[:4])[0]
        if magic == 0xa1b2c3d4:
            endian = '<'  # 小端
        elif magic == 0xd4c3b2a1:
            endian = '>'  # 大端
        else:
            raise ValueError(f"非标准 pcap magic: {magic:#010x}")

        # 解包全局头，取出 DLT（链路类型），其余字段本脚本不使用
        _, _, _, _, _, _, dlt = struct.unpack(endian + 'IHHiIII', hdr)

        # 循环读取每条数据包记录
        while True:
            rec = f.read(16)          # 读取包头（16 字节）
            if len(rec) < 16:
                break                 # 文件结束
            # incl_len = 实际保存的字节数（可能 < orig_len，因为有 snaplen 截断）
            _, _, incl_len, _ = struct.unpack(endian + 'IIII', rec)
            data = f.read(incl_len)   # 读取包体
            if len(data) < incl_len:
                break                 # 末尾截断，停止
            yield dlt, data


# ─── MAC-NR 载荷解析 ──────────────────────────────────────────────────────────

def parse_mac_nr_payload(payload):
    """
    解析 UDP 载荷中的 mac-nr 上下文字段，提取方向、RNTI、SFN/SF 等信息。

    payload: bytes，从 UDP 数据区开始（含 "mac-nr" 魔数）

    返回 dict（字段见下）或 None（格式不符）。

    TLV 解析流程示意：
      payload = b'mac-nr' + radioType(1) + direction(1) + rntiType(1)
                + [0x02 rnti_hi rnti_lo]          ← TAG_RNTI，固定 2 字节值
                + [0x03 ueid_hi ueid_lo]           ← TAG_UEID，固定 2 字节值
                + [0x06 harqid]                    ← TAG_HARQID，固定 1 字节值
                + [0x05 phr_flag]                  ← TAG_PHR_TYPE2，固定 1 字节值
                + [0x04 sfn_sf_hi sfn_sf_lo]       ← TAG_FRAME_SF，固定 2 字节值
                + [0x01] + MAC_PDU_bytes           ← TAG_PAYLOAD，之后是原始 MAC PDU
    """
    # 校验魔数
    if not payload.startswith(MAC_NR_MAGIC):
        return None
    off = len(MAC_NR_MAGIC)          # 跳过魔数，off 指向 radioType

    if len(payload) < off + 3:
        return None                  # 至少还需要 radioType + direction + rntiType

    radio_type = payload[off];  off += 1
    direction  = payload[off];  off += 1  # 0=UL, 1=DL
    rnti_type  = payload[off];  off += 1  # 2=RA-RNTI, 3=C-RNTI, ...

    rnti = ueid = harqid = sfn = sf = 0

    # 逐个解析 TLV 字段，直到遇到 TAG_PAYLOAD(0x01) 或缓冲区耗尽
    while off < len(payload):
        tag = payload[off]; off += 1

        if tag == TAG_PAYLOAD:
            # 0x01 标志 MAC PDU 开始，TLV 头解析结束
            break

        elif tag == TAG_RNTI:
            # RNTI 值：2 字节大端整数
            if off + 2 > len(payload): break
            rnti = struct.unpack('>H', payload[off:off+2])[0]
            off += 2

        elif tag == TAG_UEID:
            # UE ID：2 字节大端整数（通常与 RNTI 相同）
            if off + 2 > len(payload): break
            ueid = struct.unpack('>H', payload[off:off+2])[0]
            off += 2

        elif tag == TAG_FRAME_SF:
            # SFN/SF 编码：16 位大端，高 12 位 = SFN（0~1023），低 4 位 = SF（0~9 或 0~19）
            # 注：在 srsRAN 的 NR pcap 中，低 4 位实际是 slot-in-frame 的低 4 位
            if off + 2 > len(payload): break
            val = struct.unpack('>H', payload[off:off+2])[0]
            off += 2
            sfn = val >> 4    # 取高 12 位
            sf  = val & 0xf   # 取低 4 位

        elif tag == TAG_HARQID:
            # HARQ 进程 ID：1 字节（NR 共 16 个 HARQ 进程，0~15）
            if off >= len(payload): break
            harqid = payload[off]; off += 1

        elif tag == TAG_PHR_TYPE2:
            # PHR 类型标志：1 字节，本脚本不使用，直接跳过
            if off >= len(payload): break
            off += 1

        else:
            # 遇到未知 tag，跳过 1 字节继续（容错处理）
            off += 1

    # TAG_PAYLOAD 之后剩余的字节就是 MAC PDU 本体，记录其长度
    pdu_bytes = len(payload) - off

    return {
        'radio_type': radio_type,
        'direction':  direction,   # 0=UL, 1=DL
        'rnti_type':  rnti_type,
        'rnti':       rnti,
        'ueid':       ueid,
        'harqid':     harqid,
        'sfn':        sfn,
        'sf':         sf,
        'pdu_bytes':  pdu_bytes,
    }


def udp_to_mac_nr(data, udp_off=0):
    """
    在 data[udp_off:] 处读取一个 UDP 头（8 字节），校验端口号，
    然后把 UDP 载荷送给 parse_mac_nr_payload() 解析。

    UDP 头结构（8 字节，大端）：
      src_port(2B) dst_port(2B) length(2B) checksum(2B)

    srsRAN MAC pcap 约定：src=0xBEEF, dst=0xDEAD，用于快速识别
    """
    if udp_off + 8 > len(data):
        return None
    src_port = struct.unpack('>H', data[udp_off:udp_off+2])[0]
    dst_port = struct.unpack('>H', data[udp_off+2:udp_off+4])[0]
    # 端口号不匹配说明这个包不是 mac-nr 格式
    if src_port != 0xbeef or dst_port != 0xdead:
        return None
    # UDP 载荷从第 8 字节开始
    return parse_mac_nr_payload(data[udp_off + 8:])


# ─── 不同 DLT 格式的包解析入口 ────────────────────────────────────────────────

def parse_dlt149_packet(data):
    """
    DLT=149（srsRAN 私有格式）：包体直接就是 UDP 头 + 载荷，没有额外封装。
    SNI5GECT 生成的 pcap 文件使用此格式。
    """
    return udp_to_mac_nr(data, udp_off=0)


def parse_dlt252_packet(data):
    """
    DLT=252（Wireshark Upper PDU / exported_pdu）：
    srsRAN Project gNB 生成的 pcap 文件使用此格式。

    包体结构：
      ┌──────────────────────────────────────────┐
      │  exported_pdu TLV 元数据头               │
      │  每个 TLV：tag(2B 大端) len(2B 大端) val │
      │  结束标志：tag=0x0000 len=0x0000          │
      ├──────────────────────────────────────────┤
      │  内嵌的真实 UDP 包（从 UDP 头开始）       │
      │  → src=0xBEEF dst=0xDEAD + mac-nr 载荷   │
      └──────────────────────────────────────────┘

    本脚本只关心定位到内嵌 UDP 头的位置，不需要解析 TLV 元数据内容。
    因此只需跳过所有 TLV 直到遇到 tag=0（end-of-options）即可。

    示例：
      00 0c 00 04 75 64 70 00   → tag=12(协议名) len=4 val="udp\0"
      00 00 00 00               → tag=0 end-of-options
      be ef de ad ...           → UDP 头开始
    """
    off = 0
    while off + 4 <= len(data):
        tag    = struct.unpack('>H', data[off:off+2])[0]
        length = struct.unpack('>H', data[off+2:off+4])[0]
        off += 4         # 跳过 tag(2B) 和 len(2B)
        if tag == 0:     # tag=0 表示 TLV 列表结束，后面就是真正的数据包
            break
        off += length    # 跳过当前 TLV 的 value 部分
    # off 现在指向内嵌 UDP 头的起始位置
    return udp_to_mac_nr(data, off)


# ─── 各文件来源的顶层解析函数 ─────────────────────────────────────────────────

def parse_sni5gect_dir(sni_dir):
    """
    扫描 sni_dir 目录下所有 UE-XXXXX.pcap 文件，解析每个文件中的所有包，
    按 RNTI 和方向（DL/UL）分类收集 (sfn, sf, pdu_bytes) 元组。

    返回结构：
      {
        rnti_int: {
          'DL': [(sfn, sf, pdu_bytes), ...],
          'UL': [(sfn, sf, pdu_bytes), ...],
          'filename': 'UE-XXXXX.pcap',
        },
        ...
      }

    注意：SNI5GECT 写入 pcap 时，TAG_RNTI 字段存放的是内部 task_idx
    而非真实 RNTI。真实 RNTI 必须从文件名（UE-{RNTI}.pcap）中提取。
    """
    result = {}
    # 正则表达式匹配文件名格式 "UE-数字.pcap"，并捕获数字部分
    pattern = re.compile(r'UE-(\d+)\.pcap$')

    pcap_files = sorted([f for f in os.listdir(sni_dir) if pattern.match(f)])
    if not pcap_files:
        print(f"[警告] {sni_dir} 中未找到 UE-*.pcap 文件", file=sys.stderr)
        return result

    for fname in pcap_files:
        m = pattern.match(fname)
        rnti = int(m.group(1))           # 从文件名提取真实 RNTI
        fpath = os.path.join(sni_dir, fname)
        dl_packets = []
        ul_packets = []

        try:
            for dlt, data in read_pcap_records(fpath):
                # SNI5GECT 的 pcap 固定为 DLT=149
                pkt = parse_dlt149_packet(data)
                if pkt is None:
                    continue
                # 只保留 (sfn, sf, pdu_bytes) 三元组，其余字段不需要
                entry = (pkt['sfn'], pkt['sf'], pkt['pdu_bytes'])
                if pkt['direction'] == DIRECTION_DL:
                    dl_packets.append(entry)
                elif pkt['direction'] == DIRECTION_UL:
                    ul_packets.append(entry)
        except Exception as e:
            print(f"[警告] 解析 {fname} 出错: {e}", file=sys.stderr)
            continue

        if dl_packets or ul_packets:
            result[rnti] = {
                'DL':       dl_packets,
                'UL':       ul_packets,
                'filename': fname,
            }

    return result


def parse_gnb_pcap(gnb_path):
    """
    解析 gNB 侧导出的 gnb_mac.pcap，提取所有 C-RNTI 用户的 DL/UL 包信息。

    gNB pcap 使用 DLT=252（Wireshark Upper PDU 格式），内嵌 UDP + mac-nr 载荷。
    rnti 字段在 gNB pcap 中是真实的 C-RNTI，可以直接使用。

    过滤规则：只保留 rntiType=3（C-RNTI）的包，跳过：
      - rntiType=2（RA-RNTI）：随机接入响应（RAR），广播给竞争 UE
      - rntiType=0（P-RNTI）：寻呼（Paging）
      - rntiType=1（SI-RNTI）：系统信息（SIB）

    返回结构：
      {
        rnti_int: {
          'DL': [(sfn, sf, pdu_bytes), ...],
          'UL': [(sfn, sf, pdu_bytes), ...],
        },
        ...
      }
    """
    # defaultdict 使得第一次访问某个 rnti 时自动创建 {'DL':[], 'UL':[]}
    result = defaultdict(lambda: {'DL': [], 'UL': []})

    try:
        for dlt, data in read_pcap_records(gnb_path):
            pkt = None
            if dlt in (155, 252):
                # DLT=252 是 srsRAN Project 使用的 Wireshark Upper PDU 格式
                # DLT=155 是旧版本 Wireshark 的同类格式，结构完全相同，一并处理
                pkt = parse_dlt252_packet(data)
            elif dlt == 149:
                # 少数情况下 gNB 也可能用 DLT=149 格式
                pkt = parse_dlt149_packet(data)
            else:
                continue  # 未知 DLT，跳过

            if pkt is None:
                continue

            # 只保留 C-RNTI 包，过滤 SIB / RAR / Paging 等系统控制帧
            if pkt['rnti_type'] != RNTI_TYPE_CRNTI:
                continue

            rnti  = pkt['rnti']
            entry = (pkt['sfn'], pkt['sf'], pkt['pdu_bytes'])
            if pkt['direction'] == DIRECTION_DL:
                result[rnti]['DL'].append(entry)
            elif pkt['direction'] == DIRECTION_UL:
                result[rnti]['UL'].append(entry)

    except Exception as e:
        print(f"[错误] 解析 gNB pcap 出错: {e}", file=sys.stderr)

    return dict(result)


# ─── 成功率计算 ───────────────────────────────────────────────────────────────

def to_abs_slot(sfn, sf):
    """
    将 (SFN, SF) 转换为单调递增的绝对时槽编号。

    5G NR 时帧结构（30kHz SCS 为例）：
      1 无线帧（Radio Frame）= 10ms，帧号 SFN 循环 0~1023
      1 子帧（Subframe）= 1ms，子帧号 SF = 0~9
      1 子帧有 2 个时槽（Slot），slot-in-subframe = 0 或 1

    srsRAN pcap 中的 TAG_FRAME_SF 编码：高 12 位 = SFN，低 4 位 = SF
    在 srsRAN 的 NR pcap 实现里，"SF" 实际记录的是 slot-in-frame 的低 4 位，
    范围 0~9（此处每帧只取低 4 位，等效于子帧号），足够用于时间对齐比较。

    公式：abs_slot = SFN × 10 + SF
    （SFN 最大 1023，对应 abs_slot 最大 10239，约 5.12 秒一个循环周期）
    """
    return sfn * 10 + sf


def compute_success_rate(sni_packets, gnb_packets, window=SLOT_WINDOW):
    """
    计算 SNI5GECT 对于某一 RNTI + 方向（DL 或 UL）的解码成功率。

    核心思路：
      以"唯一时槽"（unique slot）为计数单位，使用一对一贪心匹配。

    为什么要去重时槽（dedup）？
      gNB 对同一时槽的同一 PDU 可能有多次 HARQ 重传记录（不同 RV 版本），
      直接用包数会虚高 gNB 侧的计数（实测最多 22 包/时槽）。
      转换为唯一时槽后，每个实际传输机会只计一次。

    为什么要一对一匹配（而不是集合包含检查）？
      若用简单集合扩展（将每个 SNI 时槽扩展为 ±window 个时槽的集合，再查 gNB 时槽是否在集合中），
      则 1 个 SNI 解码可以同时"覆盖"相邻的多个 gNB 时槽，导致成功率虚高。
      例如：SNI 解码了时槽 100，gNB 在时槽 99/100/101/102 都有记录，
      若 window=2，则 4 个 gNB 时槽都算"匹配成功"，但 SNI 实际只解码了 1 次。
      一对一匹配确保每个 SNI 解码最多贡献 1 次成功匹配。

    算法步骤：
      1. 对 sni_packets 和 gnb_packets 分别提取唯一时槽集合（set），并排序
      2. 过滤：只保留在 SNI 活跃时间范围（±50 槽余量）内的 gNB 时槽
      3. 对每个 gNB 时槽，用二分查找在 SNI 有序列表中找 ±window 内最近的 SNI 时槽
      4. 若找到且该 SNI 时槽尚未被匹配，则计为一次成功，并标记该 SNI 时槽已用

    返回：(matched, gnb_unique_slots, sni_unique_slots)
      gnb_unique_slots : gNB 在活跃时间窗口内的唯一时槽数（分母）
      matched          : 成功一对一匹配的时槽对数（分子）
      sni_unique_slots : SNI5GECT 解码出的唯一时槽数（供参考）
    成功率 = matched / gnb_unique_slots × 100%
    """
    if not sni_packets and not gnb_packets:
        return 0, 0, 0

    # Step 1：提取 SNI5GECT 唯一时槽，并排序（排序是二分查找的前提）
    sni_abs_sorted = sorted(set(to_abs_slot(sfn, sf) for sfn, sf, _ in sni_packets))

    if not sni_abs_sorted:
        # SNI 没有数据，返回 gNB 唯一时槽数（成功率=0）
        gnb_abs_set = set(to_abs_slot(sfn, sf) for sfn, sf, _ in gnb_packets)
        return 0, len(gnb_abs_set), 0

    # SNI 活跃时间窗口的边界（最早和最晚解码的时槽）
    sni_min = sni_abs_sorted[0]
    sni_max = sni_abs_sorted[-1]

    # Step 2：提取 gNB 唯一时槽，只保留 SNI 活跃时间段内（±50 时槽余量）的条目
    # ±50 的余量是为了包含 SNI 时间范围边缘附近的 gNB 包，避免边界截断
    gnb_abs_sorted = sorted(set(
        to_abs_slot(sfn, sf)
        for sfn, sf, _ in gnb_packets
        if sni_min - 50 <= to_abs_slot(sfn, sf) <= sni_max + 50
    ))

    if not gnb_abs_sorted:
        return 0, 0, len(sni_abs_sorted)

    # Step 3 & 4：贪心一对一匹配
    sni_list = sni_abs_sorted   # 有序 SNI 时槽列表，用于二分查找
    used_sni = set()            # 已被匹配的 SNI 时槽，防止重复使用
    matched  = 0

    for g in gnb_abs_sorted:
        # 用 bisect 在有序 sni_list 中找到所有落在 [g-window, g+window] 范围内的候选
        lo = bisect.bisect_left(sni_list,  g - window)
        hi = bisect.bisect_right(sni_list, g + window)

        # 在候选区间内找距离最近且尚未被占用的 SNI 时槽
        best      = None
        best_dist = window + 1  # 初始化为比最大允许距离大 1
        for idx in range(lo, hi):
            s = sni_list[idx]
            if s in used_sni:
                continue       # 该 SNI 时槽已被之前的 gNB 时槽匹配，跳过
            dist = abs(s - g)
            if dist < best_dist:
                best_dist = dist
                best      = s

        if best is not None:
            used_sni.add(best)  # 标记为已使用
            matched += 1

    return matched, len(gnb_abs_sorted), len(sni_abs_sorted)


# ─── 报告输出 ─────────────────────────────────────────────────────────────────

def print_report(sni_data, gnb_data, output=None):
    """
    用 PrettyTable 格式化输出解码成功率报告，同时打印到终端并可选写入文件。
    """
    sni_rntis = sorted(sni_data.keys())
    gnb_rntis = set(gnb_data.keys())

    # 创建表格并定义列名
    table = PrettyTable()
    table.field_names = ["RNTI", "方向", "gNB时槽(窗口)", "SNI5GECT时槽", "匹配时槽", "成功率", "文件"]

    # 设置各列对齐方式
    table.align["RNTI"]         = "r"
    table.align["方向"]          = "c"
    table.align["gNB时槽(窗口)"] = "r"
    table.align["SNI5GECT时槽"]  = "r"
    table.align["匹配时槽"]       = "r"
    table.align["成功率"]         = "r"
    table.align["文件"]           = "l"

    # 汇总计数器
    total_gnb_dl = total_gnb_ul = 0
    total_matched_dl = total_matched_ul = 0
    total_sni_dl = total_sni_ul = 0

    for rnti in sni_rntis:
        sni_info = sni_data[rnti]
        fname    = sni_info.get('filename', f'UE-{rnti}.pcap')

        if rnti not in gnb_data:
            # gNB pcap 中没有该 RNTI 的记录，无法计算成功率
            for direction in ('DL', 'UL'):
                sni_slots = len(set((s, f) for s, f, _ in sni_info[direction]))
                table.add_row([rnti, direction, "N/A", sni_slots, "N/A", "N/A", fname])
            continue

        gnb_info = gnb_data[rnti]

        for direction in ('DL', 'UL'):
            sni_pkts = sni_info[direction]
            gnb_pkts = gnb_info[direction]

            matched, gnb_window, sni_slots = compute_success_rate(sni_pkts, gnb_pkts)

            # 累加汇总计数
            if direction == 'DL':
                total_gnb_dl     += gnb_window
                total_matched_dl += matched
                total_sni_dl     += sni_slots
            else:
                total_gnb_ul     += gnb_window
                total_matched_ul += matched
                total_sni_ul     += sni_slots

            rate_str = f"{matched / gnb_window * 100:.1f}%" if gnb_window > 0 else "N/A"
            table.add_row([rnti, direction, gnb_window, sni_slots, matched, rate_str, fname])

    # 添加分隔行和汇总行
    table.add_divider()
    for direction, gnb_t, mat_t, sni_t in [
        ("DL", total_gnb_dl,  total_matched_dl,  total_sni_dl),
        ("UL", total_gnb_ul,  total_matched_ul,  total_sni_ul),
    ]:
        rate_str = f"{mat_t / gnb_t * 100:.1f}%" if gnb_t > 0 else "N/A"
        table.add_row(["合计", direction, gnb_t, sni_t, mat_t, rate_str, ""])

    note = (
        "\n说明:\n"
        f"  gNB时槽(窗口) : gNB pcap 在 SNI5GECT 活跃时间段内的唯一时槽数\n"
         "                  （同一时槽的多次 HARQ 重传包只计一次）\n"
         "  SNI5GECT时槽  : SNI5GECT 成功解码的唯一时槽数\n"
        f"  匹配时槽      : 一对一匹配成功的时槽对数（容差 ±{SLOT_WINDOW} 时槽）\n"
         "  成功率        : 匹配时槽 / gNB时槽(窗口) × 100%\n"
    )

    title = "\n  SNI5GECT 解码成功率分析报告\n"
    meta  = (f"  gNB 中存在的 C-RNTI : {sorted(gnb_rntis)}\n"
             f"  SNI5GECT 追踪的 RNTI: {sni_rntis}\n")

    report = title + meta + table.get_string() + note
    print(report)

    if output:
        with open(output, 'w') as f:
            f.write(report)
        print(f"报告已保存至: {output}")


# ─── 入口 ─────────────────────────────────────────────────────────────────────

def main():
    # 使用脚本顶部配置区中的硬编码路径
    sni_dir  = SNI5GECT_DIR
    gnb_path = GNB_PCAP
    output   = OUTPUT_FILE

    # 路径检查
    if not os.path.isdir(sni_dir):
        print(f"[错误] SNI5GECT 目录不存在: {sni_dir}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(gnb_path):
        print(f"[错误] gNB pcap 文件不存在: {gnb_path}", file=sys.stderr)
        sys.exit(1)

    # 解析两侧数据
    print(f"正在解析 SNI5GECT 目录: {sni_dir}", file=sys.stderr)
    sni_data = parse_sni5gect_dir(sni_dir)
    print(f"  → 找到 {len(sni_data)} 个 UE 的 pcap 文件", file=sys.stderr)

    print(f"正在解析 gNB pcap: {gnb_path}", file=sys.stderr)
    gnb_data = parse_gnb_pcap(gnb_path)
    print(f"  → 找到 {len(gnb_data)} 个 C-RNTI", file=sys.stderr)

    # 生成并输出报告
    print_report(sni_data, gnb_data, output)


if __name__ == '__main__':
    main()
