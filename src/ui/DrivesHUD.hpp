#pragma once

#include "Container.hpp"
#include "StorageButton.hpp"
#include "util/mount.hpp"

class DrivesHUD_t : public Container_t {
protected:
    int drives_x = 0;
    int drives_y = 0;

    std::vector<StorageButton *> buttons;
    Mounts *mounts = nullptr;

public:
    DrivesHUD_t(UIContext *ctx, const Style_t& style, Mounts *mounts);
    void render() override;
    void update() override;
};