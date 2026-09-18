// A contact, conditioned: one settled edge per thing a finger did, stamped when the level changed.
#include "core/input/contact.h"
#include "doctest/doctest.h"

using namespace skyblip;

namespace {

constexpr uint32_t kSettleMs = 30;

using Edge = input::Contact::Edge;

Edge step(input::Contact& contact, bool level, uint32_t& t, uint32_t ms) {
    Edge seen = Edge::None;
    for (uint32_t elapsed = 0; elapsed <= ms; elapsed += 10) {
        const Edge edge = contact.update(level, t + elapsed);
        if (edge != Edge::None) seen = edge;
    }
    t += ms;
    return seen;
}

}  // namespace

TEST_CASE("contact: one down and one up per press, whatever the level did between") {
    input::Contact c{kSettleMs};
    uint32_t t = 1000;
    CHECK(step(c, true, t, 200) == Edge::Down);
    CHECK(c.down());
    CHECK(step(c, true, t, 5000) == Edge::None);
    CHECK(step(c, false, t, 200) == Edge::Up);
    CHECK_FALSE(c.down());
    CHECK(step(c, false, t, 5000) == Edge::None);
}

TEST_CASE("contact: bounce inside the settle window is not an edge") {
    input::Contact c{kSettleMs};
    uint32_t t = 0;
    const bool chatter[] = {true, false, true, false, true};
    for (bool level : chatter) {
        CHECK(c.update(level, t) == Edge::None);
        t += 1;
    }
    CHECK_FALSE(c.down());
    CHECK(step(c, true, t, 200) == Edge::Down);
}

TEST_CASE("contact: a spike shorter than the settle is ignored entirely") {
    input::Contact c{kSettleMs};
    c.update(false, 0);
    CHECK(c.update(true, 100) == Edge::None);
    CHECK(c.update(false, 100 + kSettleMs - 10) == Edge::None);
    uint32_t t = 110;
    CHECK(step(c, false, t, 500) == Edge::None);
    CHECK_FALSE(c.down());
}

// A late poll timed at the poll turns a long touch into a tap, and steals the way home.
TEST_CASE("contact: an edge is stamped when the level changed, not when it was seen") {
    input::Contact c{kSettleMs};
    c.update(true, 1000);
    REQUIRE(c.update(true, 1000 + kSettleMs) == Edge::Down);
    CHECK(c.edge_ms() == 1000);

    CHECK(c.update(false, 4000) == Edge::None);
    REQUIRE(c.update(false, 9000) == Edge::Up);
    CHECK(c.edge_ms() == 4000);
}

TEST_CASE("contact: the settle window is the board's, so two contacts can differ") {
    input::Contact quick{5};
    input::Contact slow{200};
    uint32_t t = 0;
    CHECK(step(quick, true, t, 50) == Edge::Down);
    uint32_t t2 = 0;
    CHECK(step(slow, true, t2, 50) == Edge::None);
    CHECK(step(slow, true, t2, 200) == Edge::Down);
}
