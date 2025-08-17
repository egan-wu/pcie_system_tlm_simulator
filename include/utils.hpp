#pragma once
#include <systemc>
#include <string>

#define PCIE_LOG_EN (1)

const std::string reset_font = "\033[0m";
const std::string red_font = "\033[31m";

#if PCIE_LOG_EN
#define PCIE_LOG \
    std::cout << red_font << "[PCIe] " << reset_font << __FILE__ << ":" << __LINE__ << " " << __FUNCTION__ << " at " \
              << sc_core::sc_time_stamp() << std::endl;
#endif