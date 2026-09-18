// NO FIX is one word for a hangar roof and for a receiver twenty seconds from solving.
#include <string>

#include "core/gnss/acquisition.h"
#include "core/gnss/validity.h"
#include "doctest/doctest.h"

using namespace skyblip;
using namespace skyblip::gnss;

namespace {

GnssSolution searching() { return GnssSolution{}; }

GnssSolution timed() {
    GnssSolution s;
    s.utc_valid = true;
    s.utc = 1767225600;
    return s;
}

GnssSolution fixed() {
    GnssSolution s = timed();
    s.is_fix = true;
    return s;
}

}  // namespace

TEST_CASE("acquisition: a receiver that has said nothing is silent, not searching") {
    Acquisition a;
    CHECK(a.stage() == Stage::Silent);

    a.tick(3000);
    CHECK(a.stage() == Stage::Silent);
    // The count runs from boot, so a bench sees how long it has been quiet.
    CHECK(a.stage_ms(3000) == 3000u);
}

TEST_CASE("acquisition: sentences with no date are a receiver searching") {
    Acquisition a;
    a.observe(searching(), 1000);
    CHECK(a.stage() == Stage::Search);
    CHECK(a.stage_ms(4000) == 3000u);

    a.observe(searching(), 2000);
    a.tick(2000);
    CHECK(a.stage() == Stage::Search);
    // Still the same stage, so the clock behind it was not restarted.
    CHECK(a.stage_ms(4000) == 3000u);
}

// A date can only be read off a satellite, so it is the rung that says the antenna sees sky.
TEST_CASE("acquisition: a decoded date is a satellite read, and outranks searching") {
    Acquisition a;
    a.observe(searching(), 1000);
    a.observe(timed(), 2000);
    CHECK(a.stage() == Stage::Time);
    CHECK(a.stage_ms(5000) == 3000u);

    a.observe(fixed(), 6000);
    CHECK(a.stage() == Stage::Fixed);
    CHECK(a.stage_ms(6000) == 0u);
}

TEST_CASE("acquisition: a receiver that stops talking goes silent, whatever it last said") {
    Acquisition a;
    a.observe(fixed(), 1000);
    REQUIRE(a.stage() == Stage::Fixed);

    a.tick(1000 + kSentenceMaxAgeMs - 1);
    CHECK(a.stage() == Stage::Fixed);

    a.tick(1000 + kSentenceMaxAgeMs);
    CHECK(a.stage() == Stage::Silent);
    CHECK(a.stage_ms(1000 + kSentenceMaxAgeMs) == 0u);

    a.observe(timed(), 9000);
    CHECK(a.stage() == Stage::Time);
}

TEST_CASE("acquisition: every stage has a word short enough for the ring") {
    CHECK(std::string(stage_name(Stage::Silent)) == "SILENT");
    CHECK(std::string(stage_name(Stage::Search)) == "SEARCH");
    CHECK(std::string(stage_name(Stage::Time)) == "TIME");
    CHECK(std::string(stage_name(Stage::Fixed)) == "FIX");
    for (uint8_t i = 0; i <= static_cast<uint8_t>(Stage::Fixed); i++)
        CHECK(std::string(stage_name(static_cast<Stage>(i))).size() <= 6);
}
