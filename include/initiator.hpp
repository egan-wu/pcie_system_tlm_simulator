#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <queue>
#include "initiator_id_extension.hpp"
#include "pcie_tlp_extension.hpp"

class TagPool {
public:
    TagPool() {
        for (uint32_t i = 0; i < (1 << 8); i++) {
            free_tags.push(static_cast<uint8_t>(i));
        }
    }

    bool empty() const {
        return free_tags.empty();
    }

    uint8_t allocate_tag() {
        uint8_t tag = free_tags.front();
        free_tags.pop();
        return tag;
    }

    void release_tag(uint8_t tag) {
        free_tags.push(tag);
    }

private:
    std::queue<uint8_t> free_tags;
};

struct Initiator : sc_core::sc_module,
                   public tlm::tlm_bw_transport_if<>
{
    tlm_utils::simple_initiator_socket<Initiator> socket;
    tlm_utils::peq_with_cb_and_phase<Initiator> m_peq;

    unsigned int initiator_id;

    // variables
    uint8_t next_tag = 0;

    // tag pool
    TagPool tag_pool;

    // metrics
    uint32_t read_byte_count = 0;
    sc_core::sc_time start_time;
    sc_core::sc_time end_time;
    double max_latency = 0.0;

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
        uint32_t i = 0;
        while (true) {
            uint32_t addr = i * 20;
            i = (i + 1) % 200;
            tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
            sc_time delay = SC_ZERO_TIME;

            // allocate data buffer
            uint32_t data_byte_length = 32;
            std::vector<uint8_t> *data_buffer = new std::vector<uint8_t>(data_byte_length * sizeof(uint8_t), 0xAB);

            if (i % 2 == 0) {
                trans->set_command(tlm::TLM_READ_COMMAND);
            } else {
                trans->set_command(tlm::TLM_WRITE_COMMAND);
            }

            trans->set_address(addr);
            trans->set_data_length(data_byte_length);
            trans->set_data_ptr(reinterpret_cast<unsigned char*>(data_buffer->data()));
            trans->set_byte_enable_ptr(0);
            trans->set_dmi_allowed(false);
            trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            {
                while (tag_pool.empty()) {
                    wait(1, sc_core::SC_NS); // wait until a tag is available
                }
                uint8_t tag = tag_pool.allocate_tag();
                auto* tlp = new PCIeTLPExtension();
                tlp->tlp_type = PCIeTLPType::MRd;
                tlp->tag = tag;
                tlp->lengthDW = 8; // 32 bytes
                tlp->requester = { /*bus*/ 0x00, /*dev*/ static_cast<uint8_t>(initiator_id), /*func*/ 0x00 };
                tlp->timestamp = sc_core::sc_time_stamp();
                trans->set_extension(tlp);  
            }

            tlm::tlm_phase phase = tlm::BEGIN_REQ;

            std::cout << "[Initiator-" << initiator_id << "] Sending tag:" << static_cast<int32_t>(trans->get_extension<PCIeTLPExtension>()->tag) << " BEGIN_REQ to address: " << std::hex << trans->get_address() << std::dec << " at " << sc_core::sc_time_stamp() << std::endl;
            socket->nb_transport_fw(*trans, phase, delay);
            wait(1, sc_core::SC_NS);
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
            {
                if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                    std::cout << "[Initiator-" << initiator_id << "] Data received: \n";
                    int8_t* data_ptr_int = reinterpret_cast<int8_t*>(data_ptr);
                    for (unsigned int i = 0; i < data_length; ++i) {
                        std::cout << std::hex << static_cast<int>(data_ptr_int[i]) << " " << std::dec;
                    }
                    std::cout << std::endl;
                }
            }
            uint8_t tag = trans.get_extension<PCIeTLPExtension>()->tag;
            tag_pool.release_tag(tag);

            read_byte_count += data_length;
            end_time = sc_core::sc_time_stamp();
            double elapsed_time = (end_time - start_time).to_seconds();
            double throughput_MBps = (read_byte_count / elapsed_time) / (1024*1024);
            std::cout << "[Throughput] " << throughput_MBps << " MB/s for initiator-" << initiator_id << ", elapse_time=" << elapsed_time << std::endl;;
            
            double latency_ns = (end_time - trans.get_extension<PCIeTLPExtension>()->timestamp).to_seconds() * 1000000000;
            std::cout << "[Latency] " << latency_ns << " ns for tag:" << tag << std::endl;
            max_latency = std::max(max_latency, latency_ns);
            std::cout << "[Max Latency] " << max_latency << " ns for initiator-" << initiator_id << std::endl;

            delete(data_ptr);
            delete(&trans);
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