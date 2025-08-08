#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include "config.hpp"

struct PCIeBus : sc_core::sc_module, 
                 public tlm::tlm_fw_transport_if<>,
                 public tlm::tlm_bw_transport_if<>
{
    // TLM objects
    tlm_utils::simple_target_socket<PCIeBus> t_socket;
    tlm_utils::simple_initiator_socket<PCIeBus> i_socket;
    tlm_utils::peq_with_cb_and_phase<PCIeBus> m_peq;

    // PCIe config
    PCIeConfig& config;

    SC_CTOR(PCIeBus, PCIeConfig& cfg)
        : config(cfg), 
          t_socket("target_socket"), 
          i_socket("initiator_socket"), 
          m_peq(this, &PCIeBus::peq_callback)
    {
        t_socket.register_nb_transport_fw(this, &PCIeBus::nb_transport_fw);
        i_socket.register_nb_transport_bw(this, &PCIeBus::nb_transport_bw);
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
            std::cout << "[PCIeBus] Forward BEGIN_REQ at " << sc_core::sc_time_stamp() << std::endl;
            sc_core::sc_time delay = config.bus_delay;
            tlm::tlm_phase fw_phase = tlm::BEGIN_REQ;
            i_socket->nb_transport_fw(trans, fw_phase, delay);

            // 不處理 return status（可擴充）
        } else if (phase == tlm::END_RESP) {
            std::cout << "[PCIeBus] Backward END_RESP (ignored)\n";
        }
    }

    // 必須實作，否則變成 abstract class
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) override {
        // 如果你不用 blocking transport，可以空實作或報錯
        SC_REPORT_WARNING("PCIeBus", "b_transport not implemented");
    }

    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) override {
        return false; // 不支援 DMI
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) override {
        return 0; // 不做 debug transport
    }

    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) override {
        // 不支援 DMI，不需實作
    }
};