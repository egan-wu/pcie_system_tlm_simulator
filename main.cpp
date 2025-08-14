#include <systemc>
#include "config.hpp"
#include "initiator.hpp"
#include "target.hpp"
#include "pcie_bus.hpp"

int sc_main(int /*argc*/, char* /*argv*/[]) {
    PCIeConfig config;
    config.bus_delay = sc_core::sc_time(10, sc_core::SC_NS);
    config.target_delay = sc_core::sc_time(5, sc_core::SC_NS);

    unsigned int initiatorCount = 3;
    unsigned int targetCount = 2;

    PCIeBus bus("pcie_bus", config, initiatorCount, targetCount);
    bus.set_target_map(0, 0x0000, 0x1000);
    bus.set_target_map(1, 0x1000, 0x1000);

    Initiator initiator0("initiator0", 0);
    Initiator initiator1("initiator1", 1);
    Initiator initiator2("initiator2", 2);
    Target target0("target0", config);
    Target target1("target1", config);

    initiator0.socket.bind((*bus.t_sockets[0]));
    initiator1.socket.bind((*bus.t_sockets[1]));
    initiator2.socket.bind((*bus.t_sockets[2]));
    bus.i_sockets[0]->bind(target0.socket);
    bus.i_sockets[1]->bind(target1.socket);

    std::cout << "Starting simulation..." << std::endl;
    sc_core::sc_start();
    std::cout << "Simulation finished at " << sc_core::sc_time_stamp() << std::endl;

    return 0;
}