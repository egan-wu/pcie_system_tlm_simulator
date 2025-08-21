#include "pcie_requester.hpp"

//  =================================
//  PCIeRequester Function Definition
//  =================================

void PCIeRequester_::process_send_command()
{
    int i = 0;
    while (true) {
        // wait();

        std::vector<PCIeTLPPayload> *payloads = new std::vector<PCIeTLPPayload>();
        uint32_t length = 4;
        for (uint32_t dw = 0; dw < length; dw++) {
            PCIeTLPPayload payload;
            payload.payload = dw;
            payloads->emplace_back(payload);
        }
        SC_LOG(VERB, "send_command");

        while (m_transactionLayer->send_TLP(payloads) != true) {
            wait(5, sc_core::SC_NS);
        }

        wait(2, sc_core::SC_NS);
        
        i++;
        if (i >= 100) {
            break;
        }
    }
}