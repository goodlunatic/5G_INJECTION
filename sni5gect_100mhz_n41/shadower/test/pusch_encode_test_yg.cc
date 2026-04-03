/**
 * Copyright 2013-2023 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */
//test yg usage sudo ./lib/src/phy/phch/test/pusch_nr_phy_test -p 6 -m 28 -M 0 -v
#include "srsran/interfaces/ue_nr_interfaces.h"
#include <complex.h>
#include <getopt.h>
#include <cmath>     // or this, depending on compiler
#include <fstream> 
#ifdef __cplusplus
extern "C" {
  #include "srsran/phy/ue/ue_ul_nr.h"
  #include "srsran/phy/gnb/gnb_dl.h"
  #include "srsran/phy/phch/ra_dl_nr.h"
  #include "srsran/phy/ue/ue_dl_nr.h"
  #include "srsran/phy/utils/debug.h"
  #include "srsran/phy/phch/pusch_nr.h"
  #include "srsran/phy/phch/ra_nr.h"
  #include "srsran/phy/phch/ra_ul_nr.h"
  #include "srsran/phy/utils/random.h"
  #include "srsran/phy/utils/vector.h"
  #include "srsran/phy/gnb/gnb_ul.h"
}
#endif
#include "srsran/asn1/obj_id_cmp_utils.h"
#include "srsran/asn1/rrc_nr_utils.h"
#include "srsran/common/band_helper.h"
#include "srsran/common/string_helpers.h"
#include "srsran/mac/pdu.h"
#include <bitset>
#include <getopt.h> 
#include <cstdio>
#include <iomanip>
#include "srsran/asn1/rrc_nr.h"
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/srslog/srslog.h"
#include "srsran/common/mac_pcap.h"
#include "srsran/config.h"
#include "srsran/mac/mac_rar_pdu_nr.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include "srsran/common/yg_utils.h"
#include <array>
#include <iostream>
#include <memory>
#include <vector>
#define SRSRAN_YG1_DEFAULT_CARRIER_NR                                                                                      \
  {                                                                                                                    \
    .pci = 1, .dl_center_frequency_hz = 1842.5e6, .ul_center_frequency_hz = 1747.5e6,                      \
    .ssb_center_freq_hz = 1842.05e6, .offset_to_carrier = 0, .scs = srsran_subcarrier_spacing_15kHz, .nof_prb = 106,        \
    .start = 0, .max_mimo_layers = 1                                                                                   \
  }
//const char *mac_hex_stream = "34117c720716a63d013f00";
const char *mac_hex_stream = "341a22ee2fff063d013fff";

uint8_t mac_dl_rar_pdu[] = {0x40, 0x00, 0xe0, 0x0d, 0xc0, 0x0e, 0x46, 0x02, 0x00, 0x00};
//std::string mib_config_raw = "/home/yg/5G/0912/srsRAN_4G/mib.raw";
//std::string sib1_config_raw = "/home/yg/5G/0912/srsRAN_4G/sib1.raw";
std::string mib_config_raw = "/home/yg/5G/5g_e2e/ori/20250919/srsRAN_4G/mib.raw";
std::string sib1_config_raw = "/home/yg/5G/5g_e2e/ori/20250919/srsRAN_4G/sib1.raw";

static srsran_carrier_nr_t carrier      = SRSRAN_YG1_DEFAULT_CARRIER_NR;
static srsran_sch_cfg_nr_t pusch_cfg    = {};
void hex_string_to_byte_array(const char *hex_string, uint8_t *byte_array);
void write_record_to_file(cf_t* buffer, uint32_t length, char* name);
bool load_samples(cf_t* buffer, size_t nsamples, char* filename);
void construct_dci_ul(srsran_dci_ul_nr_t& dci_ul);
/* Load mib configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_mib(srsran::phy_cfg_nr_t& phy_cfg, std::string& filename, uint32_t ncellid);
/* Load SIB1 configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_sib1(srsran::phy_cfg_nr_t& phy_cfg, std::string& filename, uint32_t nbits);
/* Apply MIB configuration to phy cfg */
bool update_phy_cfg_from_mib(srsran::phy_cfg_nr_t& phy_cfg, srsran_mib_nr_t& mib, uint32_t ncellid);
/* Apply SIB1 configuration to phy cfg */
void update_phy_cfg_from_sib1(srsran::phy_cfg_nr_t& phy_cfg, asn1::rrc_nr::sib1_s& sib1);
bool read_raw_config(const std::string& filename, uint8_t* buffer, size_t size);
bool parse_to_sib1(uint8_t* data, uint32_t len, asn1::rrc_nr::sib1_s& sib1);
bool stdout_flag = false;
int gen_mode = 0;
void usage(char* prog)
{
  printf("Usage: %s [pTL] \n", prog);
  printf("\t-T Provide MCS table (64qam, 256qam, 64qamLowSE) [Default %s]\n",
         srsran_mcs_table_to_str(pusch_cfg.sch_cfg.mcs_table));
  printf("\t-L Provide number of layers [Default %d]\n", carrier.max_mimo_layers);
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}

int parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "sMTLv")) != -1) {
    switch (opt) {
      case 's':
        stdout_flag = true;
        break;
      case 'M':
        gen_mode = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'T':
        pusch_cfg.sch_cfg.mcs_table = srsran_mcs_table_from_str(argv[optind]);
        break;
      case 'L':
        carrier.max_mimo_layers = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'v':
        increase_srsran_verbose_level();
        break;
      default:
        usage(argv[0]);
        return SRSRAN_ERROR;
    }
  }

  return SRSRAN_SUCCESS;
}

int main(int argc, char** argv)
{
  int                   ret      = SRSRAN_ERROR;
  srsran_pusch_data_nr_t data_tx = {};
  if (parse_args(argc, argv) < SRSRAN_SUCCESS) {
  }
    /* initialize phy cfg */
  srsran::phy_cfg_nr_t phy_cfg = {};
  phy_cfg.carrier.dl_center_frequency_hz = carrier.dl_center_frequency_hz;
  phy_cfg.carrier.ul_center_frequency_hz = carrier.ul_center_frequency_hz;
  phy_cfg.carrier.ssb_center_freq_hz     = carrier.ssb_center_freq_hz;
  phy_cfg.carrier.offset_to_carrier      = 0;
  phy_cfg.carrier.scs                    = carrier.scs;
  phy_cfg.carrier.nof_prb                = carrier.nof_prb;
  phy_cfg.carrier.max_mimo_layers        = 1;
  phy_cfg.duplex.mode                    = SRSRAN_DUPLEX_MODE_FDD;
  phy_cfg.ssb.periodicity_ms             = 10;
  phy_cfg.ssb.position_in_burst[0]       = true;
  phy_cfg.ssb.scs                        = carrier.scs;
  phy_cfg.ssb.pattern                    = SRSRAN_SSB_PATTERN_A;

  uint32_t    sib1_size       = 80;    
  srsran_mib_nr_t mib = {};
  uint32_t ncellid = 1;
  if (!configure_phy_cfg_from_mib(phy_cfg, mib_config_raw, ncellid)) {
    printf("Failed to configure phy cfg from mib\n");
    return -1;
  }
  if (!configure_phy_cfg_from_sib1(phy_cfg, sib1_config_raw, sib1_size)) {
    printf("Failed to configure phy cfg from sib1");
    return -1;
  }  
  phy_cfg.pusch.scaling                = 1.0f;
  phy_cfg.pusch.beta_offsets.fix_ack   = 12.625f;
  phy_cfg.pusch.beta_offsets.fix_csi1  = 2.25f;
  phy_cfg.pusch.beta_offsets.fix_csi2  = 2.25f;
  /*
  phy_cfg.pusch.scaling                = 1.0f;
  phy_cfg.pusch.beta_offsets.fix_ack   = 0.0f;
  phy_cfg.pusch.beta_offsets.fix_csi1  = 0.0f;
  phy_cfg.pusch.beta_offsets.fix_csi2  = 0.0f;
  phy_cfg.pusch.beta_offsets.ack_index1 = 11;
  phy_cfg.pusch.beta_offsets.ack_index2 = 11;
  phy_cfg.pusch.beta_offsets.ack_index3 = 11;
  phy_cfg.pusch.beta_offsets.csi1_index1 = 13;
  phy_cfg.pusch.beta_offsets.csi1_index2 = 13;
  phy_cfg.pusch.beta_offsets.csi2_index1 = 13;
  phy_cfg.pusch.beta_offsets.csi2_index2 = 13;
  */
  cf_t*    buffer_ue[SRSRAN_MAX_PORTS]   = {};
  srsran_use_standard_symbol_size(false);
  uint32_t sf_len = SRSRAN_SF_LEN_PRB_NR(carrier.nof_prb, carrier.scs);
  printf("test yg sf_len:%d\n", sf_len);
  buffer_ue[0]    = srsran_vec_cf_malloc(sf_len);
  if (buffer_ue[0] == NULL) {
    ERROR("Error malloc");
  }
  srsran_slot_cfg_t slot = {};
  uint32_t slot_idx = 5;  // Set to 0 for steering
  slot.idx = slot_idx;

  srsran_ue_ul_nr_t ue_ul = {};
  //ue_ul.freq_offset_hz = -5.135044;

  srsue::phy_args_nr_t phy_args;
  phy_args.ul.pusch.sch.disable_simd       = false;
  phy_args.ul.pusch.measure_evm            = true;
  phy_args.ul.nof_max_prb            = carrier.nof_prb;
  phy_args.ul.pusch.max_prb          = carrier.nof_prb;
  if (srsran_ue_ul_nr_init(&ue_ul, buffer_ue[0], &phy_args.ul) < SRSRAN_SUCCESS) {
    ERROR("Error initiating UE DL NR");
  }
  if (srsran_ue_ul_nr_set_carrier(&ue_ul, &phy_cfg.carrier) < SRSRAN_SUCCESS) {
    ERROR("Error setting carrier");
  }
  printf("test yg ue_ul.pusch.max_cw:%d", ue_ul.pusch.max_cw);

  for (uint32_t i = 0; i < ue_ul.pusch.max_cw; i++) {
    data_tx.payload[i]    = srsran_vec_u8_malloc(SRSRAN_SLOT_MAX_NOF_BITS_NR);
    if (data_tx.payload[i] == NULL) {
      ERROR("Error malloc");
    }
  }
  srsran_softbuffer_tx_t softbuffer_tx = {};
  if (srsran_softbuffer_tx_init_guru(&softbuffer_tx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    ERROR("Error init soft-buffer");
   }
  
  srsran_dci_ul_nr_t dci_ul = {};
  construct_dci_ul(dci_ul);
  //test yg print
  //print_srsran_dci_ul_nr(&dci_ul);
  srsran_ra_ul_dci_to_grant_nr(&phy_cfg.carrier, &slot, &phy_cfg.pusch, &dci_ul, &pusch_cfg, &pusch_cfg.grant);
  /*if (srsran_ra_nr_fill_tb(&pusch_cfg, &pusch_cfg.grant, dci_ul.mcs, &pusch_cfg.grant.tb[0]) < SRSRAN_SUCCESS) {
    ERROR("Error filling tb");        
  }*/
  //printf("原本的grant：\n");

  for (uint32_t tb = 0; tb < SRSRAN_MAX_TB; tb++) {
    // Skip TB if no allocated
    if (data_tx.payload[tb] == NULL) {
      continue;
    }
    size_t byte_array_length = strlen(mac_hex_stream) / 2;
    uint8_t raw_bytes[int(byte_array_length)];
    hex_string_to_byte_array(mac_hex_stream, raw_bytes);
    printf("test yg print hex: size:%ld\n", byte_array_length);
    print_hex(raw_bytes, byte_array_length);
    memcpy(data_tx.payload[0], raw_bytes, byte_array_length);
    pusch_cfg.grant.tb[tb].softbuffer.tx = &softbuffer_tx;
  }
  if (srsran_ra_ul_set_grant_uci_nr(&phy_cfg.carrier, &phy_cfg.pusch, &pusch_cfg.uci, &pusch_cfg) < SRSRAN_SUCCESS) {
    ERROR("Setting UCI");        
  }
  printf("test yg phy_cfg.carrier:\n");
  print_srsran_carrier_nr(&phy_cfg.carrier);
  printf("test yg ue_ul:\n");
  print_srsran_ue_ul_nr(ue_ul);
  //print_srsran_carrier_nr(&ue_ul.carrier);
  //print_srsran_ofdm(&ue_ul.ifft);
  print_srsran_sch_cfg_nr_t(&pusch_cfg); 
  print_sch_hl_cfg_nr(&phy_cfg.pusch);

  if (srsran_ue_ul_nr_encode_pusch(&ue_ul, &slot, &pusch_cfg, &data_tx) < SRSRAN_SUCCESS) {
    printf("error Encoding PUSCH");
  } else {
    //printf("test yg data 编码完毕\n");
  }
  //DEBUG("buffer=");
  //srsran_vec_fprint_c(stdout, buffer_ue[0], sf_len);
  uint32_t zero_padding_len = sf_len / 10;
  uint32_t total_len = sf_len + 2 * zero_padding_len;
  cf_t *temp_buffer = srsran_vec_cf_malloc(total_len);
  if (!temp_buffer) {
      perror("malloc failed");
  }
  srsran_vec_cf_zero(temp_buffer, total_len);
  memcpy(temp_buffer + zero_padding_len, buffer_ue[0], sf_len * sizeof(cf_t));

  char filename1[64];
  sprintf(filename1, "gen_msg3_raw_%d_%d.fc32", dci_ul.ctx.rnti, slot.idx);
  write_record_to_file(buffer_ue[0], sf_len, filename1);

  char filename2[64];
  sprintf(filename2, "gen_msg3_padded_%d_%d.fc32", dci_ul.ctx.rnti, slot.idx);
  write_record_to_file(temp_buffer, total_len, filename2);


  printf("test yg 生成消息\n");
  ret = SRSRAN_SUCCESS;

  for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
    if (data_tx.payload[i]) {
      free(data_tx.payload[i]);
    }
    
  }  
  srsran_softbuffer_tx_free(&softbuffer_tx);
  return ret;
}


void write_record_to_file(cf_t* buffer, uint32_t length, char* name)
{

  std::ofstream f1(name, std::ios::binary);
  if (f1) {
    f1.write(reinterpret_cast<char*>(buffer), length * sizeof(cf_t));
    f1.close();
    printf("Saved original signal to %s file length:%d\n", name, length);
  } else {
    printf("Error opening file: %s\n", name);
  }
}
/* Load the IQ samples from a file */
bool load_samples(cf_t* buffer, size_t nsamples, char* filename)
{
  std::ifstream infile(filename, std::ios::binary);
  if (!infile.is_open()) {
    return false;
  }
  infile.read(reinterpret_cast<char*>(buffer), nsamples * sizeof(cf_t));
  infile.close();
  return true;
}

void construct_dci_ul(srsran_dci_ul_nr_t& dci_ul)
{
  srsran::mac_rar_pdu_nr pdu;
  fmt::memory_buffer buff;
  pdu.unpack(mac_dl_rar_pdu, sizeof(mac_dl_rar_pdu));
  pdu.to_string(buff);
  auto& mac_logger = srslog::fetch_basic_logger("MAC");
  //mac_logger.info("Rx PDU: %s", srsran::to_c_str(buff));
  srsran::mac_rar_subpdu_nr subpdu = pdu.get_subpdu(0);
  srsran_dci_msg_nr_t dci_msg = {};
  dci_msg.ctx.format          = srsran_dci_format_nr_rar; // MAC RAR grant shall be unpacked as DCI 0_0 format
  dci_msg.ctx.rnti_type       = srsran_rnti_type_ra;
  dci_msg.ctx.ss_type         = srsran_search_space_type_rar; // This indicates it is a MAC RAR
  dci_msg.ctx.rnti            = subpdu.get_temp_crnti();
  dci_msg.nof_bits            = SRSRAN_RAR_UL_GRANT_NBITS;
  srsran_vec_u8_copy(dci_msg.payload, subpdu.get_ul_grant().data(), SRSRAN_RAR_UL_GRANT_NBITS);

  if (srsran_dci_nr_ul_unpack(NULL, &dci_msg, &dci_ul) < SRSRAN_SUCCESS) {
    printf("Couldn't unpack UL grant");
  }

  std::array<char, 512> str;
  srsran_dci_nr_t       dci = {};
  srsran_dci_ul_nr_to_str(&dci, &dci_ul, str.data(), str.size());
  mac_logger.info("Setting RAR Grant: %s", str.data());
}

/* Apply MIB configuration to phy cfg */
bool update_phy_cfg_from_mib(srsran::phy_cfg_nr_t& phy_cfg, srsran_mib_nr_t& mib, uint32_t ncellid)
{
  phy_cfg.pdsch.typeA_pos = mib.dmrs_typeA_pos;
  phy_cfg.pdsch.scs_cfg   = mib.scs_common;
  phy_cfg.carrier.pci     = ncellid;

  /* Get pointA and SSB absolute frequencies */
  double pointA_abs_freq_Hz = phy_cfg.carrier.dl_center_frequency_hz -
                              phy_cfg.carrier.nof_prb * SRSRAN_NRE * SRSRAN_SUBC_SPACING_NR(phy_cfg.carrier.scs) / 2;
  double ssb_abs_freq_Hz = phy_cfg.carrier.ssb_center_freq_hz;
  /* Calculate integer SSB to pointA frequency offset in Hz */
  uint32_t ssb_pointA_freq_offset_Hz =
      (ssb_abs_freq_Hz > pointA_abs_freq_Hz) ? (uint32_t)(ssb_abs_freq_Hz - pointA_abs_freq_Hz) : 0;
  /* Create coreset0 */
  if (srsran_coreset_zero(phy_cfg.carrier.pci,
                          ssb_pointA_freq_offset_Hz,
                          phy_cfg.ssb.scs,
                          phy_cfg.carrier.scs,
                          mib.coreset0_idx,
                          &phy_cfg.pdcch.coreset[0])) {
    return false;
  }
  phy_cfg.pdcch.coreset_present[0] = true;

  /* Create SearchSpace0 */
  srsran::make_phy_search_space0_cfg(&phy_cfg.pdcch.search_space[0]);
  phy_cfg.pdcch.search_space_present[0] = true;
  return true;
}

/* Apply SIB1 configuration to phy cfg */
void update_phy_cfg_from_sib1(srsran::phy_cfg_nr_t& phy_cfg, asn1::rrc_nr::sib1_s& sib1)
{
  /* Apply PDSCH Config Common */
  if (sib1.serving_cell_cfg_common.dl_cfg_common.init_dl_bwp.pdsch_cfg_common.setup()
          .pdsch_time_domain_alloc_list.size() > 0) {
    if (!srsran::fill_phy_pdsch_cfg_common(
            sib1.serving_cell_cfg_common.dl_cfg_common.init_dl_bwp.pdsch_cfg_common.setup(), &phy_cfg.pdsch)) {
    }
  }

  /* Apply PUSCH Config Common */
  if (!srsran::fill_phy_pusch_cfg_common(
          sib1.serving_cell_cfg_common.ul_cfg_common.init_ul_bwp.pusch_cfg_common.setup(), &phy_cfg.pusch)) {
  }

  /* Apply PUCCH Config Common */
  srsran::fill_phy_pucch_cfg_common(sib1.serving_cell_cfg_common.ul_cfg_common.init_ul_bwp.pucch_cfg_common.setup(),
                                    &phy_cfg.pucch.common);

  /* Apply RACH Config Common */
  if (!srsran::make_phy_rach_cfg(sib1.serving_cell_cfg_common.ul_cfg_common.init_ul_bwp.rach_cfg_common.setup(),
                                 sib1.serving_cell_cfg_common.tdd_ul_dl_cfg_common_present ? SRSRAN_DUPLEX_MODE_TDD
                                                                                           : SRSRAN_DUPLEX_MODE_FDD,
                                 &phy_cfg.prach)) {
  }

  /* Apply PDCCH Config Common */
  srsran::fill_phy_pdcch_cfg_common(sib1.serving_cell_cfg_common.dl_cfg_common.init_dl_bwp.pdcch_cfg_common.setup(),
                                    &phy_cfg.pdcch);

  /* Apply Carrier Config */
  srsran::fill_phy_carrier_cfg(sib1.serving_cell_cfg_common, &phy_cfg.carrier);

  /* Apply SSB Config */
  srsran::fill_phy_ssb_cfg(sib1.serving_cell_cfg_common, &phy_cfg.ssb);
  /* Apply n-TimingAdvanceOffset */
  if (sib1.serving_cell_cfg_common.n_timing_advance_offset_present) {
    switch (sib1.serving_cell_cfg_common.n_timing_advance_offset.value) {
      case asn1::rrc_nr::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n0:
        phy_cfg.t_offset = 0;
        break;
      case asn1::rrc_nr::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n25600:
        phy_cfg.t_offset = 25600;
        break;
      case asn1::rrc_nr::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n39936:
        phy_cfg.t_offset = 39936;
        break;
      default:
        break;
    }
  } else {
    phy_cfg.t_offset = 25600;
  }
  if (sib1.serving_cell_cfg_common.tdd_ul_dl_cfg_common_present) {
    srsran::make_phy_tdd_cfg(sib1.serving_cell_cfg_common.tdd_ul_dl_cfg_common, &phy_cfg.duplex);
  }
}


/* Load mib configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_mib(srsran::phy_cfg_nr_t& phy_cfg, std::string& filename, uint32_t ncellid)
{
  srsran_mib_nr_t mib = {};
  if (!read_raw_config(filename, (uint8_t*)&mib, sizeof(srsran_mib_nr_t))) {
    return false;
  }
  if (!update_phy_cfg_from_mib(phy_cfg, mib, ncellid)) {
    return false;
  }
  return true;
}


/* Load SIB1 configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_sib1(srsran::phy_cfg_nr_t& phy_cfg, std::string& filename, uint32_t nbits)
{
  std::vector<uint8_t> sib1_raw(nbits);
  if (!read_raw_config(filename, sib1_raw.data(), nbits)) {
    printf("Failed to read SIB1 from %s\n", filename.c_str());
    return false;
  }

  asn1::rrc_nr::sib1_s sib1;
  if (!parse_to_sib1(sib1_raw.data(), nbits, sib1)) {
    printf("Failed to parse SIB1\n");
    return false;
  }
  update_phy_cfg_from_sib1(phy_cfg, sib1);
  return true;
}
/* Read binary form configuration dumped structure */
bool read_raw_config(const std::string& filename, uint8_t* buffer, size_t size)
{
  std::ifstream infile(filename, std::ios::binary);
  if (!infile.is_open()) {
    return false;
  }
  infile.read(reinterpret_cast<char*>(buffer), size);
  return true;
}
/* Decode SIB1 bytes to asn1 structure */
bool parse_to_sib1(uint8_t* data, uint32_t len, asn1::rrc_nr::sib1_s& sib1)
{
  asn1::rrc_nr::bcch_dl_sch_msg_s dlsch_msg;
  asn1::cbit_ref                  dlsch_bref(data, len);
  asn1::SRSASN_CODE               err = dlsch_msg.unpack(dlsch_bref);
  if (err != asn1::SRSASN_SUCCESS ||
      dlsch_msg.msg.type().value != asn1::rrc_nr::bcch_dl_sch_msg_type_c::types_opts::c1) {
    std::cerr << "Error unpacking BCCH-BCH message\n";
    return false;
  }
  sib1 = dlsch_msg.msg.c1().sib_type1();
  return true;
}