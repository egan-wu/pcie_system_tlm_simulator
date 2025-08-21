#pragma once
#include "log.hpp"
#include "pcie_layers.hpp"

using namespace sc_core;

class PCIeCompleter_
: sc_core::sc_module
{
public:
    PCIeCompleter_(sc_core::sc_module_name name, unsigned int id)
    : sc_core::sc_module(name),
      completerID(id)
    {
        m_dataLinkLayer = new PCIeDataLinkLayer("dataLinkLayer", completerID);
        m_transactionLayer = new PCIeTransactionLayer("transactionLayer", completerID, m_dataLinkLayer);

        // SC_THREAD(process_send_command);
        SC_LOG(INFO, "init done");
    }

    // General Component
    unsigned int completerID;

    //  ====================================
    //  public function can be used by other
    //  ====================================
    // void process_send_command();

    PCIeTransactionLayer *m_transactionLayer;
    PCIeDataLinkLayer *m_dataLinkLayer;

private:



    // -- function

};