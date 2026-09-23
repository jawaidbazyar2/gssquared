#pragma once

#include "FadeContainer.hpp"
#include "FaceButton.hpp"
#include "util/MenuInterface.h"
#include "SpeedSelect.hpp"
#include "DisplaySelect.hpp"

struct computer_t;

class HoverControls_t : public FadeContainer_t {
protected:
    FaceButton *hov_speed = nullptr;
    SpeedSelect_t *hov_speed_con = nullptr;

    FaceButton *hov_display = nullptr;
    DisplaySelect *hov_display_con = nullptr;

    MenuInterface *mi = nullptr;

public:
    HoverControls_t(UIContext *ctx, const Style_t& initial_style = Style_t(), computer_t *computer = nullptr);
    ~HoverControls_t();
    virtual void update() override;
};
