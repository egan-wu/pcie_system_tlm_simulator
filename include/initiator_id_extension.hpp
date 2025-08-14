#pragma once
#include <tlm>

struct InitiatorIDExtension : tlm::tlm_extension<InitiatorIDExtension> {
    unsigned int initiator_id;

    tlm_extension_base* clone() const override {
        auto* ext = new InitiatorIDExtension();
        ext->initiator_id = initiator_id;
        return ext;
    }

    void copy_from(tlm_extension_base const& ext) override {
        initiator_id = static_cast<InitiatorIDExtension const&>(ext).initiator_id;
    }

};