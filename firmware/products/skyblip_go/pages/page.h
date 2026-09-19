#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_PAGE_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_PAGE_H

#include <cstdint>

namespace skyblip::go {

enum class Page : uint8_t {
    Radar,
    Nearby,
    SixPack,
    GMeter,
    Status,
    Sats,
    RadioLog,
    SelfTest,
    kCount
};

constexpr int kPageCount = static_cast<int>(Page::kCount);

constexpr int32_t kDefaultRangeNm = 4;

constexpr int kWalkedPages = 4;

constexpr bool walked(Page page) { return static_cast<int>(page) < kWalkedPages; }

constexpr const char* page_title(Page page) {
    switch (page) {
        case Page::Radar: return "RADAR";
        case Page::Nearby: return "NEARBY";
        case Page::SixPack: return "SIX-PACK";
        case Page::Status: return "STATUS";
        case Page::Sats: return "SATELLITES";
        case Page::RadioLog: return "RADIO LOG";
        case Page::SelfTest: return "SELF TEST";
        case Page::GMeter: return "G METER";
        default: return "";
    }
}

constexpr Page menu_owner(Page page) { return walked(page) ? page : Page::Nearby; }

// INFO: fc 18sep26 a page off the walk was opened from a menu, so the pad hands it back there
constexpr Page page_after(Page page) {
    return walked(page) ? static_cast<Page>((static_cast<int>(page) + 1) % kWalkedPages)
                        : menu_owner(page);
}

}  // namespace skyblip::go

#endif
