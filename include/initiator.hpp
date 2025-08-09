#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>

struct Initiator : sc_core::sc_module,
                   public tlm::tlm_bw_transport_if<>
{
    tlm_utils::simple_initiator_socket<Initiator> socket;
    tlm_utils::peq_with_cb_and_phase<Initiator> m_peq;

    SC_CTOR(Initiator) 
    : socket("socket"),
      m_peq(this, &Initiator::peq_callback)
    {
        socket.register_nb_transport_bw(this, &Initiator::nb_transport_bw);
        SC_THREAD(process);
    }

    void process() {
        for (uint32_t addr = 0x200; addr < 0x2000; addr+=0x100) {
            tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
            sc_time delay = SC_ZERO_TIME;

            uint32_t data = 0x12345678;
            trans->set_command(tlm::TLM_WRITE_COMMAND);
            trans->set_address(addr);
            trans->set_data_ptr(reinterpret_cast<unsigned char*>(&data));
            trans->set_data_length(sizeof(data));
            trans->set_streaming_width(sizeof(data));
            trans->set_byte_enable_ptr(0);
            trans->set_dmi_allowed(false);
            trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            tlm::tlm_phase phase = tlm::BEGIN_REQ;

            std::cout << "[Initiator] Sending BEGIN_REQ to address: " << std::hex << trans->get_address() << std::dec << " at " << sc_core::sc_time_stamp() << std::endl;
            socket->nb_transport_fw(*trans, phase, delay);
        }

    }

    void peq_callback (
        tlm::tlm_generic_payload& trans, 
        const tlm::tlm_phase& phase) 
    {
        if (phase == tlm::BEGIN_RESP) {
            std::cout << "[Initiator] Received BEGIN_RESP at " << sc_core::sc_time_stamp() << std::endl;
            sc_core::sc_time delay = sc_core::sc_time(5, sc_core::SC_NS);
            tlm::tlm_phase end_phase = tlm::END_REQ;
            socket->nb_transport_fw(trans, end_phase, delay);
        }
        else if (phase == tlm::END_RESP) {
            std::cout << "[Initiator] Received END_RESP at " << sc_core::sc_time_stamp() << std::endl;
        }
        else {
            SC_REPORT_ERROR("Initiator", "peq_callback received unexpected phase");
        }
    }

    tlm::tlm_sync_enum nb_transport_bw(
        tlm::tlm_generic_payload& trans,
        tlm::tlm_phase& phase,
        sc_core::sc_time& delay) override
    {
        m_peq.notify(trans, phase, delay);
        return tlm::TLM_COMPLETED;
    }

    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) override {
        // not implemented
        (void)start_range;
        (void)end_range;
    }
};