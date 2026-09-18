#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_MENU_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_MENU_H

#include <cstdint>

#include "core/flight/atmosphere.h"
#include "products/skyblip_go/glass.h"
#include "products/skyblip_go/pages/page.h"
#include "products/skyblip_go/settings.h"

namespace skyblip::go {

// INFO: cf 02aug26 Identity is first and unchangeable, so the first pair of presses is harmless
enum class MenuRow : uint8_t {
    Identity,
    AircraftType,
    Alarm,
    Volume,
    Range,
    Units,
    Stealth,
    RadioLog,
    Sats,
    Status,
    SelfTest,
    GMeter,
    AlignQnh,
    QnhDown,
    QnhUp,
    kCount
};

struct Menu {
    const MenuRow* rows{nullptr};
    int n{0};
};

Menu menu_for(Page page);

struct MenuValues {
    go::Settings settings{};
    uint32_t qnh_pa{0};
    int32_t range_nm{kDefaultRangeNm};
    uint32_t pressure_pa{0};
    int32_t gnss_alt_cm{0};
    bool alignable{false};
};

struct MenuSnapshot {
    Page page{Page::Radar};
    MenuValues values{};
    MenuRow focus{MenuRow::Identity};
};

constexpr uint32_t kQnhStepPa = 100;
using flight::kQnhMaxPa;
using flight::kQnhMinPa;

constexpr uint8_t kMaxAlarmVolume = 5;

constexpr int32_t kRangeStepsNm[] = {1, 2, 4, 8};
constexpr int kRangeStepCount = static_cast<int>(sizeof(kRangeStepsNm) / sizeof(kRangeStepsNm[0]));

// INFO: fc 12sep26 ADS-L G.1.3 codes 11 up are UAV and reserved, nothing a pilot sits in
constexpr uint8_t kNamedAircraftTypes = 11;

enum class MenuAction : uint8_t { None, Moved, Changed, Open, Leave };

const char* menu_row_label(MenuRow row);
const char* aircraft_type_name(uint8_t code);
uint8_t next_aircraft_type(uint8_t code);
int32_t next_range_nm(int32_t nm);
uint32_t step_qnh_pa(uint32_t qnh_pa, bool up);
bool qnh_aligned_with_gnss(const MenuValues& values, uint32_t& out_pa);

constexpr bool opens_a_page(MenuRow row) {
    return row == MenuRow::RadioLog || row == MenuRow::Sats || row == MenuRow::Status ||
           row == MenuRow::SelfTest || row == MenuRow::GMeter;
}

Page page_behind(MenuRow row);

constexpr int kMenuValueCap = 16;
int menu_row_value(char* out, MenuRow row, const MenuValues& values);

constexpr int kMenuLeftX = 6;
constexpr int kMenuCellW = 6;
constexpr int kMenuRightX = 194;
constexpr int kMenuRowsTop = 24;
constexpr int kMenuRowHeight = 18;
constexpr int kMenuTextInset = 5;
constexpr int kMenuHintY = 190;
constexpr const char* kMenuHintText = "PAD MOVES  PRESS CHANGES";

constexpr int kQnhMinusX = 42;
constexpr int kQnhPlusX = 140;
constexpr int kQnhStepBoxW = 18;
constexpr int kQnhValueX = 76;

constexpr int menu_line_top(int line) { return kMenuRowsTop + line * kMenuRowHeight; }
constexpr int menu_line_text_y(int line) { return menu_line_top(line) + kMenuTextInset; }

int menu_row_index(const Menu& menu, MenuRow row);
int menu_row_line(const Menu& menu, MenuRow row);

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
