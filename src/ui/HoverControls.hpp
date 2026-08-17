#pragma once

#include "FadeContainer.hpp"
#include "LabeledButton.hpp"
#include "NClock.hpp"
#include <map>
#include "util/MenuInterface.h"
#include "SpeedSelect.hpp"
#include "DisplaySelect.hpp"

struct computer_t;

class HoverControls_t : public FadeContainer_t {
protected:
    LabeledButton *hov_speed = nullptr;    
    SpeedSelect_t *hov_speed_con = nullptr;

    LabeledButton *hov_display = nullptr;
    DisplaySelect *hov_display_con = nullptr;

    const std::map<int, int> speed_asset =  {
        {SPEED_FREE_RUN, MHzInfinityButton},
        {SPEED_1_0, MHz1_0Button},
        {SPEED_2_8, MHz2_8Button},
        {SPEED_7_1, MHz7_159Button},
        {SPEED_14_3, MHz14_318Button},
    };
    const std::map<int, int> monitor_asset =  {
        {MONITOR_COMPOSITE, ColorDisplayButton},
        {MONITOR_GS_RGB, RGBDisplayButton},
        {MONITOR_MONO_GREEN, GreenDisplayButton},
        {MONITOR_MONO_AMBER, AmberDisplayButton},
        {MONITOR_MONO_WHITE, WhiteDisplayButton},
    };
    MenuInterface *mi = nullptr;

public:
    HoverControls_t(UIContext *ctx, const Style_t& initial_style = Style_t(), computer_t *computer = nullptr);
    ~HoverControls_t();
    virtual void update() override;
};