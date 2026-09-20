#include <cstdio>

#include "ui/canvas.h"
#include "ui/widgets/stone.h"

using skyblip::ui::Band;
using skyblip::ui::Cut;
using skyblip::ui::Trend;

namespace {

constexpr int kW = 19;
constexpr int kH = 25;
constexpr int kX = kW / 2;
constexpr int kY = kH / 2;

struct Family {
    const char* name;
    Cut cut;
    Band band;
    bool alarms;
};

constexpr Family kFamilies[] = {
    {"crown-near", Cut::Crown, Band::Near, true},
    {"diamond", Cut::Diamond, Band::Near, true},
    {"pavilion-near", Cut::Pavilion, Band::Near, true},
    {"crown-far", Cut::Crown, Band::Far, false},
    {"pavilion-far", Cut::Pavilion, Band::Far, false},
};

struct Motion {
    const char* name;
    Trend trend;
};

constexpr Motion kMotions[] = {
    {"climbing", Trend::Climbing},
    {"level", Trend::Level},
    {"sinking", Trend::Sinking},
};

void emit(const char* name, Cut cut, Band band, Trend trend, bool alarm) {
    skyblip::ui::Panel<kW, kH> panel;
    panel.clear();
    skyblip::ui::draw_stone(panel, kX, kY, cut, band, trend, alarm);

    std::printf("%s %d %d\n", name, kW, kH);
    for (int y = 0; y < kH; y++) {
        for (int x = 0; x < kW; x++) std::putchar(panel.get_pixel(x, y) ? '1' : '0');
        std::putchar('\n');
    }
}

}  // namespace

int main() {
    char name[64];
    for (const Family& family : kFamilies)
        for (const Motion& motion : kMotions) {
            std::snprintf(name, sizeof(name), "%s-%s", family.name, motion.name);
            emit(name, family.cut, family.band, motion.trend, false);
            if (!family.alarms) continue;
            std::snprintf(name, sizeof(name), "%s-%s-alarm", family.name, motion.name);
            emit(name, family.cut, family.band, motion.trend, true);
        }
    return 0;
}
