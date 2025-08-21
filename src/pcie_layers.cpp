#include "pcie_layers.hpp"
#include <cassert>

//  ========================================
//  PCIeTransactionLayer Function Definition
//  ========================================

bool PCIeTransactionLayer::send_TLP(std::vector<PCIeTLPPayload>* payloads)
{
    SC_LOG(VERB, "send TLP");

    // acquire credit
    uint32_t header_credit = 1;
    uint32_t payload_credit = payloads->size();
    if (acquire_credits(header_credit, payload_credit) != true) {
        return false;
    }
    SC_LOG(VERB, "acquire_credits done");

    // acquire tag
    if (tag_pool_is_empty() != false) {
        return false;
    }
    SC_LOG(VERB, "tag pool check done");

    // allocate credit
    if (allocate_credits(header_credit, payload_credit) != true) {
        assert(0);
    }
    SC_LOG(VERB, "allocte credit done");

    // allocate tag
    uint8_t tag;
    if (allocate_tag(tag) != true) {
        assert(0);
    }
    SC_LOG(VERB, "allocte tag done");

    PCIeTLPHeader header;
    header.Length = payloads->size();
    header.reqID = requesterID;
    header.tag = tag;
    header.Type = static_cast<uint32_t>(PCIeTLPType::MRd);
    SC_LOG(VERB, "complete TLP");

    if (m_dataLinkLayer->insert_TLP(header, payloads) != 0) {
        assert(0);
        return false;
    }

    SC_LOG(TRACE, "send TLP, tag=%d", tag);

    return true;
}

bool PCIeTransactionLayer::acquire_credits(uint32_t header, uint32_t payload)
{
    if (!(header <= credits.header && payload <= credits.payload)) {
        return false;
    }
    return true;
}

bool PCIeTransactionLayer::allocate_credits(uint32_t header, uint32_t payload)
{
    if (!(header <= credits.header && payload <= credits.payload)) {
        return false;
    } 

    credits.header -= header;
    credits.payload -= payload;
    return true;
}

void PCIeTransactionLayer::release_credits(uint32_t header, uint32_t payload)
{
    credits.header += header;
    credits.payload += payload;
}

void PCIeTransactionLayer::set_credits(uint32_t header, uint32_t payload)
{
    credits.header = header;
    credits.payload = payload;
}

void PCIeTransactionLayer::init_tag_pool(uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        tagPool.push(i);
    }
}

bool PCIeTransactionLayer::tag_pool_is_empty(void)
{
    return (tagPool.size() == 0) ? true : false;
}

bool PCIeTransactionLayer::allocate_tag(uint8_t& tag)
{
    if (tag_pool_is_empty() == true) {
        return false;
    }

    tag = tagPool.front();
    tagPool.pop();
    return true;
}

void PCIeTransactionLayer::release_tag(uint8_t tag)
{
    tagPool.push(tag);
}

//  =====================================
//  PCIeDataLinkLayer Function Definition
//  =====================================

int PCIeDataLinkLayer::insert_TLP(PCIeTLPHeader header, std::vector<PCIeTLPPayload>* payloads)
{
    TLPToDLLP_queue.push(std::make_pair(header, payloads));
    event_TLPToDLLP.notify();
    return 0;
}

void PCIeDataLinkLayer::init_seqNumPool(uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        seqNumPool.push(i);
    }
    // std::cout << "init seqNumPool: #" << seqNumPool.size() << " done" << std::endl;
    SC_LOG(INFO, "init seqNumPool: #%d", seqNumPool.size());
}

void PCIeDataLinkLayer::init_replayBuffer(uint32_t count)
{
    replayBuffer_header.resize(count);
    replayBuffer_payload.resize(count);
    // std::cout << "init replay Buffer: #" << seqNumPool.size() << " done" << std::endl;
    SC_LOG(INFO, "init replay Buffer: #%d", seqNumPool.size());
}

void PCIeDataLinkLayer::peq_callback(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase)
{
    (void)phase;
    if (phase == tlm::BEGIN_REQ) {
        auto tlp_ext = trans.get_extension<PCIeTLPExtension>();
        SC_LOG(VERB, "Get TLP: SeqNum=%d", tlp_ext->tlp.dll_header.seqNum);
        
        // create TLM transaction
        tlm::tlm_generic_payload* dllp_trans = new tlm::tlm_generic_payload();
        tlm::tlm_phase dllp_phase = tlm::BEGIN_RESP;
        sc_time dllp_delay = sc_core::sc_time(10, SC_NS);

        // create TLP extension for TLM
        auto* dllp_ext = new PCIeDLLPExtension();
        dllp_ext->seqNum = tlp_ext->tlp.dll_header.seqNum;
        dllp_ext->dllp_type = PCIeDLLPType::Ack;
        dllp_trans->set_extension(dllp_ext);

        s_out->nb_transport_fw(*dllp_trans, dllp_phase, dllp_delay);
        SC_LOG(VERB, "Send DLLP back");
    }

    else if (phase == tlm::BEGIN_RESP) {
        auto dllp_ext = trans.get_extension<PCIeDLLPExtension>();
        uint32_t seqNum = dllp_ext->seqNum;
        SC_LOG(VERB, "Get DLLP: SeqNum=%d, %s", seqNum, ((dllp_ext->dllp_type == PCIeDLLPType::Ack) ? "Ack" : "Nack"));

        // release replay buffer according to seqNum
        auto old_TLP_pair = replayBuffer_header[seqNum];
        PCIeTLPHeader old_tlp_header = old_TLP_pair.second;
        std::vector<PCIeTLPPayload>* old_payloads = replayBuffer_payload[seqNum];
        uint8_t old_tag = old_tlp_header.tag;
        seqNumPool.push(seqNum);

        uint32_t release_credit_header = 1;
        uint32_t release_credit_payload = old_payloads->size();
        m_transactionLayer->release_credits(release_credit_header, release_credit_payload);
        m_transactionLayer->release_tag(old_tag);
        delete(old_payloads);
        SC_LOG(VERB, "Release credit=(%d, %d), tag=%d, seqNum=%d", release_credit_header, release_credit_payload, old_tag, seqNum);
        SC_LOG(TRACE, "finish TLP, tag=%d", old_tag);
    }

    else {
        assert(0);
    }

}

void PCIeDataLinkLayer::process_DLLP_flow_control()
{
    
}

void PCIeDataLinkLayer::process_TLP_to_DLLP()
{
    while (true) {
        wait(event_TLPToDLLP);

        while (!TLPToDLLP_queue.empty()) {
            if (seqNumPool.size() != 0) {
                auto TLP_pair = TLPToDLLP_queue.front();
                SC_LOG(VERB, "get TLP packet");
                SC_LOG(VERB, "DLLP send");

                // acquire seqNum and write to replay buffer
                uint32_t seqNum = seqNumPool.front();
                seqNumPool.pop();
                PCIeTLPDLLHeader dll_header;
                dll_header.seqNum = seqNum;
                replayBuffer_header[seqNum] = std::make_pair(dll_header, TLP_pair.first);
                replayBuffer_payload[seqNum] = TLP_pair.second;
                SC_LOG(VERB, "write header and payload into replay buffer");

                // create TLM transaction
                tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
                tlm::tlm_phase phase = tlm::BEGIN_REQ;
                sc_time delay = sc_core::sc_time(30, SC_NS);

                SC_LOG(VERB, "TLM component done");

                // create TLP extension for TLM
                auto* tlp_ext = new PCIeTLPExtension();
                tlp_ext->tlp.dll_header = dll_header;
                tlp_ext->tlp.tlp_header = TLP_pair.first;
                tlp_ext->tlp.payloads = TLP_pair.second;
                tlp_ext->tlp.lcrc = 0x12345678;
                trans->set_extension(tlp_ext);
                SC_LOG(VERB, "TLP extension done");

                s_out->nb_transport_fw(*trans, phase, delay);
                SC_LOG(VERB, "DLLP send done");

                TLPToDLLP_queue.pop();
            }
            
            // wait();
        }
    }
}

tlm::tlm_sync_enum PCIeDataLinkLayer::nb_transport_fw(tlm::tlm_generic_payload& trans,
                                                     tlm::tlm_phase& phase,
                                                     sc_core::sc_time& delay)
{
    SC_LOG(VERB, "fw get transaction");
    m_peq.notify(trans, phase, delay);
    return tlm::TLM_ACCEPTED;
}

tlm::tlm_sync_enum PCIeDataLinkLayer::nb_transport_bw(tlm::tlm_generic_payload& trans,
                                                     tlm::tlm_phase& phase,
                                                     sc_core::sc_time& delay)
{
    (void)trans;
    (void)phase;
    (void)delay;
    return tlm::TLM_ACCEPTED;
}

void PCIeDataLinkLayer::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) 
{
    // TODO: implement
    (void)trans;
    (void)delay;
}

bool PCIeDataLinkLayer::get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) 
{
    (void)trans;
    (void)dmi_data;
    return false;
}

unsigned int PCIeDataLinkLayer::transport_dbg(tlm::tlm_generic_payload& trans) 
{
    (void)trans;
    return 0;
}

void PCIeDataLinkLayer::invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) 
{
    // TODO: implement
    (void)start_range;
    (void)end_range;
}