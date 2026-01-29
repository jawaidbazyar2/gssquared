#pragma once

#include "devices/displaypp/VideoScannerEvent.hpp"

enum device_irq_id {
    IRQ_ID_SOUNDGLU = 8,
    IRQ_ID_KEYGLOO = 9,       // ADB Micro Mouse interrupt
    IRQ_ID_ADB_DATAREG = 10,  // ADB Micro 'data register full' interrupt
    IRQ_ID_VGC = 11,       
};

typedef void (*device_irq_handler_t)(void *context, VideoScannerEvent event);
struct device_irq_handler_s {
    device_irq_handler_t handler;
    void *context;
};