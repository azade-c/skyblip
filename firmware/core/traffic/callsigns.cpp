#include "core/traffic/callsigns.h"

namespace skyblip::traffic {

void CallsignTable::learn(uint8_t addr_table, uint32_t addr, const char* callsign, uint32_t now) {
    if (callsign == nullptr || callsign[0] == 0) return;
    int idx = find_slot(addr_table, addr);
    if (idx < 0) idx = oldest_slot();
    if (idx < 0) return;

    Entry& e = slots_[static_cast<size_t>(idx)];
    e.used = true;
    e.addr = addr;
    e.addr_table = addr_table;
    e.heard_s = now;
    int n = 0;
    while (n < kTextBytes - 1 && callsign[n] != 0) {
        e.text[n] = callsign[n];
        n++;
    }
    e.text[n] = 0;
}

const char* CallsignTable::find(uint8_t addr_table, uint32_t addr) const {
    const int idx = find_slot(addr_table, addr);
    if (idx < 0) return nullptr;
    return slots_[static_cast<size_t>(idx)].text;
}

void CallsignTable::age_out(uint32_t now, uint32_t forget_s) {
    for (Entry& e : slots_) {
        if (!e.used) continue;
        if (now < e.heard_s || now - e.heard_s <= forget_s) continue;
        e = Entry{};
    }
}

int CallsignTable::count() const {
    int n = 0;
    for (const Entry& e : slots_)
        if (e.used) n++;
    return n;
}

void CallsignTable::clear() { slots_ = {}; }

int CallsignTable::find_slot(uint8_t addr_table, uint32_t addr) const {
    for (int i = 0; i < kCapacity; i++) {
        const Entry& e = slots_[static_cast<size_t>(i)];
        if (e.used && e.addr == addr && e.addr_table == addr_table) return i;
    }
    return -1;
}

int CallsignTable::oldest_slot() const {
    int victim = -1;
    for (int i = 0; i < kCapacity; i++) {
        const Entry& e = slots_[static_cast<size_t>(i)];
        if (!e.used) return i;
        if (victim < 0 || e.heard_s < slots_[static_cast<size_t>(victim)].heard_s) victim = i;
    }
    return victim;
}

}  // namespace skyblip::traffic
