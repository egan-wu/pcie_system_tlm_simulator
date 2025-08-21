#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <queue>
#include <unordered_map>
#include "utils.hpp"
#include "log.hpp"
#include "pcie_tlp_extension.hpp"

using namespace sc_core;

class PCIeTransactionLayer;

class PCIeDataLinkLayer
: sc_core::sc_module,
  public tlm::tlm_fw_transport_if<>,
  public tlm::tlm_bw_transport_if<>
{
public:

    PCIeDataLinkLayer(sc_core::sc_module_name name, unsigned int id)
    : sc_core::sc_module(name),
      s_out("data_link_layer_tx"),
      s_in("data_link_layer_rx"),
      m_peq(this, &PCIeDataLinkLayer::peq_callback),
      requesterID(id)
    {
        SC_THREAD(process_DLLP_flow_control);
        SC_THREAD(process_TLP_to_DLLP);

        s_out.register_nb_transport_bw(this, &PCIeDataLinkLayer::nb_transport_bw);
        s_in.register_nb_transport_fw(this, &PCIeDataLinkLayer::nb_transport_fw);

        uint32_t seqNumCount = 100;
        init_seqNumPool(seqNumCount);
        init_replayBuffer(seqNumCount);
    }

    // TLM component
    tlm_utils::simple_initiator_socket<PCIeDataLinkLayer> s_out;
    tlm_utils::simple_target_socket<PCIeDataLinkLayer> s_in;
    tlm_utils::peq_with_cb_and_phase<PCIeDataLinkLayer> m_peq;
    sc_core::sc_event event_TLPToDLLP;

    // General Component
    unsigned int requesterID;

    // Transaction Layer Component
    PCIeTransactionLayer *m_transactionLayer;

    // Data Link Layer Component
    std::queue<std::pair<PCIeTLPHeader, std::vector<PCIeTLPPayload>*>> TLPToDLLP_queue;
    std::queue<uint32_t> seqNumPool;
    std::vector<std::pair<PCIeTLPDLLHeader, PCIeTLPHeader>> replayBuffer_header;
    std::vector<std::vector<PCIeTLPPayload>*> replayBuffer_payload;

    //  ====================================
    //  public function can be used by other
    //  ====================================

    // DLLP layer function
    int send_DLLP();
    int insert_TLP(PCIeTLPHeader header, std::vector<PCIeTLPPayload>* payloads);

private:

    //  ===============================================
    //  private function only used in data link layer
    //  ===============================================
    void process_DLLP_flow_control();
    void process_TLP_to_DLLP();

    // PEQ callback
    void peq_callback (tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);

    // seqNumber
    void init_seqNumPool(uint32_t count);
    void init_replayBuffer(uint32_t count);

    // credit control
    void set_header_credit(uint32_t credits);
    void set_payload_credit(uint32_t credits);

    // tlm_fw/bw_transport_if override function
    tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay);
    tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay);
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range);
};

class PCIeTransactionLayer
: sc_core::sc_module
{
public:

    PCIeTransactionLayer(sc_core::sc_module_name name, unsigned int id, PCIeDataLinkLayer *m_dataLinkLayer_)
    : sc_core::sc_module(name),
      requesterID(id),
      m_dataLinkLayer(m_dataLinkLayer_)
    {
        set_credits(10, 50);
        init_tag_pool(10);
    }

    // TLM component

    // General Component
    unsigned int requesterID;

    // Transaction Layer Component
    PCIeTLPCredit credits;
    std::queue<uint8_t> tagPool;

    //  ====================================
    //  public function can be used by other
    //  ====================================
    void release_credits(uint32_t header, uint32_t payload);
    void release_tag(uint8_t tag);

    // TLP layer function
    bool send_TLP(std::vector<PCIeTLPPayload>* payloads);

private:

    //  ===============================================
    //  private function only used in transaction layer
    //  ===============================================
    
    // DLLP layer component
    PCIeDataLinkLayer *m_dataLinkLayer;

    // tag pool function
    void init_tag_pool(uint32_t count);
    bool tag_pool_is_empty(void);
    bool allocate_tag(uint8_t& tag);

    // credits function
    bool acquire_credits(uint32_t header, uint32_t payload);
    bool allocate_credits(uint32_t header, uint32_t payload);
    void set_credits(uint32_t header, uint32_t payload);

};