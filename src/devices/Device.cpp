#include "Device.hpp"


#include "devices/keyboard/keyboard.hpp"
#include "devices/speaker/speaker.hpp"
#include "display/display.hpp"
#include "devices/game/gamecontroller.hpp"
#include "devices/languagecard/languagecard.hpp"
#include "devices/prodos_clock/prodos_clock.hpp"
#include "devices/diskii/ndiskii.hpp"
#include "devices/memoryexpansion/memexp.hpp"
#include "devices/thunderclock_plus/thunderclockplus.hpp"
#include "devices/pdblock2/pdblock2.hpp"
#include "devices/pdblock3/pdblock3.hpp"
#include "devices/parallel/parallel.hpp"
#include "devices/videx/videx.hpp"
#include "devices/mockingboard/mb.hpp"
#include "devices/iiememory/iiememory.hpp"
#include "devices/applemouseii/mouse.hpp"
#include "devices/cassette/cassette.hpp"
#include "devices/vidhd/vidhd.hpp"
#include "devices/rtc/rtc_pram.hpp"
#include "devices/adb/keygloo.hpp"
#include "devices/es5503/soundglu.hpp"
#include "devices/scc8530/scc8530.hpp"
#include "devices/iwm/iwm_device.hpp"

Device *DeviceFactory::create(device_id id, SlotType_t slot)
{
    switch (id) {
        case DEVICE_ID_KEYBOARD_IIPLUS:
            return new KeyboardIIPlus(slot);
        case DEVICE_ID_KEYBOARD_IIE:
            return new KeyboardIIE(slot);
        default:
            return nullptr;
    }
}