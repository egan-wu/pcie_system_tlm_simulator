#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include "config.hpp"

using namespace sc_core;

struct Target : sc_module {
    tlm_utils::simple_target_socket<Target> socket;
    PCIeConfig& config;

    SC_CTOR(Target, PCIeConfig& cfg) 
    : socket("socket"), config(cfg) 
    {
        socket.register_b_transport(this, &Target::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
        delay += config.target_delay;

        std::cout << "[Target] Received "
            << (trans.get_command() == tlm::TLM_WRITE_COMMAND ? "WRITE" : "READ")
            << " to address 0x" << std::hex << trans.get_address()
            << ", delay: " << delay << std::endl;

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};