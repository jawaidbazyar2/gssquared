#pragma once

#include "devices/displaypp/VideoScannerEvent.hpp"

enum device_irq_id {
    IRQ_SLOT_0 = 0,
    IRQ_SLOT_1 = 1,
    IRQ_SLOT_2 = 2,
    IRQ_SLOT_3 = 3,
    IRQ_SLOT_4 = 4,
    IRQ_SLOT_5 = 5,
    IRQ_SLOT_6 = 6,
    IRQ_SLOT_7 = 7,
    IRQ_ID_SOUNDGLU = 8,
    IRQ_ID_KEYGLOO = 9,       // ADB Micro Mouse interrupt
    IRQ_ID_ADB_DATAREG = 10,  // ADB Micro 'data register full' interrupt
    IRQ_ID_VGC = 11, 
    IRQ_ID_SCC = 12,
};

typedef void (*device_irq_handler_t)(void *context, VideoScannerEvent event);
struct device_irq_handler_s {
    device_irq_handler_t handler;
    void *context;
};