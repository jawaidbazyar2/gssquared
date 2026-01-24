#pragma once

enum device_irq_id {
    IRQ_ID_SOUNDGLU = 8,
    IRQ_ID_KEYGLOO = 9,
    IRQ_ID_VGC = 10,   
};

typedef void (*device_irq_handler_t)(void *context);
struct device_irq_handler_s {
    device_irq_handler_t handler;
    void *context;
};