#include "Button.hpp"
#include "UIContext.hpp"
#include "SlotData.hpp"

#include <string>

class SlotButton : public Button_t {
    public:
        SlotButton(UIContext *ctx, int assetID, int group, int slot_number, const std::string& device_name);
        void render() override;
        void set_device_name(const std::string& name);
        int get_slot_number() const { return slot_number; }
    private:
        int slot_number;
        SlotType_t slot_type;
        std::string slot_string;
        std::string slot_device_name;
};
