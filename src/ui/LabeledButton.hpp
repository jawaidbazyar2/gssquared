#pragma once

#include "Button.hpp"

class LabeledButton : public Button_t {
    protected:

    public:       
        LabeledButton(AssetAtlas_t* assetp, int assetID, const std::string& button_text, TextRenderer* tr, int group = 0);
        void render(SDL_Renderer* renderer) override;
};