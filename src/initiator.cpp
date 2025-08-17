#include "initiator.hpp"

TagPool::TagPool(uint32_t tag_count = 256) 
{
    for (uint32_t i = 0; i < tag_count; i++) {
        free_tags.push(static_cast<uint8_t>(i));
    }
};

bool TagPool::empty() const
{
    return free_tags.empty();
};

uint8_t TagPool::allocate_tag() 
{
    uint8_t tag = free_tags.front();
    free_tags.pop();
    return tag;
};

void TagPool::release_tag(uint8_t tag) 
{
    free_tags.push(tag);
};

void PCIeRequester::gen_command_process() 
{
    // preprocess
    start_time = sc_core::sc_time_stamp();
    uint32_t i = 0;

    while (true) {
        // command preprocess
        tlm::tlm_generic_payload* trans   = new tlm::tlm_generic_payload();
        tlm::tlm_command cmd_type         = (i % 2 == 0) ? tlm::TLM_READ_COMMAND : tlm::TLM_WRITE_COMMAND;
        uint32_t data_dw_size             = 32;
        std::vector<uint8_t> *data_buffer = new std::vector<uint8_t>(data_dw_size * sizeof(uint8_t), 0xAB);
        uint32_t addr                     = i * 20;
        sc_time delay                     = SC_ZERO_TIME;

        // build TLM transaction
        trans->set_address(addr);
        trans->set_data_length(data_dw_size);
        trans->set_data_ptr(reinterpret_cast<unsigned char*>(data_buffer->data()));
        trans->set_byte_enable_ptr(0);
        trans->set_dmi_allowed(false);
        trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        // TLP packet
        while (tag_pool.empty()) { wait(1, sc_core::SC_NS); }
        uint8_t tag = tag_pool.allocate_tag();
        auto* tlp = new PCIeTLPExtension();
        tlp->tlp_type = (cmd_type == tlm::TLM_READ_COMMAND) ? PCIeTLPType::MRd : PCIeTLPType::MWr;
        tlp->tag = tag;
        tlp->lengthDW = data_dw_size;
        tlp->requester = { /*bus*/ 0x00, /*dev*/ static_cast<uint8_t>(requester_id), /*func*/ 0x00 };
        tlp->timestamp = sc_core::sc_time_stamp();
        trans->set_extension(tlp);
        
        // TLM phase
        tlm::tlm_phase phase = tlm::BEGIN_REQ;

        std::cout << "[Initiator-" << requester_id << "] Sending tag:" << static_cast<int32_t>(trans->get_extension<PCIeTLPExtension>()->tag) << " BEGIN_REQ to address: " << std::hex << trans->get_address() << std::dec << " at " << sc_core::sc_time_stamp() << std::endl;
        socket->nb_transport_fw(*trans, phase, delay);
        wait(1, sc_core::SC_NS);

        i = (i + 1) % 200;
    }
};

void PCIeRequester::peq_callback(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase)
{
    if (phase == tlm::BEGIN_RESP) 
    {
        std::cout << "[Initiator-" << requester_id << "] Received BEGIN_RESP at " << sc_core::sc_time_stamp() << std::endl;

        std::cout << "[Initiator-" << requester_id << "] Sending END_REQ at " << sc_core::sc_time_stamp() << std::endl;
        sc_core::sc_time resp_delay = sc_core::sc_time(5, sc_core::SC_NS);
        tlm::tlm_phase end_phase = tlm::END_REQ;
        socket->nb_transport_fw(trans, end_phase, resp_delay);
    }
    
    else if (phase == tlm::END_RESP) 
    {
        std::cout << "[Initiator-" << requester_id << "] Received END_RESP at " << sc_core::sc_time_stamp() << std::endl;

        unsigned char* data_ptr = trans.get_data_ptr();
        unsigned int data_length = trans.get_data_length();
        {
            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                std::cout << "[Initiator-" << requester_id << "] Data received: \n";
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
        std::cout << "[Throughput] " << throughput_MBps << " MB/s for initiator-" << requester_id << ", elapse_time=" << elapsed_time << std::endl;;
        
        double latency_ns = (end_time - trans.get_extension<PCIeTLPExtension>()->timestamp).to_seconds() * 1000000000;
        std::cout << "[Latency] " << latency_ns << " ns for tag:" << tag << std::endl;
        max_latency = std::max(max_latency, latency_ns);
        std::cout << "[Max Latency] " << max_latency << " ns for initiator-" << requester_id << std::endl;

        delete(data_ptr);
        delete(&trans);
    }

    else 
    {
        SC_REPORT_ERROR("Initiator", "peq_callback received unexpected phase");
    }
};

tlm::tlm_sync_enum PCIeRequester::nb_transport_bw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay)
{
    m_peq.notify(trans, phase, delay);
    return tlm::TLM_COMPLETED;
}

void PCIeRequester::invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range)
{
    // not implemented
    (void)start_range;
    (void)end_range;
}