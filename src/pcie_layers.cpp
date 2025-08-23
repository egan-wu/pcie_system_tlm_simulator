#include "pcie_layers.hpp"
#include <cassert>

//  ========================================
//  PCIeTransactionLayer Function Definition
//  ========================================

bool PCIeTransactionLayer::send_TLP(PCIeTLPType type, std::vector<PCIeTLPPayload>* payloads)
{
    int32_t vacc = (internalBufferHead - internalBufferTail + internalBufferSize - 1) % internalBufferSize;
    if ((int)payloads->size() > vacc) {
        return false;
    }
    SC_LOG(VERB, "internalBufferHead=%d, internalBufferTail=%d, internalBufferSize=%d, payload_size=%d, vaccancy=%d", internalBufferHead, internalBufferTail, internalBufferSize, payloads->size(), vacc - payloads->size());
    SC_LOG(VERB, "allocate TLP internal buffer");

    TL_transaction tlp_trans;
    tlp_trans.type = type;
    tlp_trans.length = payloads->size();
    tlp_trans.internal_buffer_base = internalBufferTail;
    for (size_t i = 0; i < tlp_trans.length; i++) {
        internalBuffer[internalBufferTail++] = payloads->at(i).payload;
        internalBufferTail %= internalBufferSize;
    }

    internalTrans_queue.push(tlp_trans);
    event_internalTrans.notify();
    SC_LOG(VERB, "send_TLP done");
    return true;
}

void PCIeTransactionLayer::process_build_TLP()
{
    while (true) {
        wait(event_internalTrans);
        while (!internalTrans_queue.empty()) {
            SC_LOG(VERB, "processing next TLP internalTrans");

            TL_transaction tlp_trans = internalTrans_queue.front();
            internalTrans_queue.pop();

            // acquire credit
            uint32_t header_credit = 1;
            uint32_t payload_credit = tlp_trans.length;
            SC_LOG(VERB, "attempt to acquire_credits...");
            while (acquire_credits(header_credit, payload_credit) != true) {
                wait(1, SC_NS);
            }
            SC_LOG(VERB, "acquire_credits done");

            // acquire tag
            SC_LOG(VERB, "attempt to acquire tag...");
            while (tag_pool_is_empty() != false) {
                wait(1, SC_NS);
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

            // setup TLP header
            PCIeTLPHeader header;
            header.Length = tlp_trans.length;
            header.reqID = requesterID;
            header.tag = tag;
            header.Type = static_cast<uint32_t>(tlp_trans.type);
            SC_LOG(VERB, "complete TLP header");

            if (m_dataLinkLayer->insert_TLP(header, tlp_trans.internal_buffer_base) != 0) {
                wait(1, SC_NS);
            }

            SC_LOG(TRACE, "send TLP, tag=%d", tag);
        }
    }
}

bool PCIeTransactionLayer::acquire_credits(uint32_t header, uint32_t payload)
{
    if (!(header <= credits.header && payload <= credits.payload)) {
        return false;
    }
    SC_LOG(VERB, "acquire_credits, header=%d/%d, payload=%d/%d", header, credits.header, payload, credits.payload);
    return true;
}

bool PCIeTransactionLayer::allocate_credits(uint32_t header, uint32_t payload)
{
    SC_LOG(VERB, "credits, header=%d/%d, payload=%d/%d", header, credits.header, payload, credits.payload);
    if (!(header <= credits.header && payload <= credits.payload)) {
        return false;
    } 

    credits.header -= header;
    credits.payload -= payload;
    SC_LOG(VERB, "allocate_credits, header=%d, payload=%d", credits.header, credits.payload);
    return true;
}

void PCIeTransactionLayer::release_credits(uint32_t header, uint32_t payload)
{
    credits.header += header;
    credits.payload += payload;
    internalBufferHead = (internalBufferHead + payload) % internalBufferSize;
    SC_LOG(VERB, "internalBuffer: head=%d, tail=%d", internalBufferHead, internalBufferTail);
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

uint32_t PCIeTransactionLayer::get_internalBuffer_dw(uint32_t index)
{
    return internalBuffer[(index % internalBufferSize)];
}

//  =====================================
//  PCIeDataLinkLayer Function Definition
//  =====================================

int PCIeDataLinkLayer::insert_TLP(PCIeTLPHeader header, uint32_t payload_index)
{
    int32_t header_credit = 1;
    int32_t payload_credit = header.Length;

    int32_t header_vacc = (replayBufferHeader_head - replayBufferHeader_tail + seqNumCount - 1) % seqNumCount;
    SC_LOG(VERB, "replayBufferHeader_head=%d, replayBufferHeader_tail=%d, seqNumCount=%d, header_credit=%d, vaccancy=%d", replayBufferHeader_head, replayBufferHeader_tail, seqNumCount, header_credit, header_vacc);
    if (header_credit > header_vacc) {
        return -1;
    }

    int32_t payload_vacc = (replayBufferPayload_head - replayBufferPayload_tail + seqNumCount - 1) % seqNumCount;
    SC_LOG(VERB, "replayBufferPayload_head=%d, replayBufferPayload_tail=%d, seqNumCount=%d, payload_credit=%d, vaccancy=%d", replayBufferPayload_head, replayBufferPayload_tail, seqNumCount, header_credit, payload_vacc);
    if (payload_credit > payload_vacc) {
        return -1;
    }

    SC_LOG(VERB, "insert TLP payload allocate done, header=%d, payload=%d", header_credit, payload_credit);

    DLL_transaction dll_trans;
    dll_trans.replayBufferHeader_base = replayBufferHeader_tail;
    dll_trans.headerLength = header_credit;
    dll_trans.replayBufferPayload_base = replayBufferPayload_tail;
    dll_trans.payloadLength = payload_credit;
      
    for (size_t i = 0; i < dll_trans.headerLength; i++) {
        replayBuffer_header[replayBufferHeader_tail++] = header;
        replayBufferHeader_tail %= seqNumCount;
    }

    // PCIeTLPPayload tlp_payload;
    for (size_t i = 0; i < dll_trans.payloadLength; i++) {
        replayBuffer_payload[replayBufferPayload_tail++] = {m_transactionLayer->get_internalBuffer_dw(payload_index + i)};
        replayBufferPayload_tail %= seqNumCount;
    }

    DLLTrans_queue.push(dll_trans);
    event_DLLTrans_queue.notify();
    return 0;
}

void PCIeDataLinkLayer::init_seqNumPool(uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        seqNumPool.push(i);
    }
    SC_LOG(INFO, "init seqNumPool: #%d", count);
}

void PCIeDataLinkLayer::init_replayBuffer(uint32_t count)
{
    replayBuffer_header.resize(count);
    replayBuffer_payload.resize(count);
    SC_LOG(INFO, "init replay Buffer: #%d", count);
}

void PCIeDataLinkLayer::peq_callback(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase)
{
    (void)phase;
    if (phase == tlm::BEGIN_REQ) {
        auto tlp_ext = trans.get_extension<PCIeTLPExtension>();
        SC_LOG(VERB, "Get TLP: SeqNum=%d", tlp_ext->tlp.dll_header.seqNum);

        std::vector<PCIeTLPPayload> *payloads = tlp_ext->tlp.payloads;
        for (size_t i = 0; i < tlp_ext->tlp.tlp_header.Length; i++) {
            SC_LOG(VERB, "Get TLP: data[%d]: %d", i, payloads->at(i).payload);
        }
        
        // create TLM transaction
        tlm::tlm_generic_payload* dllp_trans = new tlm::tlm_generic_payload();
        tlm::tlm_phase dllp_phase = tlm::BEGIN_RESP;
        sc_time dllp_delay = sc_core::sc_time(10, SC_NS);

        // create TLP extension for TLM
        auto* dllp_ext = new PCIeDLLPExtension();
        dllp_ext->seqNum = tlp_ext->tlp.dll_header.seqNum;
        dllp_ext->dllp_type = PCIeDLLPType::AckNack;
        dllp_trans->set_extension(dllp_ext);

        s_out->nb_transport_fw(*dllp_trans, dllp_phase, dllp_delay);
        SC_LOG(VERB, "Send DLLP[AckNack] back");

        // create TLM transaction
        tlm::tlm_generic_payload* dllp_trans_fc = new tlm::tlm_generic_payload();
        tlm::tlm_phase dllp_phase_fc = tlm::BEGIN_RESP;
        sc_time dllp_delay_fc = sc_core::sc_time(10, SC_NS);

        // create TLP extension for TLM
        auto* dllp_ext_fc = new PCIeDLLPExtension();
        dllp_ext_fc->fc = tlp_ext->tlp.tlp_header.Length;
        dllp_ext_fc->dllp_type = PCIeDLLPType::UpdateFC;
        dllp_trans_fc->set_extension(dllp_ext_fc);

        s_out->nb_transport_fw(*dllp_trans_fc, dllp_phase_fc, dllp_delay_fc);
        SC_LOG(VERB, "Send DLLP[UpdateFC] back");
    }

    else if (phase == tlm::BEGIN_RESP) {
        auto dllp_ext = trans.get_extension<PCIeDLLPExtension>();

        if (dllp_ext->dllp_type== PCIeDLLPType::AckNack) {
            uint32_t seqNum = dllp_ext->seqNum;
            SC_LOG(VERB, "Get DLLP[AckNack]: SeqNum=%d, Ack", seqNum);

            // release replay buffer according to seqNum
            PCIeTLPHeader old_header = replayBuffer_header[seqNum];
            uint8_t old_tag = old_header.tag;
            seqNumPool.push(seqNum); // release seqNum

            // release replay buffer
            replayBufferHeader_head = (replayBufferHeader_head + 1) % seqNumCount; 
            replayBufferPayload_head = (replayBufferPayload_head + old_header.Length) % seqNumCount;
            m_transactionLayer->release_tag(old_tag);
            SC_LOG(VERB, "release replay buffer & tag(%d)", old_tag);
            SC_LOG(TRACE, "finish TLP, tag=%d", old_tag);
        }

        else if (dllp_ext->dllp_type == PCIeDLLPType::UpdateFC) {
            uint32_t fc = dllp_ext->fc;
            SC_LOG(VERB, "Get DLLP[UpdateFC]: fc=%d", fc);

            m_transactionLayer->release_credits(1, fc);
            SC_LOG(VERB, "release credit");
        }

        else {
            SC_LOG(ERROR, "unhandled DLLP type");
            assert(0);
        }


    }

    else {
        std::cout << phase << std::endl;
        SC_LOG(ERROR, "Get unknown phase");
        assert(0);
    }

}

void PCIeDataLinkLayer::process_DLLTrans_queue()
{
    while (true) {
        wait(event_DLLTrans_queue);

        while(!DLLTrans_queue.empty()) {
                DLL_transaction DLL_trans = DLLTrans_queue.front();

                // acquire seqNum and write to replay buffer
                uint32_t seqNum = seqNumPool.front();
                seqNumPool.pop();
                PCIeTLPDLLHeader dll_header;
                dll_header.seqNum = seqNum;

                // create TLM transaction
                tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
                tlm::tlm_phase phase = tlm::BEGIN_REQ;
                sc_time delay = SC_ZERO_TIME;
                SC_LOG(VERB, "TLM component done");

                std::vector<PCIeTLPPayload> *payloads = new std::vector<PCIeTLPPayload>(DLL_trans.payloadLength);
                for (size_t i = 0; i < DLL_trans.payloadLength; i++) {
                    payloads->at(i) = replayBuffer_payload[(DLL_trans.replayBufferPayload_base + i) % seqNumCount];
                }

                // create TLP extension for TLM
                auto* tlp_ext = new PCIeTLPExtension();
                tlp_ext->tlp.dll_header = dll_header;
                tlp_ext->tlp.tlp_header = replayBuffer_header[DLL_trans.replayBufferHeader_base];
                tlp_ext->tlp.payloads = payloads;
                tlp_ext->tlp.lcrc = 0x12345678;
                trans->set_extension(tlp_ext);
                SC_LOG(VERB, "TLP extension done");

                wait(DLL_trans.payloadLength * 2, SC_NS); // simulate transaction latency of requester's physical layer to completer's physical layter 
                s_out->nb_transport_fw(*trans, phase, delay);
                SC_LOG(VERB, "DLLP send done");

                DLLTrans_queue.pop();
                DLLTrans_map[seqNum] = DLL_trans;
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