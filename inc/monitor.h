#pragma once
#include <array>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#define CYCLE 3

extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;

typedef std::tuple<uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, float, float>
    cache_stats_t; // current_cycle, cpu_retired_inst, cpu_current_cycle, access, hit, miss, block_count

class Monitor
{
private:
  uint32_t cpu_id;
  std::string NAME;
  std::string trace_name;
  uint64_t LIMIT_THRESHOLD;
  std::vector<cache_stats_t> RECORDS;
  cache_stats_t CACHE_STATS; // current_cycle, cpu_retired_inst, cpu_current_cycle, access, hit, miss, block_count
  bool COMPLET_FLAG;
  std::vector<uint32_t> BLOCKS_DATA;
  uint32_t bs;

public:
  bool warmup_flag;
  Monitor(uint32_t cpu_id, uint64_t limit_threshold, std::string name, std::string trace_name, std::string blocks_data, uint32_t block_size)
  {
    this->cpu_id = cpu_id;
    this->NAME = name;
    this->LIMIT_THRESHOLD = limit_threshold;
    this->trace_name = trace_name;
    this->RECORDS = std::vector<cache_stats_t>();
    this->CACHE_STATS = std::make_tuple(0, 0, 0, 0, 0, 0, 0, 0.0, 0.0);
    this->COMPLET_FLAG = false;
    this->bs = block_size;
    if (blocks_data.size() != 0) {
      std::string delimiter = " ";
      size_t pos = 0;
      std::string token;
      while ((pos = blocks_data.find(delimiter)) != std::string::npos) {
        token = blocks_data.substr(0, pos);
        this->BLOCKS_DATA.push_back(std::stoi(token));
        blocks_data.erase(0, pos + delimiter.length());
      }
      this->BLOCKS_DATA.push_back(std::stoi(blocks_data));
    }

    this->warmup_flag = true;
    std::cout << "Monitor: " << this->NAME << " is created cpu_id: " << this->cpu_id << " LIMIT_THRESHOLD: " << this->LIMIT_THRESHOLD
              << " trace_name: " << this->trace_name << " blocks_data: " << this->BLOCKS_DATA.size() << " block_size: " << this->bs << std::endl;
  }

  uint32_t get_block_size()
  {
    auto next_bs = 0;
    auto cycle = this->RECORDS.size();
    if (this->BLOCKS_DATA.size() > cycle) {
      next_bs = this->BLOCKS_DATA[cycle];
    } else {
      next_bs = this->bs;
    }
    return next_bs;
  }

  uint32_t update_cache_stats(uint64_t current_cycle, uint64_t cpu_retired_inst, uint64_t cpu_current_cycle, uint64_t access, uint64_t hit, uint64_t miss,
                              uint64_t block_count, float crycle_ipc, float all_ipc)
  {
    std::get<0>(CACHE_STATS) = current_cycle;
    std::get<1>(CACHE_STATS) = cpu_retired_inst;
    std::get<2>(CACHE_STATS) = cpu_current_cycle;
    std::get<3>(CACHE_STATS) = access - std::get<3>(CACHE_STATS);
    std::get<4>(CACHE_STATS) = hit - std::get<4>(CACHE_STATS);
    std::get<5>(CACHE_STATS) = miss - std::get<5>(CACHE_STATS);
    std::get<6>(CACHE_STATS) = block_count - std::get<6>(CACHE_STATS);
    std::get<7>(CACHE_STATS) = crycle_ipc;
    std::get<8>(CACHE_STATS) = all_ipc;
    this->RECORDS.push_back(this->CACHE_STATS);
    // then update the CACHE_STATS
    this->CACHE_STATS = std::make_tuple(current_cycle, cpu_retired_inst, cpu_current_cycle, access, hit, miss, block_count, crycle_ipc, all_ipc);
    return this->get_block_size();
  }

  uint32_t add_record(uint32_t cpuid, uint64_t current_cycle, uint64_t cpu_retired_inst, uint64_t cpu_current_cycle, uint64_t access, uint64_t hit,
                      uint64_t miss, uint64_t block_count, float crycle_ipc, float all_ipc)
  {

    cache_stats_t current_state = std::make_tuple(current_cycle, cpu_retired_inst, cpu_current_cycle, access, hit, miss, block_count, crycle_ipc, all_ipc);
    if (cpuid != this->cpu_id) {
      // If the cpuid is not the same as the monitor's cpuid, return directly
      std::cout << "ERROR: add_record cpuid: " << cpuid << " is not the same as the monitor's cpuid: " << this->cpu_id << std::endl;
      abort();
    }

    if (this->COMPLET_FLAG) {
      // If the record is complete, return directly
      return this->bs;
    }

    if (std::get<CYCLE>(current_state) % LIMIT_THRESHOLD != 0) {
      // If the number of misses is not a multiple of the threshold, return directly
      return this->bs;
    }
    // campare this record with the previous one
    if (std::get<CYCLE>(this->CACHE_STATS) == std::get<CYCLE>(current_state)) {
      // If the number of misses does not change, it is not recorded
      return this->bs;
    }
    ooo_cpu[cpuid]->next_print_instruction = std::get<CYCLE>(current_state);

    auto current_block_size = update_cache_stats(current_cycle, cpu_retired_inst, cpu_current_cycle, access, hit, miss, block_count, crycle_ipc, all_ipc);
    if (this->warmup_flag) {
      return this->bs;
    }
    return current_block_size;
  }

  void end_record(uint32_t cpuid, uint64_t current_cycle, uint64_t cpu_retired_inst, uint64_t cpu_current_cycle, uint64_t access, uint64_t hit, uint64_t miss,
                  uint64_t block_count)
  {
    std::cout << "end_record cpu: " << cpuid << " Name: " << this->NAME << " current_cycle: " << current_cycle << " cpu_retired_inst: " << cpu_retired_inst
              << " cpu_current_cycle: " << cpu_current_cycle << " access: " << access << " hit: " << hit << " miss: " << miss << " block_count: " << block_count
              << std::endl;
    if (cpuid != this->cpu_id) {
      // If the cpuid is not the same as the monitor's cpuid, return directly
      std::cout << "ERROR: end_record cpuid: " << cpuid << " is not the same as the monitor's cpuid: " << this->cpu_id << std::endl;
      abort();
    }

    if (this->COMPLET_FLAG) {
      // If the record is complete, return directly
      std::cout << "ERROR: END record << " << this->NAME << " >> of CPU " << cpuid << " has been completed." << std::endl;
      return;
    }

    this->COMPLET_FLAG = 1;

    if (access == 0) {
      // If the number of accesses, hits, misses, and block counts is 0, return directly
      return;
    }

    // if (std::get<5>(this->CACHE_STATS) != 0) {
    //   this->RECORDS.push_back(this->CACHE_STATS);
    // }
    this->CACHE_STATS = std::make_tuple(current_cycle, cpu_retired_inst, cpu_current_cycle, access, hit, miss, block_count, 0, 0);

    // if (miss != 0 && miss % LIMIT_THRESHOLD == 0) {
    //   this->RECORDS.push_back(this->CACHE_STATS);
    // }

    return;
  }

  std::string to_json_string()
  {
    if (std::get<5>(this->CACHE_STATS) == 0 && this->RECORDS.size() == 0) {
      return " ";
    }
    std::string json_str = "{";
    json_str += "\"CacheName\":\"" + this->NAME + "\",";
    json_str += "\"BlockSize\":" + std::to_string(BLOCK_SIZE) + ",";
    json_str += "\"ActualBlockSize\":"
                + (this->warmup_flag ? std::to_string(this->bs) : (this->BLOCKS_DATA.size() == 0 ? std::to_string(this->bs) : "\"CaL_Drive\"")) + ",";
    json_str += "\"TraceName\":\"" + this->trace_name + "\",";
    json_str += "\"CpuId\":" + std::to_string(this->cpu_id) + ",";
    json_str += "\"LeastState\": {";
    {
      json_str += "\"CurrentCycle\":" + std::to_string(std::get<0>(this->CACHE_STATS)) + ",";
      json_str += "\"CpuRetiredInst\":" + std::to_string(std::get<1>(this->CACHE_STATS)) + ",";
      json_str += "\"CpuCurrentCycle\":" + std::to_string(std::get<2>(this->CACHE_STATS)) + ",";
      json_str += "\"Access\":" + std::to_string(std::get<3>(this->CACHE_STATS)) + ",";
      json_str += "\"Hit\":" + std::to_string(std::get<4>(this->CACHE_STATS)) + ",";
      json_str += "\"Miss\":" + std::to_string(std::get<5>(this->CACHE_STATS)) + ",";
      json_str += "\"BlockCount\":" + std::to_string(std::get<6>(this->CACHE_STATS)) + ",";
      json_str += "\"Hit_rate\":" + std::to_string(((float)(std::get<4>(this->CACHE_STATS))) / std::get<3>(this->CACHE_STATS)) + ",";
      json_str += "\"Miss_rate\":" + std::to_string(((float)(std::get<5>(this->CACHE_STATS))) / std::get<3>(this->CACHE_STATS)) + ",";
      json_str += "\"CaL\":" + std::to_string(((float)(std::get<3>(this->CACHE_STATS))) / (std::get<6>(this->CACHE_STATS) * BLOCK_SIZE));
      json_str += "},";
    }
    json_str += "\"Data\":[";
    {
      int i = 0;
      for (auto record : this->RECORDS) {
        json_str += "{";
        json_str += "\"count\":" + std::to_string(i) + ",";
        json_str += "\"current_cycle\":" + std::to_string(std::get<0>(record)) + ",";
        json_str += "\"cpu_retired_inst\":" + std::to_string(std::get<1>(record)) + ",";
        json_str += "\"cpu_current_cycle\":" + std::to_string(std::get<2>(record)) + ",";
        json_str += "\"access\":" + std::to_string(std::get<3>(record)) + ",";
        json_str += "\"hit\":" + std::to_string(std::get<4>(record)) + ",";
        json_str += "\"miss\":" + std::to_string(std::get<5>(record)) + ",";
        json_str += "\"block_count\":" + std::to_string(std::get<6>(record)) + ",";
        json_str += "\"crycle_ipc\":" + std::to_string(std::get<7>(record)) + ",";
        json_str += "\"all_ipc\":" + std::to_string(std::get<8>(record)) + ",";
        json_str += "\"Hit_rate\":" + std::to_string((float)std::get<4>(record) / std::get<3>(record)) + ",";
        json_str += "\"Miss_rate\":" + std::to_string((float)std::get<5>(record) / std::get<3>(record)) + ",";
        // CaL = Access / BlockCount / BlockSize
        json_str += "\"CaL\":" + std::to_string(((float)(std::get<3>(record))) / (std::get<6>(record) * BLOCK_SIZE));
        json_str += "},";
        i++;
      }
      json_str.pop_back();
    }
    json_str += "]";
    json_str += "}";
    return json_str;
  }

  void save_and_clear(std::string file_folder, std::string mark, bool warmup_flag)
  {
    std::ofstream result_file;
    std::string file_name = file_folder;
    if (this->BLOCKS_DATA.size() != 0) {
      file_name = file_name + this->trace_name + "_CaL-Drive" + "_" + this->NAME + "_LOAD_" + mark + "_cpu_" + std::to_string(this->cpu_id) + ".json";
    } else {
      file_name =
          file_name + this->trace_name + "_" + std::to_string(this->bs) + "_" + this->NAME + "_LOAD_" + mark + "cpu_" + std::to_string(this->cpu_id) + ".json";
    }

    result_file.open(file_name, std::ios::out | std::ios::app);
    std::cout << "result: " << mark << " save to: " << file_name << std::endl;
    result_file << this->to_json_string() << std::endl;
    result_file.close();
    {
      if (std::get<CYCLE>(this->CACHE_STATS) != 0)
        std::cout << "cache_stats: " << std::get<0>(this->CACHE_STATS) << " " << std::get<1>(this->CACHE_STATS) << " " << std::get<2>(this->CACHE_STATS) << " "
                  << std::get<3>(this->CACHE_STATS) << " " << std::get<4>(this->CACHE_STATS) << " " << std::get<5>(this->CACHE_STATS) << " "
                  << std::get<6>(this->CACHE_STATS) << std::endl;
    }
    cout << "save_and_clear" << endl;
    this->RECORDS.clear();
    this->CACHE_STATS = std::make_tuple(0, 0, 0, 0, 0, 0, 0, 0.0, 0.0);
    this->COMPLET_FLAG = false;
    this->warmup_flag = warmup_flag;
  }
};