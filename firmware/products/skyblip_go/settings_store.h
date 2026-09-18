#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_SETTINGS_STORE_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_SETTINGS_STORE_H

#include "core/comms/config_store.h"
#include "products/skyblip_go/settings.h"

namespace skyblip::go {

class SettingsStore : public comms::ConfigStore {
   public:
    explicit SettingsStore(Settings& settings) : settings_(settings) {}

    void write_fields(json::Writer& w) const override { write_json_fields(w, settings_); }

    Status apply(const char* json, int len) override { return apply_json(settings_, json, len); }

   private:
    Settings& settings_;
};

}  // namespace skyblip::go

#endif
