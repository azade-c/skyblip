#include "products/skyblip_go/pages/installing.h"

namespace skyblip::go {

void draw_installing(ui::Canvas& fb) {
    fb.clear(true);
    fb.rect(0, 0, kGlassW, 26, true, /*fill=*/true);
    fb.draw_text(kInstallingLeftX + kInstallingCellW, 6, kInstallingHeader, false, 2);

    fb.draw_text(kInstallingLeftX, kInstallingTitleY, kInstallingTitle, true, 2);
    fb.hline(kInstallingLeftX, kInstallingTitleY + 22, kGlassW - 2 * kInstallingLeftX, true);

    for (int row = 0; row < kInstallingBodyRows; row++)
        fb.draw_text(kInstallingLeftX, installing_body_y(row), kInstallingBody[row], true, 1);
}

}  // namespace skyblip::go
