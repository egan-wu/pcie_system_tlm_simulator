#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <queue>
#include "utils.hpp"
#include "initiator_id_extension.hpp"
#include "pcie_tlp_extension.hpp"

using namespace sc_core;

/**
 * TagPool manages a pool of tags for PCIe transactions.
 * It allows allocation and release of tags, ensuring that tags are reused efficiently.
 */
class TagPool {
public:
    TagPool(uint32_t tag_count);

    bool empty() const;
    uint8_t allocate_tag();
    void release_tag(uint8_t tag);

private:
    std::queue<uint8_t> free_tags;
};

struct PCIeRequester 
: sc_core::sc_module,
  public tlm::tlm_bw_transport_if<>
{
    // TLM component
    tlm_utils::simple_initiator_socket<PCIeRequester> socket;
    tlm_utils::peq_with_cb_and_phase<PCIeRequester> m_peq;

    // Requester Information
    unsigned int requester_id;

    // variables
    uint8_t next_tag = 0;

    // tag pool
    TagPool tag_pool;

    // metrics
    uint32_t read_byte_count = 0;
    sc_core::sc_time start_time;
    sc_core::sc_time end_time;
    double max_latency = 0.0;

    SC_CTOR(PCIeRequester, unsigned int id) 
    : socket("socket"),
      m_peq(this, &PCIeRequester::peq_callback),
      requester_id(id),
      tag_pool(256)
    {
        socket.register_nb_transport_bw(this, &PCIeRequester::nb_transport_bw);
        SC_THREAD(gen_command_process);
    }

    // PCIeRequester Function
    void gen_command_process();
    void peq_callback (tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);

    // tlm::tlm_bw_transport_if<> inherit virtual function
    tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay);
    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range);
};