#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include "initiator_id_extension.hpp"
#include "pcie_tlp_extension.hpp"
#include "config.hpp"

struct MapEntry {
    uint64_t base;
    uint64_t size;
};

struct PCIeBus : sc_core::sc_module, 
                 public tlm::tlm_fw_transport_if<>,
                 public tlm::tlm_bw_transport_if<>
{
    // TLM objects
    tlm_utils::peq_with_cb_and_phase<PCIeBus> m_peq;

    // PCIe config
    PCIeConfig& config;

    // PCIe conponents
    std::vector<MapEntry> addr_map;
    std::vector<std::unique_ptr<tlm_utils::simple_initiator_socket<PCIeBus>>> i_sockets;
    std::vector<std::unique_ptr<tlm_utils::simple_target_socket<PCIeBus>>> t_sockets;
    std::queue<std::pair<tlm::tlm_generic_payload*, tlm::tlm_phase>> trans_queue;
    sc_core::sc_event queue_event;

    // trans_map < (RequesterID, Tag), TransactionInfo>
    std::unordered_map<tlm::tlm_generic_payload*, std::pair<int, int>> trans_map;
    std::unordered_map<int /* initiator_id */, std::unordered_map<unsigned int /* tag */, int /* target_id */>> tag_map;
    std::queue<unsigned int> arb_queue;
    unsigned int last_served = 0;

    SC_CTOR(PCIeBus, PCIeConfig& cfg, unsigned int initiator_count = 1, unsigned int target_count = 1)
        : m_peq(this, &PCIeBus::peq_callback),
          config(cfg)
    {
        // initialize initiator sockets
        for (unsigned int i = 0; i < target_count; ++i) {
            std::string initiator_name = std::string("i_socket_") + std::to_string(i);
            auto i_socket = std::make_unique<tlm_utils::simple_initiator_socket<PCIeBus>>(initiator_name.c_str());
            i_socket->register_nb_transport_bw(this, &PCIeBus::nb_transport_bw);
            i_sockets.push_back(std::move(i_socket));
            addr_map.push_back({0, 0});
        }

        // initialize target sockets
        for (unsigned int i = 0; i < initiator_count; ++i) {
            std::string target_name = std::string("t_socket_") + std::to_string(i);
            auto t_socket = std::make_unique<tlm_utils::simple_target_socket<PCIeBus>>(target_name.c_str());
            t_socket->register_nb_transport_fw(this, &PCIeBus::nb_transport_fw);
            t_sockets.push_back(std::move(t_socket));
        }
    
        SC_THREAD(process_trans_queue);
    }

    void peq_callback(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);
    void process_trans_queue();
    void set_target_map(unsigned int target_idx, uint64_t base, uint64_t size);
    int find_target(uint64_t address);

    // tlm_fw/bw_transport_if override function
    tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay);
    tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay);
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range);
};