#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include "config.hpp"

struct Target : sc_core::sc_module,
                public tlm::tlm_fw_transport_if<>
{
    tlm_utils::simple_target_socket<Target> socket;
    tlm_utils::peq_with_cb_and_phase<Target> m_peq;
    PCIeConfig& config;

    SC_CTOR(Target, PCIeConfig& cfg) 
    : socket("socket"), 
      m_peq(this, &Target::peq_callback),
      config(cfg)
    {
        socket.register_nb_transport_fw(this, &Target::nb_transport_fw);
    }

    tlm::tlm_sync_enum nb_transport_fw(
        tlm::tlm_generic_payload& trans,
        tlm::tlm_phase& phase,
        sc_core::sc_time& delay) override
    {
        m_peq.notify(trans, phase, delay);
        return tlm::TLM_ACCEPTED;
    }

    void peq_callback(
        tlm::tlm_generic_payload& trans,
        const tlm::tlm_phase& phase )
    {
        if (phase == tlm::BEGIN_REQ) {
            std::cout << "[Target] Received BEGIN_REQ at " << sc_core::sc_time_stamp() << std::endl;
            sc_core::sc_time resp_delay = config.target_delay;
            tlm::tlm_phase resp_phase = tlm::BEGIN_RESP;
            socket->nb_transport_bw(trans, resp_phase, resp_delay);
        }
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) override {
        (void)trans;
        (void)delay;
        SC_REPORT_WARNING("Target", "b_transport not implemented");
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
};