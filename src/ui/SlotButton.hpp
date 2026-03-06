#include "Button.hpp"
#include "SlotData.hpp"

class SlotManager_t;
class Device_t;

class SlotButton : public Button_t {
    public:
        SlotButton(AssetAtlas_t* assetp, int assetID, TextRenderer* tr, int group = 0, int slot_number = 0, SlotManager_t *slot_manager = nullptr);
        void render(SDL_Renderer* renderer) override;
    private:
        SlotManager_t *slot_manager;
        Device_t *device;
        int slot_number;
        SlotType_t slot_type;
        std::string slot_string;
        std::string slot_device_name;
};