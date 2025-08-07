#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include "config.hpp"

struct PCIeBus : sc_module {
    // TLM objects
    tlm_utils::simple_target_socket<PCIeBus> t_socket;
    tlm_utils::simple_initiator_socket<PCIeBus> i_socket;

    // PCIe config
    PCIeConfig& config;

    SC_CTOR(PCIeBus, PCIeConfig& cfg)
    : config(cfg), t_socket("target_socket"), i_socket("initiator_socket")
    {
        t_socket.register_b_transport(this, &PCIeBus::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
        delay += config.bus_delay;
        i_socket->b_transport(trans, delay);
    }
};