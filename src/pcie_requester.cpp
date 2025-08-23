#include "pcie_requester.hpp"

//  =================================
//  PCIeRequester Function Definition
//  =================================

void PCIeRequester_::process_send_command()
{
    int i = 0;
    sc_core::sc_time start_time = sc_core::sc_time_stamp();

    while (true) {
        wait(5, sc_core::SC_NS);

        std::vector<PCIeTLPPayload> *payloads = new std::vector<PCIeTLPPayload>();
        uint32_t length = rand.nextInt();
        for (uint32_t dw = 0; dw < length; dw++) {
            PCIeTLPPayload payload;
            payload.payload = i;
            payloads->emplace_back(payload);
        }
        SC_LOG(DEBUG, "send_command, layload_size=%d", length);

        while (m_transactionLayer->send_TLP(PCIeTLPType::MWr, payloads) != true) {
            wait(5, sc_core::SC_NS);
        }

        // profiling
        profile_write_byte_size += (length * 4);
        
        if (((i + 1) % 1000) == 0) {
            sc_core::sc_time current_time = sc_core::sc_time_stamp();
            sc_core::sc_time elapse_time = current_time - start_time;
            // SC_LOG(INFO, "write dw size: %d Bytes", (int)profile_write_byte_size);
            // SC_LOG(INFO, "elapse time: %d s", elapse_time.to_seconds());
            SC_LOG(INFO, "write throughput: %.2f GB/s", ((profile_write_byte_size / (1024 * 1024)) / elapse_time.to_seconds()) );
        }

        delete(payloads);
        
        i++;
        // if (i >= 1000) {
        //     break;
        // }
    }
}