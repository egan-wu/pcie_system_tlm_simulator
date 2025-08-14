#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include "initiator_id_extension.hpp"

struct Initiator : sc_core::sc_module,
                   public tlm::tlm_bw_transport_if<>
{
    tlm_utils::simple_initiator_socket<Initiator> socket;
    tlm_utils::peq_with_cb_and_phase<Initiator> m_peq;

    unsigned int initiator_id;

    // metrics
    uint32_t read_byte_count = 0;
    sc_core::sc_time start_time;
    sc_core::sc_time end_time;

    SC_CTOR(Initiator, unsigned int id) 
    : socket("socket"),
      m_peq(this, &Initiator::peq_callback),
      initiator_id(id)
    {
        socket.register_nb_transport_bw(this, &Initiator::nb_transport_bw);
        SC_THREAD(process);
        std::cout << "Initiator " << initiator_id << " constructed done at " << sc_core::sc_time_stamp() << std::endl;
    }

    void process() {
        start_time = sc_core::sc_time_stamp();
        for (uint32_t addr = 0x200; addr < 0x2000; addr+=0x2000) {
            tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
            sc_time delay = SC_ZERO_TIME;

            // allocate data buffer
            uint32_t data_byte_length = 64;
            std::vector<uint8_t> *data_buffer = new std::vector<uint8_t>(data_byte_length * sizeof(uint32_t), 0x00);

            // uint32_t data = 0x12345678;
            trans->set_command(tlm::TLM_READ_COMMAND);
            trans->set_address(addr);
            trans->set_data_ptr(reinterpret_cast<unsigned char*>(&data));
            trans->set_data_length(data_byte_length);
            trans->set_data_ptr(reinterpret_cast<unsigned char*>(data_buffer->data()));
            trans->set_streaming_width(sizeof(data));
            trans->set_byte_enable_ptr(0);
            trans->set_dmi_allowed(false);
            trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            InitiatorIDExtension* ext = new InitiatorIDExtension();
            ext->initiator_id = initiator_id;
            trans->set_extension(ext);

            tlm::tlm_phase phase = tlm::BEGIN_REQ;

            std::cout << "[Initiator-" << initiator_id << "] Sending BEGIN_REQ to address: " << std::hex << trans->get_address() << std::dec << " at " << sc_core::sc_time_stamp() << std::endl;
            socket->nb_transport_fw(*trans, phase, delay);
        
            wait(5, sc_core::SC_NS);
        }
    }

    void peq_callback (
        tlm::tlm_generic_payload& trans, 
        const tlm::tlm_phase& phase) 
    {
        if (phase == tlm::BEGIN_RESP) {
            std::cout << "[Initiator-" << initiator_id << "] Received BEGIN_RESP at " << sc_core::sc_time_stamp() << std::endl;
            sc_core::sc_time delay = sc_core::sc_time(5, sc_core::SC_NS);
            tlm::tlm_phase end_phase = tlm::END_REQ;
            std::cout << "[Initiator-" << initiator_id << "] Sending END_REQ at " << sc_core::sc_time_stamp() << std::endl;
            socket->nb_transport_fw(trans, end_phase, delay);
        }
        else if (phase == tlm::END_RESP) {
            std::cout << "[Initiator-" << initiator_id << "] Received END_RESP at " << sc_core::sc_time_stamp() << std::endl;

            unsigned char* data_ptr = trans.get_data_ptr();
            unsigned int data_length = trans.get_data_length();
            // int8_t* data_ptr_int = reinterpret_cast<int8_t*>(data_ptr);
            // for (unsigned int i = 0; i < data_length; ++i) {
            //     std::cout << "Data[" << i << "]: " << std::hex << static_cast<int>(data_ptr_int[i]) << std::dec;
            // }
            // std::cout << std::endl;
            delete(data_ptr);

            read_byte_count += data_length;
            end_time = sc_core::sc_time_stamp();
            double elapsed_time = (end_time - start_time).to_seconds();
            double throughput_MBps = (read_byte_count / elapsed_time) / (1024*1024);
            std::cout << "[Throughput] " << throughput_MBps << " MB/s for initiator-" << initiator_id << ", elapse_time=" << elapsed_time << std::endl;;
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