#pragma once
#include <systemc>

using namespace sc_core;

struct PCIeConfig {
    sc_time bus_delay = sc_time(200, SC_NS);
    sc_time target_delay = sc_time(50, SC_NS);
};