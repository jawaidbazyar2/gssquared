#pragma once

#include "Button.hpp"
#include "systemconfig.hpp"

class SystemButton : public Button_t {
    public:
        SystemButton(AssetAtlas_t *aa, SystemConfig_t *system_config, Style_t style) : Button_t(aa, system_config->image_id, style, 0) {
            this->system_config = system_config;
        }
        ~SystemButton() { };
        void render(SDL_Renderer *renderer) override;

    private:
        SystemConfig_t *system_config;
};