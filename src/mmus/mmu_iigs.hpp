#include "mmu.hpp"

class MMU_iigs : public MMU {
    protected:
        int ram_pages;
        uint8_t *main_ram = nullptr;
        
    public:

        MMU_iigs(int page_table_size, int ram_amount, uint8_t *rom_pointer);
        MMU_iigs(int page_table_size) : MMU(page_table_size) {
            ram_pages = (16 * 1024 * 1024) / page_table_size * GS2_PAGE_SIZE;
            main_ram = new uint8_t[ram_pages * GS2_PAGE_SIZE];
        };
        virtual ~MMU_iigs();
        uint8_t read(uint32_t address) override;
        void write(uint32_t address, uint8_t value) override;
        
        virtual uint8_t *get_rom_base();
        virtual uint8_t *get_memory_base() { return main_ram; };
        virtual void init_map();
        virtual void reset();
};
