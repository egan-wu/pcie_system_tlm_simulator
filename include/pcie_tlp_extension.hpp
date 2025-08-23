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

enum class PCIeDLLPType {
    AckNack  = 2,
    UpdateFC = 5,
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

struct PCIeTLPHeader {
    uint32_t Length     :10;
    uint32_t AT         :2;
    uint32_t Attr0      :2;
    uint32_t EP         :1;
    uint32_t TD         :1;
    uint32_t TH         :1;
    uint32_t Rsv0       :1;
    uint32_t Attr1      :1;
    uint32_t Rsv1       :1;
    uint32_t TC         :3;
    uint32_t Rsv2       :1;
    uint32_t Type       :5;
    uint32_t Fmt        :3;
    uint32_t DWBE_1st   :4;
    uint32_t DWBE_last  :4;
    uint32_t tag        :8;
    uint32_t reqID      :16;
    uint32_t Addr_h     :32;
    uint32_t Rsv3       :2;
    uint32_t Addr_l     :30;
};

struct PCIeTLPPayload {
    uint32_t payload;
};

struct PCIeTLPDLLHeader {
    uint32_t seqNum;
};

struct PCIeTLP {
    PCIeTLPDLLHeader dll_header; // DLL header
    PCIeTLPHeader tlp_header; // TLP header
    std::vector<PCIeTLPPayload>* payloads; // TLP payload
    uint32_t lcrc; // LinkCRC
};

struct PCIeDLLPHeader {
    uint32_t Type;
    uint32_t VC;
};

struct PCIeDLLPPayload {
    uint32_t payload;
};

struct PCIeDLLP {
    PCIeDLLPHeader header;
    std::vector<PCIeDLLPPayload>* payloads;
    uint32_t crc32;
};

struct DLLPFrame {
    uint8_t type;
    union {
        PCIeTLP tlp;
        PCIeDLLP dllp;
    };
};

struct PCIeTLPExtension : tlm::tlm_extension<PCIeTLPExtension> {
    PCIeTLP tlp;
    PCIeTLPType tlp_type = PCIeTLPType::MRd;
    uint8_t   tag        = 0;
    uint8_t   lengthDW   = 0;
    PCIeTLPHeader header;
    std::vector<PCIeTLPPayload> *payloads;
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

struct PCIeDLLPExtension : tlm::tlm_extension<PCIeDLLPExtension> {
    PCIeDLLP dllp;
    uint32_t seqNum;
    uint32_t fc;
    PCIeDLLPType dllp_type = PCIeDLLPType::AckNack;
    PCIeRequesterID requester;
    PCIeCompleterID completer;
    sc_core::sc_time timestamp;

    virtual tlm::tlm_extension_base* clone() const override {
        return new PCIeDLLPExtension(*this);
    }

    virtual void copy_from(tlm_extension_base const &ext) override {
        auto const& other = static_cast<PCIeDLLPExtension const&>(ext);
        *this = other;
    }

    int get_device_id() const {
        return (requester.bus << 8) | requester.dev;
    }
};

struct PCIeTLPCredit {
    uint32_t header;
    uint32_t payload;
};