#include "pcie_bus.hpp"

void PCIeBus::peq_callback(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase) 
{
    if (phase == tlm::BEGIN_REQ) {
        if (arb_queue.empty()) {
            InitiatorIDExtension* ext = nullptr;
            trans.get_extension(ext);
            if (ext) {
                arb_queue.push(ext->initiator_id);
            } else {
                SC_REPORT_WARNING("PCIeBus", "Transaction without InitiatorIDExtension, using default initiator_id 0");
                arb_queue.push(0);
            }
        }
        unsigned int initiator_id = arb_queue.front();
        arb_queue.pop();

        tlm::tlm_phase fw_phase = tlm::BEGIN_REQ;
        trans_queue.push(std::make_pair(&trans, fw_phase));
        queue_event.notify(SC_ZERO_TIME);
        std::cout << "[PCIeBus] Received and queued BEGIN_REQ from initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
    }

    else if (phase == tlm::BEGIN_RESP) {
        auto it = trans_map.find(&trans);
        if (it == trans_map.end()) {
            SC_REPORT_ERROR("PCIeBus", "Transaction not found in map [BEGIN_RESP]");
            return;
        }
        unsigned int initiator_id = it->second.first;
        if (initiator_id < t_sockets.size()) {
            std::cout << "[PCIeBus] Forwarding BEGIN_RESP to initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
            tlm::tlm_phase resp_phase = tlm::BEGIN_RESP;
            sc_core::sc_time zero_delay = sc_core::SC_ZERO_TIME;
            (*t_sockets[initiator_id])->nb_transport_bw(trans, resp_phase, zero_delay);
        }
    }

    else if (phase == tlm::END_REQ) {
        unsigned int initiator_id = arb_queue.front();
        arb_queue.pop();

        tlm::tlm_phase fw_phase = tlm::END_REQ;
        trans_queue.push(std::make_pair(&trans, fw_phase));
        queue_event.notify(SC_ZERO_TIME);
        std::cout << "[PCIeBus] Received and queued END_REQ from initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
    }

    else if (phase == tlm::END_RESP) {
        auto it = trans_map.find(&trans);
        if (it == trans_map.end()) {
            SC_REPORT_ERROR("PCIeBus", "Transaction not found in map [BEGIN_RESP]");
            return;
        }
        unsigned int initiator_id = it->second.first;
        if (initiator_id < t_sockets.size()) {
            std::cout << "[PCIeBus] Forwarding END_RESP to initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
            tlm::tlm_phase resp_phase = tlm::END_RESP;
            sc_core::sc_time zero_delay = sc_core::SC_ZERO_TIME;
            (*t_sockets[initiator_id])->nb_transport_bw(trans, resp_phase, zero_delay);
        }

        // release from map
        trans_map.erase(it);
        auto* tlp = trans.get_extension<PCIeTLPExtension>();
        if (tlp) {
            tag_map[initiator_id].erase(tlp->tag);
            if (tag_map[initiator_id].empty()) {
                tag_map.erase(initiator_id);
            }
        }
    }

    else {
        SC_REPORT_ERROR("PCIeBus", "peq_callback received unexpected phase");
    }
};

void PCIeBus::process_trans_queue()
{
    while (true) {
        wait(queue_event);
        while(!trans_queue.empty()) {
            auto& trans_pair = trans_queue.front();
            trans_queue.pop();
            tlm::tlm_generic_payload* trans = trans_pair.first;
            tlm::tlm_phase phase = trans_pair.second;
            
            if (phase == tlm::BEGIN_REQ) {
                uint64_t addr = trans->get_address();
                int target_id = find_target(addr);
                if (target_id < 0) {
                    SC_REPORT_ERROR("PCIeBus", "Address not mapped to any target");
                }

                int initiator_id = -1;
                auto it = trans_map.find(trans);
                if (it != trans_map.end()) {
                    initiator_id = it->second.first;
                }

                auto* tlp = trans->get_extension<PCIeTLPExtension>();
                if (initiator_id < 0) {
                    // a transaction from a new initiator
                    trans->get_extension(tlp);
                    if (tlp) {
                        initiator_id = tlp->get_device_id();
                    }
                }

                trans_map[trans] = std::make_pair(initiator_id, target_id);
                std::cout << "[PCIeBus] insert trans_map={" << initiator_id << ", " << target_id << "}" << std::endl;
                tag_map[initiator_id][tlp->tag] = target_id;
                std::cout << "[PCIeBus] insert tag_map={" << initiator_id << ", " << static_cast<int>(tlp->tag) << ", " << target_id << "}" << std::endl;

                std::cout << "[PCIeBus] Forwarding BEGIN_REQ to target " << target_id << " from initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
                sc_core::sc_time bus_delay = config.bus_delay;
                tlm::tlm_phase fw_phase = tlm::BEGIN_REQ;
                (*i_sockets[target_id])->nb_transport_fw(*trans, fw_phase, bus_delay);
            }

            else if (phase == tlm::END_REQ) {
                uint64_t addr = trans->get_address();
                int tidx = find_target(addr);

                int initiator_id = -1;
                auto it = trans_map.find(trans);
                if (it != trans_map.end()) {
                    initiator_id = it->second.first;
                } 
                else {
                    SC_REPORT_WARNING("PCIeBus", "no mapping found for transaction w/o target id, assume initiator_id = 0");
                    initiator_id = 0;
                }

                std::cout << "[PCIeBus] Forwarding END_REQ to target " << tidx << " from initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
                sc_core::sc_time bus_delay = config.bus_delay;
                tlm::tlm_phase fw_phase = tlm::END_REQ;
                (*i_sockets[tidx])->nb_transport_fw(*trans, fw_phase, bus_delay);
            }

            else {
                std::cout << "[PCIeBus] Received unexpected phase " << phase << " for transaction at " << sc_core::sc_time_stamp() << std::endl;
                SC_REPORT_ERROR("PCIeBus", "Unsupported phase in process");
                continue;
            }
        }
    }
};

tlm::tlm_sync_enum PCIeBus::nb_transport_fw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay)
{
    PCIeTLPExtension* tlp = nullptr;
    trans.get_extension(tlp);
    if (!tlp) {
        SC_REPORT_ERROR("PCIeBus", "transaction without PCIeTLPExtension");
    }
    int initiator_id = tlp->get_device_id();

    if (phase == tlm::BEGIN_REQ) {
        std::cout << "[PCIeBus] Received BEGIN_REQ from initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
    }
    else if (phase == tlm::END_REQ) {
        std::cout << "[PCIeBus] Received END_REQ from initiator " << initiator_id << " at " << sc_core::sc_time_stamp() << std::endl;
        auto it = trans_map.find(&trans);
        if (it == trans_map.end()) {
            SC_REPORT_ERROR("PCIeBus", "Transaction not found in map [END_REQ]");
            return tlm::TLM_COMPLETED;
        }

        if (it->second.first != initiator_id) {
            SC_REPORT_ERROR("PCIeBus", "Initiator ID mismatch in END_REQ");
            return tlm::TLM_COMPLETED;
        }
    }

    arb_queue.push(initiator_id);
    m_peq.notify(trans, phase, delay);
    return tlm::TLM_ACCEPTED;
};

void PCIeBus::set_target_map(unsigned int target_idx, uint64_t base, uint64_t size)
{
    if (target_idx < addr_map.size()) {
        addr_map[target_idx].base = base;
        addr_map[target_idx].size = size;
    } else {
        SC_REPORT_ERROR("PCIeBus", "Set target index out of range");
    }
    std::cout << "---------------------------------------" << std::endl<< "[PCIeBus] Set target map for index " << target_idx << ": base = " << std::hex << base << ", size = " << std::dec << size << std::endl;
};

int PCIeBus::find_target(uint64_t address)
{
    for (unsigned int i = 0; i < addr_map.size(); ++i) {
        if (addr_map[i].size == 0) continue;
        if (address >= addr_map[i].base && 
            address < (addr_map[i].base + addr_map[i].size)) {
            std::cout << "[PCIeBus] Found Target Address " << std::hex << address << " mapped to target index " << i << " (base = " << addr_map[i].base << ", size = " << addr_map[i].size << ")" << std::dec << std::endl;
            return i;
        }
    }
    return -1;
};

tlm::tlm_sync_enum PCIeBus::nb_transport_bw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay)
{
    std::cout << "[PCIeBus] Received nb_transport_bw phase = " << phase << " at " << sc_core::sc_time_stamp() << std::endl;
    m_peq.notify(trans, phase, delay);
    return tlm::TLM_ACCEPTED;
};

void PCIeBus::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) 
{
    SC_REPORT_WARNING("PCIeBus", "b_transport not implemented");
    (void)trans;
    (void)delay;
};

bool PCIeBus::get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) 
{
    (void)trans;
    (void)dmi_data;
    return false;
};

unsigned int PCIeBus::transport_dbg(tlm::tlm_generic_payload& trans) 
{
    (void)trans;
    return 0;
};

void PCIeBus::invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) 
{
    // not implemented
    (void)start_range;
    (void)end_range;
};