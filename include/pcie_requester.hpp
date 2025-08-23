#pragma once
#include <queue>
#include <unordered_map>
#include "log.hpp"
#include "pcie_layers.hpp"

using namespace sc_core;

class PCIeRequester_
: sc_core::sc_module
{
public:
    PCIeRequester_(sc_core::sc_module_name name, unsigned int id)
    : sc_core::sc_module(name),
      requesterID(id),
      rand(1, 64)
    {
        SC_THREAD(process_send_command);

        m_dataLinkLayer = new PCIeDataLinkLayer("dataLinkLayer", requesterID);
        m_transactionLayer = new PCIeTransactionLayer("transactionLayer", requesterID, m_dataLinkLayer);
        m_dataLinkLayer->m_transactionLayer = m_transactionLayer;

        // profiling
        profile_write_byte_size = 0;
        profile_read_byte_size = 0;

        SC_LOG(INFO, "init done");
    }

    // General Component
    unsigned int requesterID;

    //  ====================================
    //  public function can be used by other
    //  ====================================
    void process_send_command();

    PCIeTransactionLayer *m_transactionLayer;
    PCIeDataLinkLayer *m_dataLinkLayer;

private:

    // -- component
    Randomizer rand;

    // -- function

    // -- profile
    double profile_write_byte_size, profile_write_throughput;
    double profile_read_byte_size, profile_read_throughput;

};