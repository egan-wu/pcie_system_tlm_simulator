#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

using namespace sc_core;

struct Initiator : sc_module {
    tlm_utils::simple_initiator_socket<Initiator> socket;

    SC_CTOR(Initiator) : socket("socket") {
        SC_THREAD(process);
    }

    void process() {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;

        uint32_t data = 0x12345678;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0x1000);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(sizeof(data));
        trans.set_streaming_width(sizeof(data));
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(trans, delay);

        if (trans.is_response_error()) {
            SC_REPORT_ERROR("TLM-2", "Response error from b_transport");
        }

        std::cout << "[Initiator] Sent write, delay: " << delay << std::endl;
    }
};