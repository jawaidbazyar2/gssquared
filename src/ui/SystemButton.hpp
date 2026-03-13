#pragma once

#include "Button.hpp"
#include "UIContext.hpp"
#include "systemconfig.hpp"
#include "platforms.hpp"

class SystemButton : public Button_t {
    public:
        SystemButton(UIContext *ctx, SystemConfig_t *system_config, Style_t style) : Button_t(ctx, get_platform(system_config->platform_id)->image_id, style, 0) {
            this->system_config = system_config;
        }
        ~SystemButton() { };
        void render() override;

    private:
        SystemConfig_t *system_config;
};