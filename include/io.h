#pragma once

#include "util.h"

namespace IO {

class IOSystem {
public:
  IOSystem(
      const string &input_dir, graph &finest, fpga &fpgas,
      flat_hash_map<string, int> &fpga_map,
      flat_hash_map<string, int> &node_map,
      flat_hash_map<int, string> &fpga_reverse_map,
      flat_hash_map<int, string> &node_reverse_map);
  void write_design_fpga_out(
      const graph &finest, const string &ouput_file, vector<int> &parts,
      const flat_hash_map<int, string> &fpga_reverse_map,
      const flat_hash_map<int, string> &node_reverse_map);

private:
  void read_design_info(
      const string &file_path, fpga &fpgas,
      flat_hash_map<string, int> &fpga_map,
      flat_hash_map<int, string> &fpga_reverse_map);
  void read_design_are(
      const string &file_path, graph &finest,
      flat_hash_map<string, int> &node_map,
      flat_hash_map<int, string> &node_reverse_map);
  void read_design_net(
      const string &file_path, graph &finest,
      const flat_hash_map<string, int> &node_map);
  void read_design_topo(
      const string &file_path, fpga &fpgas,
      const flat_hash_map<string, int> &fpga_map);
};

} // namespace IO
