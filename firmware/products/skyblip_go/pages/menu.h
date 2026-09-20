#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_MENU_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_MENU_H

#include <cstdint>

#include "products/skyblip_go/glass.h"
#include "products/skyblip_go/pages/page.h"
#include "products/skyblip_go/settings.h"

namespace skyblip::go {

enum class MenuRow : uint8_t {
    AircraftType,
    Units,
    Range,
    Alarm,
    Volume,
    Status,
    Sats,
    RadioLog,
    Raw,
    Capture,
    SelfTest,
    kCount
};

struct Menu {
    const MenuRow* rows{nullptr};
    int n{0};
};

Menu menu_for(Page page);

struct MenuValues {
    go::Settings settings{};
    int range_step{kDefaultRangeStep};
};

struct MenuSnapshot {
    Page page{Page::Radar};
    MenuValues values{};
    MenuRow focus{MenuRow::AircraftType};
};

constexpr uint8_t kMaxAlarmVolume = 5;

// INFO: fc 12sep26 ADS-L G.1.3 codes 11 up are UAV and reserved, nothing a pilot sits in
constexpr uint8_t kNamedAircraftTypes = 11;

enum class MenuAction : uint8_t { None, Moved, Changed, Open, Leave };

const char* menu_row_label(MenuRow row);
const char* aircraft_type_name(uint8_t code);
uint8_t next_aircraft_type(uint8_t code);

constexpr bool opens_a_page(MenuRow row) {
    return row == MenuRow::RadioLog || row == MenuRow::Sats || row == MenuRow::Status ||
           row == MenuRow::Raw || row == MenuRow::Capture || row == MenuRow::SelfTest;
}

Page page_behind(MenuRow row);

constexpr int kMenuValueCap = 16;
int menu_row_value(char* out, MenuRow row, const MenuValues& values);

constexpr int kMenuScale = 2;
constexpr int kMenuLeftX = 6;
constexpr int kSmallCellW = 6;
constexpr int kMenuCellW = kSmallCellW * kMenuScale;
constexpr int kMenuRightX = 194;
constexpr int kMenuRowsTop = 26;
constexpr int kMenuRowHeight = 28;
constexpr int kMenuTextInset = 7;
constexpr int kMenuHintY = 190;
constexpr const char* kMenuHintText = "PAD MOVES    BUTTON CHANGES";

constexpr int text_cells(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

constexpr int kMenuHintX = (kGlassW - text_cells(kMenuHintText) * kSmallCellW) / 2;

constexpr int menu_line_top(int line) { return kMenuRowsTop + line * kMenuRowHeight; }
constexpr int menu_line_text_y(int line) { return menu_line_top(line) + kMenuTextInset; }

int menu_row_index(const Menu& menu, MenuRow row);

void draw_menu(ui::Canvas& fb, const MenuSnapshot& snapshot);

class MenuEditor {
   public:
    // INFO: cf 02aug26 a menu left open is the traffic picture taken away, and nobody dismissed it
    static constexpr uint32_t kIdleReturnMs = 60000;

    void enter(Page page, uint32_t now_ms);
    void leave();

    bool active() const { return active_; }
    Page page() const { return page_; }
    MenuRow focus() const;
    Page opening() const { return opening_; }

    void change(uint32_t now_ms);
    void next_row(uint32_t now_ms);

    MenuAction tick(uint32_t now_ms, const MenuValues& current, MenuValues& next);

   private:
    MenuAction act(const MenuValues& current, MenuValues& next);
    MenuAction advance();

    enum class Pending : uint8_t { None, Act, Advance };

    Page page_{Page::Radar};
    Page opening_{Page::Radar};
    int focus_{0};
    uint32_t idle_since_ms_{0};
    Pending pending_{Pending::None};
    bool active_{false};
};

}  // namespace skyblip::go

#endif
