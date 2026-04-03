#include "shadower/source/source.h"
#include "shadower/utils/utils.h"
#include "srsran/common/band_helper.h"
#include "srsran/phy/sync/ssb.h"
#include <cmath>
#include <csignal>
#include <deque>
#include <getopt.h>

namespace {

volatile std::sig_atomic_t running = 1;

struct MonitorConfig {
  double                      center_freq_hz = 2565e6;
  double                      ssb_freq_hz    = 2565e6;
  double                      sample_rate_hz = 184.32e6;
  uint32_t                    band           = 41;
  uint32_t                    rx_gain        = 40;
  uint32_t                    window         = 50;
  double                      report_sec     = 1.0;
  std::string                 source_params  = "type=x300,master_clock_rate=184.32e6";
  std::string                 config_file;
  srsran_subcarrier_spacing_t scs = srsran_subcarrier_spacing_30kHz;
};

struct RollingStats {
  std::deque<float> snr;
  std::deque<float> cfo;
  std::deque<float> rsrp;
  uint64_t          total_samples = 0;
  uint64_t          valid_cells   = 0;
  uint64_t          invalid_cells = 0;
};

void on_signal(int)
{
  running = 0;
}

float mean(const std::deque<float>& values)
{
  if (values.empty()) {
    return NAN;
  }
  float sum = 0.0f;
  for (float v : values) {
    sum += v;
  }
  return sum / values.size();
}

float min_value(const std::deque<float>& values)
{
  if (values.empty()) {
    return NAN;
  }
  float min_v = values.front();
  for (float v : values) {
    min_v = std::min(min_v, v);
  }
  return min_v;
}

float max_value(const std::deque<float>& values)
{
  if (values.empty()) {
    return NAN;
  }
  float max_v = values.front();
  for (float v : values) {
    max_v = std::max(max_v, v);
  }
  return max_v;
}

void push_value(std::deque<float>& values, float value, uint32_t limit)
{
  values.push_back(value);
  while (values.size() > limit) {
    values.pop_front();
  }
}

void print_value_line(const char* label, const std::deque<float>& values, const char* unit)
{
  if (values.empty()) {
    printf("  %-4s last=%8s avg=%8s min=%8s max=%8s %s\n", label, "n/a", "n/a", "n/a", "n/a", unit);
    return;
  }

  printf("  %-4s last=%+8.2f avg=%+8.2f min=%+8.2f max=%+8.2f %s\n",
         label,
         values.back(),
         mean(values),
         min_value(values),
         max_value(values),
         unit);
}

void print_report(const MonitorConfig& cfg, const RollingStats& stats)
{
  printf("\033[2J\033[H");
  printf("Live signal monitor for X310\n");
  printf("  Center freq: %.3f MHz\n", cfg.center_freq_hz / 1e6);
  printf("  SSB freq: %.3f MHz\n", cfg.ssb_freq_hz / 1e6);
  printf("  Sample rate: %.2f MHz\n", cfg.sample_rate_hz / 1e6);
  printf("  RX gain: %u dB\n", cfg.rx_gain);
  printf("  Window: %u valid cells\n", cfg.window);
  printf("\n");
  printf("  Samples: %lu  valid: %lu  invalid: %lu  valid_ratio: %.2f%%\n",
         stats.total_samples,
         stats.valid_cells,
         stats.invalid_cells,
         stats.total_samples ? (100.0 * stats.valid_cells / stats.total_samples) : 0.0);
  print_value_line("SNR", stats.snr, "dB");
  print_value_line("CFO", stats.cfo, "Hz");
  print_value_line("RSRP", stats.rsrp, "dB");
  printf("\n");
  printf("Ctrl+C to stop\n");
  fflush(stdout);
}

void usage(const char* prog)
{
  printf("Usage: %s [-c <config.yaml>] [-f <ssb_freq_mhz>] [-d <uhd_args>] [options]\n", prog);
  printf("  -c <path>  Load center freq / SSB freq / sample rate / gain / source params from shadower YAML\n");
  printf("  -f <MHz>   SSB frequency in MHz\n");
  printf("  -d <str>   UHD source parameters, e.g. type=x300,time_source=gpsdo,master_clock_rate=184.32e6\n");
  printf("  -s <MHz>   Sample rate in MHz, default 184.32\n");
  printf("  -b <band>  NR band, default 41\n");
  printf("  -S <scs>   SSB SCS in kHz string, default 30\n");
  printf("  -g <dB>    RX gain, default 40\n");
  printf("  -w <n>     Rolling window length, default 50\n");
  printf("  -r <sec>   Report interval in seconds, default 1.0\n");
}

bool parse_args(int argc, char* argv[], MonitorConfig& cfg)
{
  int opt;
  while ((opt = getopt(argc, argv, "c:f:d:s:b:S:g:w:r:h")) != -1) {
    switch (opt) {
      case 'c':
        cfg.config_file = optarg;
        break;
      case 'f':
        cfg.ssb_freq_hz = atof(optarg) * 1e6;
        break;
      case 'd':
        cfg.source_params = optarg;
        break;
      case 's':
        cfg.sample_rate_hz = atof(optarg) * 1e6;
        break;
      case 'b':
        cfg.band = static_cast<uint32_t>(atoi(optarg));
        break;
      case 'S':
        cfg.scs = srsran_subcarrier_spacing_from_str(optarg);
        break;
      case 'g':
        cfg.rx_gain = static_cast<uint32_t>(atoi(optarg));
        break;
      case 'w':
        cfg.window = static_cast<uint32_t>(atoi(optarg));
        break;
      case 'r':
        cfg.report_sec = atof(optarg);
        break;
      case 'h':
      default:
        usage(argv[0]);
        return false;
    }
  }

  if (cfg.ssb_freq_hz <= 0 || cfg.sample_rate_hz <= 0 || cfg.report_sec <= 0 || cfg.window == 0 ||
      cfg.source_params.empty()) {
    usage(argv[0]);
    return false;
  }
  return true;
}

bool load_from_config_file(MonitorConfig& cfg)
{
  if (cfg.config_file.empty()) {
    cfg.center_freq_hz = cfg.ssb_freq_hz;
    return true;
  }

  ShadowerConfig parsed = {};
  char*          argv[] = {const_cast<char*>("signal_monitor"), const_cast<char*>(cfg.config_file.c_str())};
  if (parse_args(parsed, 2, argv) != SRSRAN_SUCCESS) {
    return false;
  }

  if (parsed.channels.empty()) {
    fprintf(stderr, "Config file has no enabled RF channel\n");
    return false;
  }
  cfg.center_freq_hz = parsed.channels[0].rx_frequency;
  cfg.ssb_freq_hz    = parsed.ssb_freq;
  cfg.sample_rate_hz = parsed.sample_rate;
  cfg.band           = parsed.band;
  cfg.rx_gain        = static_cast<uint32_t>(parsed.channels[0].rx_gain);
  cfg.scs            = parsed.scs_ssb;
  if (parsed.source_type == "uhd" && !parsed.source_params.empty()) {
    cfg.source_params = parsed.source_params;
  }
  return true;
}

} // namespace

int main(int argc, char* argv[])
{
  MonitorConfig cfg = {};
  if (!parse_args(argc, argv, cfg)) {
    return 1;
  }
  if (!load_from_config_file(cfg)) {
    return 1;
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  ShadowerConfig log_cfg = {};
  log_cfg.log_level      = srslog::basic_levels::error;
  srslog::basic_logger& logger = srslog_init(&log_cfg);

  srsran::srsran_band_helper band_helper;
  srsran_ssb_pattern_t       ssb_pattern = band_helper.get_ssb_pattern(cfg.band, cfg.scs);
  srsran_duplex_mode_t       duplex_mode = band_helper.get_duplex_mode(cfg.band);

  srsran_ssb_t ssb = {};
  if (!init_ssb(ssb, cfg.sample_rate_hz, cfg.center_freq_hz, cfg.ssb_freq_hz, cfg.scs, ssb_pattern, duplex_mode)) {
    logger.error("Failed to initialize SSB monitor");
    return 1;
  }

  ShadowerConfig source_cfg = {};
  source_cfg.source_type    = "uhd";
  source_cfg.source_module  = uhd_source_module_path;
  source_cfg.source_params  = cfg.source_params;
  source_cfg.sample_rate    = cfg.sample_rate_hz;
  source_cfg.channels.resize(1);
  source_cfg.channels[0].rx_frequency = cfg.center_freq_hz;
  source_cfg.channels[0].tx_frequency = cfg.center_freq_hz;
  source_cfg.channels[0].rx_gain      = cfg.rx_gain;
  source_cfg.channels[0].tx_gain      = 0;

  create_source_t creator = load_source(uhd_source_module_path);
  Source*         source  = creator(source_cfg);
  source->set_rx_freq(cfg.center_freq_hz);

  uint32_t sf_len = static_cast<uint32_t>(cfg.sample_rate_hz * SF_DURATION);
  cf_t*    buffer = srsran_vec_cf_malloc(sf_len);
  cf_t*    rx_buffer[SRSRAN_MAX_CHANNELS] = {};
  rx_buffer[0]                            = buffer;
  srsran_timestamp_t ts                   = {};
  RollingStats stats                      = {};
  double report_period_sec                = cfg.report_sec;
  auto   next_report                      = std::chrono::steady_clock::now();

  while (running) {
    source->recv(rx_buffer, sf_len, &ts);

    srsran_ssb_search_res_t res = {};
    if (srsran_ssb_search(&ssb, buffer, sf_len, &res) < SRSRAN_SUCCESS) {
      continue;
    }

    stats.total_samples += 1;
    if (!res.pbch_msg.crc || res.measurements.snr_dB < -10.0f) {
      stats.invalid_cells += 1;
    } else {
      stats.valid_cells += 1;
      push_value(stats.snr, res.measurements.snr_dB, cfg.window);
      push_value(stats.cfo, res.measurements.cfo_hz, cfg.window);
      push_value(stats.rsrp, res.measurements.rsrp_dB, cfg.window);
    }

    auto now = std::chrono::steady_clock::now();
    if (now >= next_report) {
      print_report(cfg, stats);
      next_report = now + std::chrono::milliseconds(static_cast<int64_t>(report_period_sec * 1000));
    }
  }

  source->close();
  srsran_ssb_free(&ssb);
  free(buffer);
  return 0;
}
