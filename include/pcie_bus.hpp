#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <memory>
#include <vector>
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
    tlm_utils::simple_target_socket<PCIeBus> t_socket;
    // tlm_utils::simple_initiator_socket<PCIeBus> i_socket;
    tlm_utils::peq_with_cb_and_phase<PCIeBus> m_peq;

    // PCIe config
    PCIeConfig& config;

    // PCIe conponents
    std::vector<MapEntry> addr_map;
    std::vector<std::unique_ptr<tlm_utils::simple_initiator_socket<PCIeBus>>> i_sockets;

    SC_CTOR(PCIeBus, PCIeConfig& cfg, unsigned int target_count = 1)
        : t_socket("target_socket"),
          m_peq(this, &PCIeBus::peq_callback),
          config(cfg)
    {
        for (unsigned int i = 0; i < target_count; ++i) {
            std::string initiator_name = std::string("i_socket_") + std::to_string(i);
            auto i_socket = std::make_unique<tlm_utils::simple_initiator_socket<PCIeBus>>(initiator_name.c_str());
            i_socket->register_nb_transport_bw(this, &PCIeBus::nb_transport_bw);
            i_sockets.push_back(std::move(i_socket));
            addr_map.push_back({0, 0});
        }

        t_socket.register_nb_transport_fw(this, &PCIeBus::nb_transport_fw);
    }

    void set_target_map(unsigned int target_idx, uint64_t base, uint64_t size) {
        if (target_idx < addr_map.size()) {
            addr_map[target_idx].base = base;
            addr_map[target_idx].size = size;
        } else {
            SC_REPORT_ERROR("PCIeBus", "Set target index out of range");
        }
        std::cout << "---------------------------------------" << std::endl
                  << "[PCIeBus] Set target map for index " << target_idx 
                  << ": base = " << std::hex << base 
                  << ", size = " << std::dec << size << std::endl;
    }

    int find_target(uint64_t address) {
        for (unsigned int i = 0; i < addr_map.size(); ++i) {
            if (addr_map[i].size == 0) continue;
            if (address >= addr_map[i].base && 
                address < (addr_map[i].base + addr_map[i].size)) {
                std::cout << "[PCIeBus] Found Target Address " << std::hex << address 
                          << " mapped to target index " << i 
                          << " (base = " << addr_map[i].base 
                          << ", size = " << addr_map[i].size << ")" << std::dec << std::endl;
                return i;
            }
        }
        return -1;
    }

    tlm::tlm_sync_enum nb_transport_fw(
        tlm::tlm_generic_payload& trans,
        tlm::tlm_phase& phase,
        sc_core::sc_time& delay ) override
    {
        m_peq.notify(trans, phase, delay);
        return tlm::TLM_ACCEPTED;
    }

    tlm::tlm_sync_enum nb_transport_bw(
        tlm::tlm_generic_payload& trans,
        tlm::tlm_phase& phase,
        sc_core::sc_time& delay) override
    {
        std::cout << "[PCIeBus] Received nb_transport_bw phase = " << phase << std::endl;
        return t_socket->nb_transport_bw(trans, phase, delay);
    }

    void peq_callback(
        tlm::tlm_generic_payload& trans, 
        const tlm::tlm_phase& phase) 
    {
        if (phase == tlm::BEGIN_REQ) {
            uint64_t addr = trans.get_address();
            int tidx = find_target(addr);
            if (tidx < 0) {
                SC_REPORT_WARNING("PCIeBus", "Address not mapped to any target");
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                tlm::tlm_phase resp_phase = tlm::BEGIN_RESP;
                sc_core::sc_time zero_delay = sc_core::SC_ZERO_TIME;
                t_socket->nb_transport_bw(trans, resp_phase, zero_delay);
                return;
            }

            std::cout << "[PCIeBus] Forwarding BEGIN_REQ to target " << tidx << " at " << sc_core::sc_time_stamp() << std::endl;
            sc_core::sc_time bus_delay = config.bus_delay;
            tlm::tlm_phase fw_phase = tlm::BEGIN_REQ;
            (*i_sockets[tidx])->nb_transport_fw(trans, fw_phase, bus_delay);

        } else if (phase == tlm::END_RESP) {
            std::cout << "[PCIeBus] Backward END_RESP (ignored)\n";
        }
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) override {
        SC_REPORT_WARNING("PCIeBus", "b_transport not implemented");
        (void)trans;
        (void)delay;
    }

    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) override {
        (void)trans;
        (void)dmi_data;
        return false;
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) override {
        (void)trans;
        return 0;
    }

    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) override {
       // not implemented
       (void)start_range;
       (void)end_range;
    }
};