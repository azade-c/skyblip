#include "products/skyblip_go/services/traffic.h"

#include "core/events/rf.h"
#include "core/flight/state.h"
#include "core/model/aircraft.h"
#include "core/model/ownship.h"
#include "core/timing/slot.h"

namespace skyblip::go {

// The one thing this service knows before any frame arrives: which aircraft is
// this one. A ground relay rebroadcasts everything it heard, us included, and
// the table is where that is refused (core/traffic/table.h).
Status TrafficService::setup() {
    context_.state.traffic.set_own_address(context_.roles.device_addr);
    uplink_ = has_feature(supported(declared_, context_.roles.capabilities), Feature::UplinkRx);
    return Status::Ok;
}

void TrafficService::tick(uint32_t now_ms) {
    // The reference the table's range gate measures a claimed position against
    // (core/traffic/sanity.h), refreshed before this pass drains a single frame:
    // own-ship has already run this pass, so this is the newest fix there is.
    context_.state.traffic.set_own_reference(context_.state.own);
    events::RfEvent event{};
    while (context_.bus.rf.pop(event)) {
        switch (event.type) {
            case events::RfEventType::RxDone: on_frame(event, now_ms); break;
            case events::RfEventType::CrcError:
                context_.state.air.rx_bad++;
                log(event, stamp_for(event, now_ms), radio::Event::BadCrc);
                break;
            case events::RfEventType::Missed:
                context_.state.air.tx_lost++;
                log(event, stamp_for(event, now_ms), radio::Event::Lost);
                break;
            // The executor's own timestamp, carried alongside the counter it
            // already bumps: RadioService owns the deadline this closes
            // against, and reads it from here rather than a second drain of
            // the same bus.
            case events::RfEventType::TxDone:
                context_.state.air.tx_ok++;
                context_.state.air.last_tx_done_at_us = event.at_us;
                log(event, stamp_for(event, now_ms), radio::Event::Transmitted);
                break;
        }
    }
    context_.state.traffic.age_out(context_.state.traffic_now(now_ms));
}

events::Stamp TrafficService::stamp_for(const events::RfEvent& event, uint32_t now_ms) const {
    const bus::State& state = context_.state;
    return events::stamp_of(event.at_us, state.clock.pps_edge_us, state.clock.pps_locked,
                            state.traffic_now(now_ms));
}

// INFO: fc 16sep26 §C.5: slot 1 reaches kSlot1Wrap past the second its sender keyed in
uint32_t TrafficService::keyed_utc(const events::Stamp& stamp, uint32_t now_s) {
    if (!stamp.phase_valid) return now_s;
    const bool in_slot1_tail = stamp.into_ms < timing::kSlot1Wrap;
    return (in_slot1_tail && stamp.at_s > 0) ? stamp.at_s - 1 : stamp.at_s;
}

void TrafficService::log(const events::RfEvent& event, const events::Stamp& stamp,
                         radio::Event outcome, const model::AircraftObs* obs) {
    const bus::State& state = context_.state;
    radio::Entry entry{};
    entry.event = outcome;
    entry.band = event.band;
    entry.channel = event.freq_hz == timing::kMband1Hz ? 1 : 0;
    entry.at_s = stamp.at_s;
    entry.into_ms = stamp.into_ms;
    entry.phase_valid = stamp.phase_valid;
    entry.utc = state.own.utc_valid;
    entry.airborne = flight::airborne(state.own.flight_state);
    entry.rssi_dbm = event.rssi_dbm;
    entry.rssi_valid = event.rssi_valid;
    if (event.type == events::RfEventType::RxDone) entry.len = event.len;
    if (outcome == radio::Event::Transmitted && state.rf.tx_deadline_us != 0) {
        entry.tx_keyed_us = radio::tx_span_of(event.keyed_at_us, state.rf.tx_deadline_us);
        entry.tx_span_us = radio::tx_span_of(event.at_us, state.rf.tx_deadline_us);
        entry.tx_span_valid = true;
        context_.state.rf.last_tx_keyed_us = entry.tx_keyed_us;
        context_.state.rf.last_tx_span_us = entry.tx_span_us;
    }
    if (obs != nullptr) {
        entry.source = obs->source;
        entry.addr = obs->addr;
    }
    context_.state.radio_log.record(entry);
}

void TrafficService::on_frame(const events::RfEvent& event, uint32_t now_ms) {
    const events::Stamp stamp = stamp_for(event, now_ms);
    protocol::Frame frame{};
    const protocol::System system =
        protocol::receive_burst(event.band, event.data.data(), event.len, frame);
    if (system == protocol::System::AdslUplink) {
        on_uplink(event, stamp, now_ms);
        return;
    }
    if (system == protocol::System::Unknown) {
        if (protocol::framed_noise(frame)) {
            context_.state.air.rx_noise++;
            return;
        }
        count_refusal(radio::Event::Undecoded);
        log(event, stamp, radio::Event::Undecoded);
        return;
    }

    const uint32_t utc = context_.state.traffic_now(now_ms);
    const uint32_t keyed = keyed_utc(stamp, utc);
    model::AircraftObs obs{};
    const bool alptas = system == protocol::System::Alptas;
    const radio::Event outcome = alptas ? decode_alptas(frame, keyed, stamp.phase_valid, obs)
                                        : decode_adsl(frame, keyed, stamp, obs);
    if (outcome != radio::Event::Received) {
        count_refusal(outcome);
        log(event, stamp, outcome);
        return;
    }

    obs.received.into_ms = stamp.into_ms;
    obs.received.phase_valid = stamp.phase_valid;
    obs.at_ms = now_ms;
    obs.rssi_dbm = event.rssi_dbm;
    context_.state.traffic.update(obs, utc);
    context_.state.air.rx_ok++;
    log(event, stamp, radio::Event::Received, &obs);
}

// One frame from the ground, up to thirteen aircraft in it (§C.4's higher rate
// buys the room the M band has no space for), each one an observation in its
// own right.
// The counters are the uplink's own: a codeword Reed-Solomon refuses is not an
// M-band framing failure, and counting it as one is what let this whole path go
// missing without a single number moving.
void TrafficService::on_uplink(const events::RfEvent& event, const events::Stamp& stamp,
                               uint32_t now_ms) {
    if (!uplink_) return;
    context_.state.air.uplink_frames++;

    model::AircraftObs relayed[protocol::AdslUplink::kMaxTargets];
    protocol::AdslUplink::DecodeStats stats{};
    if (uplink_codec_.decode(event.data.data(), relayed, protocol::AdslUplink::kMaxTargets,
                             stats) != Status::Ok) {
        context_.state.air.uplink_bad++;
        log(event, stamp, radio::Event::BadCrc);
        return;
    }

    // The relay's own reception is older than this second by however long the
    // ground station took to compose the frame, and nothing in it says by how
    // much. Stamping it with the second it arrived in is the only honest
    // reading, and core/traffic/table.h is what stops that recency from
    // outranking a direct reception of the same aircraft.
    model::AircraftObs relay{};
    relay.source = model::Source::AdslUplink;
    log(event, stamp, radio::Event::Received, &relay);

    const uint32_t utc = context_.state.traffic_now(now_ms);
    for (int i = 0; i < stats.targets; i++) {
        model::AircraftObs& obs = relayed[i];
        obs.received = stamp;
        obs.received.at_s = utc;
        obs.at_ms = now_ms;
        obs.rssi_dbm = event.rssi_dbm;
        if (context_.state.traffic.update(obs, utc) >= 0) context_.state.air.uplink_targets++;
    }
}

void TrafficService::count_refusal(radio::Event outcome) {
    switch (outcome) {
        case radio::Event::Unattempted: context_.state.air.rx_wait++; break;
        case radio::Event::Unsupported: context_.state.air.rx_type++; break;
        default: context_.state.air.rx_bad++; break;
    }
}

// The Manchester error map travels with the frame, so the forward correction
// knows which bits the air already told us not to trust.
radio::Event TrafficService::decode_adsl(protocol::Frame& frame, uint32_t utc,
                                         const events::Stamp& stamp, model::AircraftObs& obs) {
    protocol::AdslPacket p{};
    p.init();
    __builtin_memcpy(&p.Version, frame.data, protocol::kAdslFrameBytes);
    if (p.check_crc() != 0 && (p.correct(frame.err) < 0 || p.check_crc() != 0))
        return radio::Event::BadCrc;
    p.descramble();
    events::Stamp received = stamp;
    received.at_s = utc;
    if (!protocol::to_obs(p, received, 0, model::Source::AdslDirect, obs))
        return radio::Event::Undecoded;
    return radio::Event::Received;
}

// INFO: fc 16sep26 ALP-TAS keys on the second its sender keyed in, so an undated burst is a guess
radio::Event TrafficService::decode_alptas(const protocol::Frame& frame, uint32_t utc, bool dated,
                                           model::AircraftObs& obs) const {
    const model::OwnState& own = context_.state.own;
    if (!own.fix_valid || !own.utc_valid) return radio::Event::Unattempted;
    if (!protocol::alptas_crc_ok(frame.data)) return radio::Event::BadCrc;
    const int32_t lat = own.lat_1e7;
    const int32_t lon = own.lon_1e7;
    const radio::Event first = verdict_of(protocol::alptas_decode(frame.data, utc, lat, lon, obs));
    if (first != radio::Event::Undecoded || dated || utc == 0) return first;
    return verdict_of(protocol::alptas_decode(frame.data, utc - 1, lat, lon, obs));
}

radio::Event TrafficService::verdict_of(Status decoded) {
    switch (decoded) {
        case Status::Ok: return radio::Event::Received;
        case Status::Unsupported: return radio::Event::Unsupported;
        default: return radio::Event::Undecoded;
    }
}

}  // namespace skyblip::go
