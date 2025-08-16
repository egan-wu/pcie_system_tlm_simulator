#pragma once
#include <tlm>
#include <systemc>

enum class PCIeTLPType {
    MRd     = 0,   // 0_0000, Memory Read Request
    MRdLk   = 1,   // 0_0001, Memory Read Lock Request
    MWr     = 2,   // 0_0010, Memory Write Request
    IORd    = 3,   // 0_0011, I/O Read Request
    IOWr    = 4,   // 0_0100, I/O Write Request
    CfgRd0  = 5,   // 0_0101, Configuration Read Request (Type 0)
    CfgWr0  = 6,   // 0_0110, Configuration Write Request (Type 0)
    CfgRd1  = 7,   // 0_0111, Configuration Read Request (Type 1)
    CfgWr1  = 8,   // 0_1000, Configuration Write Request (Type 1)
    Msg     = 9,   // 0_1001, Message Request
    MsgD    = 10,  // 0_1010, Message Data W/Data
    Cpl     = 11,  // 0_1011, Completion Request
    CplD    = 12,  // 0_1100, Completion W/Data
    // ...etc
};

struct PCIeRequesterID {
    uint16_t bus  = 0;
    uint8_t  dev  = 0;
    uint8_t  func = 0;
};

struct PCIeCompleterID {
    uint16_t bus  = 0;
    uint8_t  dev  = 0;
    uint8_t  func = 0;
};

struct PCIeTLPExtension : tlm::tlm_extension<PCIeTLPExtension> {
    PCIeTLPType tlp_type = PCIeTLPType::MRd;
    uint8_t   tag        = 0;
    uint8_t   lengthDW   = 0;
    PCIeRequesterID requester;
    PCIeCompleterID completer;
    bool      poisoned   = false;
    bool      relaxed    = false;
    bool      no_snoop   = false;
    sc_core::sc_time timestamp;

    virtual tlm::tlm_extension_base* clone() const override {
        return new PCIeTLPExtension(*this);
    }

    virtual void copy_from(tlm_extension_base const &ext) override {
        auto const& other = static_cast<PCIeTLPExtension const&>(ext);
        *this = other;
    }

    int get_device_id() const {
        return (requester.bus << 8) | requester.dev;
    }
};
