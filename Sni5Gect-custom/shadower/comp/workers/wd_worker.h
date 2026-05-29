#ifndef WD_WORKER_H
#define WD_WORKER_H
#include "shadower/modules/exploit.h"
#include "shadower/utils/constants.h"
#include "srsran/phy/common/phy_common_nr.h"
#include "srsran/srslog/srslog.h"
#include "wdissector.h"
#include <map>
#include <mutex>
/* WD worker host a single instance of wdissector used to decode the packets in sequence*/
class WDWorker
{
public:
  WDWorker(srsran_duplex_mode_t duplex_mode, srslog::basic_levels log_level);
  ~WDWorker() {}
  void process(uint8_t*    data,
               uint32_t    len,
               uint16_t    rnti,
               uint16_t    frame_number,
               uint16_t    slot_number,
               uint32_t    slot_idx,
               direction_t direction,
               Exploit*    exploit);

private:
  srslog::basic_logger& logger = srslog::fetch_basic_logger("wd_worker", false);

  wd_t*                wd;
  std::mutex           mtx;
  srsran_duplex_mode_t duplex_mode;

  // Buffer must hold fake_pcap_header (48B) + fixed fields (~12B) + max MAC PDU.
  // NR 20 MHz (106 PRB) max TBS is ~12,600 bytes; use 16384 for safety.
  static constexpr uint32_t WD_BUFFER_SIZE = 16384;
  uint8_t buffer[WD_BUFFER_SIZE]{};
};

#endif // WD_WORKER_H