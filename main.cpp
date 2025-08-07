#include <systemc>
#include "config.hpp"
#include "initiator.hpp"
#include "target.hpp"
#include "pcie_bus.hpp"

int sc_main(int /*argc*/, char* /*argv*/[]) {
    PCIeConfig config;

    Initiator initiator("initiator");
    Target target("target", config);
    PCIeBus bus("pcie_bus", config);

    initiator.socket.bind(bus.t_socket);
    bus.i_socket.bind(target.socket);

    std::cout << "Starting simulation..." << std::endl;
    sc_core::sc_start();
    std::cout << "Simulation finished at " << sc_core::sc_time_stamp() << std::endl;

    return 0;
}