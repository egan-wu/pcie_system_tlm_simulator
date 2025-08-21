#include "pcie_completer.hpp"

//  =================================
//  PCIeCompleter Function Definition
//  =================================

// void PCIeCompleter_::process_send_command()
// {
//     while (true) {
//         wait();

//         std::vector<PCIeTLPPayload> *payloads = new std::vector<PCIeTLPPayload>();
//         uint32_t length = 4;
//         for (uint32_t dw = 0; dw < length; dw++) {
//             PCIeTLPPayload payload;
//             payload.payload = dw;
//             payloads->emplace_back(payload);
//         }

//         while (m_transactionLayer->send_TLP(payloads) != 0) {
//             wait(5, sc_core::SC_NS);
//         }

//         wait(50, sc_core::SC_NS);
//     }
// }