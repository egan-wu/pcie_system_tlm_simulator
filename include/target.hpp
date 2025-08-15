#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <vector>
#include "config.hpp"
#include "pcie_tlp_extension.hpp"

struct Target : sc_core::sc_module,
                public tlm::tlm_fw_transport_if<>
{
    tlm_utils::simple_target_socket<Target> socket;
    tlm_utils::peq_with_cb_and_phase<Target> m_peq;
    PCIeConfig& config;

    // component
    std::vector<uint8_t> memory;

    SC_CTOR(Target, PCIeConfig& cfg) 
    : socket("socket"), 
      m_peq(this, &Target::peq_callback),
      config(cfg)
    {
        socket.register_nb_transport_fw(this, &Target::nb_transport_fw);
        std::cout << "Target constructed done at " << sc_core::sc_time_stamp() << std::endl;

        memory.resize(4096, 0x55);
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
        // unsigned int initiator_id = trans.get_extension<InitiatorIDExtension>()->initiator_id;
        unsigned int initiator_id = trans.get_extension<PCIeTLPExtension>()->get_device_id();
        if (phase == tlm::BEGIN_REQ) {
            std::cout << "[Target] Received BEGIN_REQ from initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            sc_core::sc_time resp_delay = config.target_delay;
            tlm::tlm_phase resp_phase = tlm::BEGIN_RESP;
            socket->nb_transport_bw(trans, resp_phase, resp_delay);
        }
        else if (phase == tlm::END_REQ) {
            std::cout << "[Target] Received END_REQ from initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            sc_core::sc_time resp_delay = config.target_delay;
            tlm::tlm_phase resp_phase = tlm::END_RESP;

            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                unsigned char* data_ptr = trans.get_data_ptr();
                unsigned int data_length = trans.get_data_length();
                memcpy(data_ptr, memory.data(), data_length);
            }

            else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
                unsigned char* data_ptr = trans.get_data_ptr();
                unsigned int data_length = trans.get_data_length();
                memcpy(memory.data(), data_ptr, data_length);
            }

            socket->nb_transport_bw(trans, resp_phase, resp_delay);
        }
        else {
            SC_REPORT_WARNING("Target", "peq_callback received unexpected phase");
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