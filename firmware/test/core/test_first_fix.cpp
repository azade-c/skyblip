// F5. The first solution is what a pilot wants confirmed and what is least worth transmitting.
#include "core/flight/state.h"
#include "core/gnss/first_fix.h"
#include "doctest/doctest.h"

using namespace skyblip;
using namespace skyblip::gnss;

namespace {

// A solution the extrapolation model predicted to `resid_m` L1 metres, height solved.
Convergence predicted(uint16_t resid_m) {
    Convergence c;
    c.fix_valid = true;
    c.resid_valid = true;
    c.height_solved = true;
    c.resid_m = resid_m;
    return c;
}

// The first fix of a run: no previous fix to have predicted it from.
Convergence unmeasured() {
    Convergence c;
    c.fix_valid = true;
    return c;
}

Convergence no_fix() { return Convergence{}; }

void hold(FirstFix& f, const Convergence& solution, uint32_t from_ms, int seconds) {
    for (int i = 0; i < seconds; i++) f.update(solution, from_ms + static_cast<uint32_t>(i) * 1000);
}

}  // namespace

TEST_CASE("first fix: the confirmation fires once, on the first solution ever") {
    FirstFix f;
    CHECK_FALSE(f.ever_fixed());
    CHECK_FALSE(f.take_acquired());

    f.update(no_fix(), 1000);
    CHECK_FALSE(f.take_acquired());

    f.update(unmeasured(), 5000);
    CHECK(f.ever_fixed());
    CHECK(f.take_acquired());
    CHECK_FALSE(f.take_acquired());  // consumed: nothing can chirp twice

    // A fix held over many ticks is still one acquisition.
    f.update(predicted(1), 6000);
    f.update(predicted(1), 7000);
    CHECK_FALSE(f.take_acquired());

    // Getting it back after losing it is not a second acquisition either.
    f.update(no_fix(), 8000);
    f.update(unmeasured(), 9000);
    CHECK_FALSE(f.take_acquired());
}

TEST_CASE("first fix: a receiver whose solutions still walk waits out the clock") {
    FirstFix f;
    f.update(unmeasured(), 1000);
    CHECK(f.fix_since_ms() == 1000u);
    CHECK_FALSE(f.settled(1000));

    // Forty metres of prediction error every second: solving, and still moving.
    hold(f, predicted(40), 2000, 18);
    CHECK(f.converged_fixes() == 0);
    CHECK_FALSE(f.settled(1000 + kFirstFixSettleMs - 1));
    CHECK(f.settled(1000 + kFirstFixSettleMs));
    CHECK(f.settled(1000 + kFirstFixSettleMs + 60000));
}

TEST_CASE("first fix: three solutions the model predicted settle it before the clock does") {
    FirstFix f;
    f.update(unmeasured(), 1000);

    f.update(predicted(kSettleResidualM - 1), 2000);
    CHECK_FALSE(f.settled(2000));
    f.update(predicted(kSettleResidualM - 1), 3000);
    CHECK_FALSE(f.settled(3000));
    f.update(predicted(kSettleResidualM - 1), 4000);
    CHECK(f.converged_fixes() == kSettleFixes);
    CHECK(f.settled(4000));
    // Three seconds, where the clock alone would have taken twenty.
    CHECK(4000 - 1000 < kFirstFixSettleMs);
}

TEST_CASE("first fix: one solution over the threshold takes the count back to nothing") {
    FirstFix f;
    f.update(unmeasured(), 1000);
    f.update(predicted(1), 2000);
    f.update(predicted(1), 3000);
    REQUIRE(f.converged_fixes() == 2);

    f.update(predicted(kSettleResidualM), 4000);
    CHECK(f.converged_fixes() == 0);
    CHECK_FALSE(f.settled(4000));

    f.update(predicted(1), 5000);
    f.update(predicted(1), 6000);
    CHECK_FALSE(f.settled(6000));
    f.update(predicted(1), 7000);
    CHECK(f.settled(7000));
}

TEST_CASE("first fix: a 2D solution never counts as converged") {
    FirstFix f;
    f.update(unmeasured(), 1000);

    Convergence flat = predicted(1);
    flat.height_solved = false;  // no VDOP: the receiver never solved for height
    hold(f, flat, 2000, 10);
    CHECK(f.converged_fixes() == 0);
    CHECK_FALSE(f.settled(11000));
    CHECK(f.settled(1000 + kFirstFixSettleMs));
}

TEST_CASE("first fix: losing the fix unsettles it, and the way back is shorter") {
    FirstFix f;
    f.update(unmeasured(), 1000);
    REQUIRE(f.settled(1000 + kFirstFixSettleMs));

    // A receiver that blinked has nothing settled to offer while it is out.
    f.update(no_fix(), 40000);
    CHECK_FALSE(f.settled(40000));
    CHECK_FALSE(f.settled(90000));

    // Back with a fix: the almanac and the filters are warm, so the second wait is the short one.
    f.update(unmeasured(), 50000);
    CHECK_FALSE(f.settled(50000 + kRefixSettleMs - 1));
    CHECK(f.settled(50000 + kRefixSettleMs));
    CHECK(kRefixSettleMs < kFirstFixSettleMs);
}

TEST_CASE("first fix: a fix lost mid-count starts the count again") {
    FirstFix f;
    f.update(unmeasured(), 1000);
    f.update(predicted(1), 2000);
    f.update(predicted(1), 3000);
    REQUIRE(f.converged_fixes() == 2);

    f.update(no_fix(), 4000);
    CHECK(f.converged_fixes() == 0);

    f.update(unmeasured(), 5000);
    f.update(predicted(1), 6000);
    f.update(predicted(1), 7000);
    CHECK_FALSE(f.settled(7000));
    f.update(predicted(1), 8000);
    CHECK(f.settled(8000));
}

TEST_CASE("first fix: the settling clock survives a millisecond counter that wraps") {
    FirstFix f;
    const uint32_t near_wrap = 0xFFFFFF00u;
    f.update(unmeasured(), near_wrap);
    CHECK_FALSE(f.settled(near_wrap + 1000));
    CHECK(f.settled(near_wrap + kFirstFixSettleMs));  // wraps through zero
}
