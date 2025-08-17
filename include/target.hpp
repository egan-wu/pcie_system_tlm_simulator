#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <vector>
#include "config.hpp"
#include "pcie_tlp_extension.hpp"

#define MEMORY_SIZE  0x1000  // bytes

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
        memory.resize(MEMORY_SIZE, 0x00);    
    };

    void peq_callback(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase );

    // tlm_fw_transport_if override function
    tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay);
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
};