/*
 *   Copyright (c) 2025 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * This file is 6502 / 65c02 instruction processing.
 * 
 * This file is included from a wrapper that places all the functions inside it, into a
 * namespace. 98% of the architecture is identical.
 * 
 * If #define CPU_65C02 is set, additional instructions will be
 * exposed at compile-time.
 * 
 * The end result is two modules, one with the complete 6502 code in it,
 * one with the complete 65c02 code in it. No runtime if-then is done at
 * the instruction level. Only at the CPU module level.
 * 
 * It will eventually be used one more time in this same manner, which is a
 * version for the 65c816 in 8-bit "emulation mode". It will have a few different
 * instructions yet from the 6502 / 65c02. And then there will need to be something
 * very different for the 16-bit mode stuff.
 */

#include "opcodes.hpp"

#include "core_6502.hpp"

/**
 * References: 
 * Apple Machine Language: Don Inman, Kurt Inman
 * https://www.righto.com/2012/12/the-6502-overflow-flag-explained.html?m=1
 * https://www.masswerk.at/6502/6502_instruction_set.html#USBC
 * 
 */

/**
 * TODO: Decide whether to implement the 'illegal' instructions. Maybe have that as a processor variant? the '816 does not implement them.
 * 
 */
namespace cpu_6502 {

int execute_next(cpu_state *cpu) {

    system_trace_entry_t *tb = &cpu->trace_entry;
    TRACE(
    if (cpu->trace) {
    tb->cycle = cpu->cycles;
    tb->pc = cpu->pc;
    tb->a = cpu->a_lo;
    tb->x = cpu->x_lo;
    tb->y = cpu->y_lo;
    tb->sp = cpu->sp;
    tb->d = cpu->d;
    tb->p = cpu->p;
    tb->db = cpu->db;
    tb->pb = cpu->pb;
    tb->eaddr = 0;
    }
    )

#if 0
    if (DEBUG(DEBUG_CLOCK)) {
        uint64_t current_time = get_current_time_in_microseconds();
        fprintf(stdout, "[ %llu ]", cpu->cycles);
        uint64_t elapsed_time = current_time - cpu->boot_time;
        fprintf(stdout, "[eTime: %llu] ", elapsed_time);
        float cycles_per_second = (cpu->cycles * 1000000000.0) / (elapsed_time * 1000.0);
        fprintf(stdout, "[eHz: %.0f] ", cycles_per_second);
    }
#endif

    if (!cpu->I && cpu->irq_asserted) { // if IRQ is not disabled, and IRQ is asserted, handle it.
        push_word(cpu, cpu->pc); // push current PC
        push_byte(cpu, cpu->p | FLAG_UNUSED); // break flag and Unused bit set to 1.
        cpu->p |= FLAG_I; // interrupt disable flag set to 1.
        cpu->pc = cpu->read_word(IRQ_VECTOR);
        cpu->incr_cycles();
        cpu->incr_cycles();
        return 0;
    }

    opcode_t opcode = cpu->read_byte_from_pc();
    tb->opcode = opcode;

    switch (opcode) {

        /* ADC --------------------------------- */
        case OP_ADC_IMM: /* ADC Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                add_and_set_flags(cpu, N);                    
            }
            break;

        case OP_ADC_ZP: /* ADC ZP */
            {
                byte_t N = get_operand_zeropage(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ZP_X: /* ADC ZP, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ABS: /* ADC Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ABS_X: /* ADC Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ABS_Y: /* ADC Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_IND_X: /* ADC (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_IND_Y: /* ADC (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

    /* AND --------------------------------- */

        case OP_AND_IMM: /* AND Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                cpu->a_lo &= N; // replace with an and_and_set_flags 
                set_n_z_flags(cpu, cpu->a_lo); 
            }
            break;

        case OP_AND_ZP: /* AND Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_ZP_X: /* AND Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_ABS: /* AND Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;
        
        case OP_AND_ABS_X: /* AND Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_ABS_Y: /* AND Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_IND_X: /* AND (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_IND_Y: /* AND (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

    /* ASL --------------------------------- */

    case OP_ASL_ACC: /* ASL Accumulator */
        {
            byte_t N = cpu->a_lo;
            cpu->a_lo = arithmetic_shift_left(cpu, N);
        }
        break;

    case OP_ASL_ZP: /* ASL Zero Page */
        {
            absaddr_t addr = get_operand_address_zeropage(cpu);
            arithmetic_shift_left_addr(cpu, addr);
        }
        break;

    case OP_ASL_ZP_X: /* ASL Zero Page, X */
        {
            zpaddr_t zpaddr = get_operand_address_zeropage_x(cpu);
            arithmetic_shift_left_addr(cpu, zpaddr);
        }
        break;

    case OP_ASL_ABS: /* ASL Absolute */
        {
            absaddr_t addr = get_operand_address_absolute(cpu);
            arithmetic_shift_left_addr(cpu, addr);
        }
        break;
        
    case OP_ASL_ABS_X: /* ASL Absolute, X */
        {
            absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
            arithmetic_shift_left_addr(cpu, addr);
        }
        break;

    /* Branching --------------------------------- */
        case OP_BCC_REL: /* BCC Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->C == 0);
            }
            break;

        case OP_BCS_REL: /* BCS Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->C == 1);
            }
            break;

        case OP_BEQ_REL: /* BEQ Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->Z == 1);
            }
            break;

        case OP_BNE_REL: /* BNE Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->Z == 0);
            }
            break;

        case OP_BMI_REL: /* BMI Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->N == 1);
            }
            break;

        case OP_BPL_REL: /* BPL Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->N == 0);
            }
            break;

        case OP_BVC_REL: /* BVC Relative */
            {
                uint8_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->V == 0);
            }
            break;

        case OP_BVS_REL: /* BVS Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->V == 1);
            }
            break;

    /* CMP --------------------------------- */
        case OP_CMP_IMM: /* CMP Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_ZP: /* CMP Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_ZP_X: /* CMP Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_ABS: /* CMP Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_ABS_X: /* CMP Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_ABS_Y: /* CMP Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_IND_X: /* CMP (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_IND_Y: /* CMP (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

    /* CPX --------------------------------- */
        case OP_CPX_IMM: /* CPX Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                compare_and_set_flags(cpu, cpu->x_lo, N);
            }
            break;

        case OP_CPX_ZP: /* CPX Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                compare_and_set_flags(cpu, cpu->x_lo, N);
            }
            break;

        case OP_CPX_ABS: /* CPX Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                compare_and_set_flags(cpu, cpu->x_lo, N);
            }
            break;

    /* CPY --------------------------------- */
        case OP_CPY_IMM: /* CPY Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                compare_and_set_flags(cpu, cpu->y_lo, N);
            }
            break;

        case OP_CPY_ZP: /* CPY Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                compare_and_set_flags(cpu, cpu->y_lo, N);
            }
            break;

        case OP_CPY_ABS: /* CPY Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                compare_and_set_flags(cpu, cpu->y_lo, N);
            }
            break;

    /* DEC --------------------------------- */
        case OP_DEC_ZP: /* DEC Zero Page */
            {
                zpaddr_t zpaddr = get_operand_address_zeropage(cpu);
                dec_operand(cpu, zpaddr);
            }
            break;

        case OP_DEC_ZP_X: /* DEC Zero Page, X */
            {
                zpaddr_t zpaddr = get_operand_address_zeropage_x(cpu);
                dec_operand(cpu, zpaddr);
            }
            break;

        case OP_DEC_ABS: /* DEC Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                dec_operand(cpu, addr);
            }
            break;

        case OP_DEC_ABS_X: /* DEC Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                dec_operand(cpu, addr);
            }
            break;

    /* DE(xy) --------------------------------- */
        case OP_DEX_IMP: /* DEX Implied */
            {
                cpu->x_lo --;
                cpu->incr_cycles();
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_DEY_IMP: /* DEY Implied */
            {
                cpu->y_lo --;
                cpu->incr_cycles();
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;


    /* EOR --------------------------------- */

        case OP_EOR_IMM: /* EOR Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_ZP: /* EOR Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_ZP_X: /* EOR Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_ABS: /* EOR Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;
        
        case OP_EOR_ABS_X: /* EOR Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_ABS_Y: /* EOR Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_IND_X: /* EOR (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_IND_Y: /* EOR (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

    /* INC --------------------------------- */
        case OP_INC_ZP: /* INC Zero Page */
            {
                zpaddr_t zpaddr = get_operand_address_zeropage(cpu);
                inc_operand(cpu, zpaddr);
            }
            break;

        case OP_INC_ZP_X: /* INC Zero Page, X */
            {
                zpaddr_t zpaddr = get_operand_address_zeropage_x(cpu);
                inc_operand(cpu, zpaddr);
            }
            break;

        case OP_INC_ABS: /* INC Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                inc_operand(cpu, addr);
            }
            break;

        case OP_INC_ABS_X: /* INC Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                inc_operand(cpu, addr);
            }
            break;

    /* IN(xy) --------------------------------- */

        case OP_INX_IMP: /* INX Implied */
            {
                cpu->x_lo ++;
                cpu->incr_cycles();
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_INY_IMP: /* INY Implied */
            {
                cpu->y_lo ++;
                cpu->incr_cycles();
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

    /* LDA --------------------------------- */

        case OP_LDA_IMM: /* LDA Immediate */
            {
                cpu->a_lo = get_operand_immediate(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_ZP: /* LDA Zero Page */
            {
                cpu->a_lo =  get_operand_zeropage(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_ZP_X: /* LDA Zero Page, X */
            {
                cpu->a_lo = get_operand_zeropage_x(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_ABS: /* LDA Absolute */
            {
                cpu->a_lo = get_operand_absolute(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;
        
        case OP_LDA_ABS_X: /* LDA Absolute, X */
            {
                cpu->a_lo = get_operand_absolute_x(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_ABS_Y: /* LDA Absolute, Y */
            {
                cpu->a_lo = get_operand_absolute_y(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_IND_X: /* LDA (Indirect, X) */
            {
                cpu->a_lo = get_operand_zeropage_indirect_x(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_IND_Y: /* LDA (Indirect), Y */
            {
                cpu->a_lo = get_operand_zeropage_indirect_y(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        /* LDX --------------------------------- */

        case OP_LDX_IMM: /* LDX Immediate */
            {
                cpu->x_lo = get_operand_immediate(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_LDX_ZP: /* LDX Zero Page */
            {
                cpu->x_lo = get_operand_zeropage(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_LDX_ZP_Y: /* LDX Zero Page, Y */
            {
                cpu->x_lo = get_operand_zeropage_y(cpu);
                cpu->incr_cycles(); // ldx zp, y uses an extra cycle.
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_LDX_ABS: /* LDX Absolute */
            {
                cpu->x_lo = get_operand_absolute(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_LDX_ABS_Y: /* LDX Absolute, Y */
            {
                cpu->x_lo = get_operand_absolute_y(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        /* LDY --------------------------------- */

        case OP_LDY_IMM: /* LDY Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;
        
        case OP_LDY_ZP: /* LDY Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

        case OP_LDY_ZP_X: /* LDY Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

        case OP_LDY_ABS: /* LDY Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

        case OP_LDY_ABS_X: /* LDY Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

    /* LSR  --------------------------------- */

        case OP_LSR_ACC: /* LSR Accumulator */
            {
                byte_t N = cpu->a_lo;
                cpu->a_lo = logical_shift_right(cpu, N);
            }
            break;

        case OP_LSR_ZP: /* LSR Zero Page */
            {
                absaddr_t addr = get_operand_address_zeropage(cpu);
                logical_shift_right_addr(cpu, addr);
            }
            break;

        case OP_LSR_ZP_X: /* LSR Zero Page, X */
            {
                absaddr_t addr = get_operand_address_zeropage_x(cpu);
                logical_shift_right_addr(cpu, addr);
            }
            break;

        case OP_LSR_ABS: /* LSR Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                logical_shift_right_addr(cpu, addr);
            }
            break;

        case OP_LSR_ABS_X: /* LSR Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                logical_shift_right_addr(cpu, addr);
            }
            break;


    /* ORA --------------------------------- */

        case OP_ORA_IMM: /* AND Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_ZP: /* AND Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_ZP_X: /* AND Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_ABS: /* AND Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;
        
        case OP_ORA_ABS_X: /* AND Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_ABS_Y: /* AND Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_IND_X: /* AND (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_IND_Y: /* AND (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

    /* Stack operations --------------------------------- */

        case OP_PHA_IMP: /* PHA Implied */
            {
                push_byte(cpu, cpu->a_lo);
            }
            break;

        case OP_PHP_IMP: /* PHP Implied */
            {
                push_byte(cpu, (cpu->p | (FLAG_B | FLAG_UNUSED))); // break flag and Unused bit set to 1.
            }
            break;

        case OP_PLP_IMP: /* PLP Implied */
            {
                cpu->p = pop_byte(cpu) & ~FLAG_B; // break flag is cleared.
                cpu->incr_cycles(); // TODO: where should this extra cycle actually go?
            }
            break;

        case OP_PLA_IMP: /* PLA Implied */
            {
                cpu->a_lo = pop_byte(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
                cpu->incr_cycles();
            }
            break;
    /* ROL --------------------------------- */

        case OP_ROL_ACC: /* ROL Accumulator */
            {
                byte_t N = cpu->a_lo;
                cpu->a_lo = rotate_left(cpu, N);
            }
            break;

        case OP_ROL_ZP: /* ROL Zero Page */
            {
                absaddr_t addr = get_operand_address_zeropage(cpu);
                rotate_left_addr(cpu, addr);
            }
            break;

        case OP_ROL_ZP_X: /* ROL Zero Page, X */
            {
                absaddr_t addr = get_operand_address_zeropage_x(cpu);
                rotate_left_addr(cpu, addr);
            }
            break;

        case OP_ROL_ABS: /* ROL Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                rotate_left_addr(cpu, addr);
            }
            break;

        case OP_ROL_ABS_X: /* ROL Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                rotate_left_addr(cpu, addr);
            }
            break;

    /* ROR --------------------------------- */
        case OP_ROR_ACC: /* ROR Accumulator */
            {
                byte_t N = cpu->a_lo;
                cpu->a_lo = rotate_right(cpu, N);
            }
            break;

        case OP_ROR_ZP: /* ROR Zero Page */
            {
                absaddr_t addr = get_operand_address_zeropage(cpu);
                rotate_right_addr(cpu, addr);
            }
            break;

        case OP_ROR_ZP_X: /* ROR Zero Page, X */
            {
                absaddr_t addr = get_operand_address_zeropage_x(cpu);
                rotate_right_addr(cpu, addr);
            }
            break;

        case OP_ROR_ABS: /* ROR Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                rotate_right_addr(cpu, addr);
            }
            break;

        case OP_ROR_ABS_X: /* ROR Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                rotate_right_addr(cpu, addr);
            }
            break;

        /* SBC --------------------------------- */
        case OP_SBC_IMM: /* SBC Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ZP: /* SBC Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ZP_X: /* SBC Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ABS: /* SBC Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ABS_X: /* SBC Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ABS_Y: /* SBC Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_IND_X: /* SBC (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_IND_Y: /* SBC (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        /* STA --------------------------------- */
        case OP_STA_ZP: /* STA Zero Page */
            {
                store_operand_zeropage(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_ZP_X: /* STA Zero Page, X */
            {
                store_operand_zeropage_x(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_ABS: /* STA Absolute */
            {
                store_operand_absolute(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_ABS_X: /* STA Absolute, X */
            {
                store_operand_absolute_x(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_ABS_Y: /* STA Absolute, Y */
            {
                store_operand_absolute_y(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_IND_X: /* STA (Indirect, X) */
            {
                store_operand_zeropage_indirect_x(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_IND_Y: /* STA (Indirect), Y */
            {
                store_operand_zeropage_indirect_y(cpu, cpu->a_lo);
            }
            break;
        
        /* STX --------------------------------- */
        case OP_STX_ZP: /* STX Zero Page */
            {
                store_operand_zeropage(cpu, cpu->x_lo);
            }
            break;

        case OP_STX_ZP_Y: /* STX Zero Page, Y */
            {
                store_operand_zeropage_y(cpu, cpu->x_lo);
                cpu->incr_cycles(); // ldx zp, y uses an extra cycle.
                // TODO: look into this and see where the extra cycle might need to actually go.
            }
            break;

        case OP_STX_ABS: /* STX Absolute */
            {
                store_operand_absolute(cpu, cpu->x_lo);
            }
            break;

    /* STY --------------------------------- */
        case OP_STY_ZP: /* STY Zero Page */
            {
                store_operand_zeropage(cpu, cpu->y_lo);
            }
            break;

        case OP_STY_ZP_X: /* STY Zero Page, X */
            {
                store_operand_zeropage_x(cpu, cpu->y_lo);
            }
            break;
        
        case OP_STY_ABS: /* STY Absolute */
            {   
                store_operand_absolute(cpu, cpu->y_lo);
            }
            break;

        /* Transfer between registers --------------------------------- */



        case OP_TAX_IMP: /* TAX Implied */
            {
                op_transfer_to_x(cpu, cpu->a_lo);                
            }
            break;

        case OP_TAY_IMP: /* TAY Implied */
            {
                op_transfer_to_y(cpu, cpu->a_lo);
            }
            break;

        case OP_TYA_IMP: /* TYA Implied */
            {
                op_transfer_to_a(cpu, cpu->y_lo);
            }
            break;

        case OP_TSX_IMP: /* TSX Implied */
            {
                op_transfer_to_x(cpu, cpu->sp);
            }
            break;

        case OP_TXA_IMP: /* TXA Implied */
            {
                op_transfer_to_a(cpu, cpu->x_lo);
            }
            break;

        case OP_TXS_IMP: /* TXS Implied */
            {
                op_transfer_to_s(cpu, cpu->x_lo);
            }
            break;

        /* BRK --------------------------------- */
        case OP_BRK_IMP: /* BRK */
            {               
                push_word(cpu, cpu->pc+1); // pc of BRK plus 1 - leaves room for BRK 'mark'
                push_byte(cpu, cpu->p | FLAG_B | FLAG_UNUSED); // break flag and Unused bit set to 1.
                cpu->p |= FLAG_I; // interrupt disable flag set to 1.
                cpu->pc = cpu->read_word(BRK_VECTOR);
            }
            break;

        /* JMP --------------------------------- */
        case OP_JMP_ABS: /* JMP Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                cpu->pc = addr;
            }
            break;

        case OP_JMP_IND: /* JMP (Indirect) */
            {
                absaddr_t addr = get_operand_address_absolute_indirect(cpu);
                cpu->pc = addr;
            }
            break;

        /* JSR --------------------------------- */
        case OP_JSR_ABS: /* JSR Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                push_word(cpu, cpu->pc -1); // return address pushed is last byte of JSR instruction
                cpu->pc = addr;
                // load address fetched into the PC
                cpu->incr_cycles();
            }
            break;

        /* RTI --------------------------------- */
        case OP_RTI_IMP: /* RTI */
            {
                // pop status register "ignore B | unused" which I think means don't change them.
                byte_t oldp = cpu->p & (FLAG_B | FLAG_UNUSED);
                byte_t p = pop_byte(cpu) & ~(FLAG_B | FLAG_UNUSED);
                cpu->p = p | oldp;

                cpu->pc = pop_word(cpu);
                TRACE(cpu->trace_entry.operand = cpu->pc;)
            }
            break;

        /* RTS --------------------------------- */
        case OP_RTS_IMP: /* RTS */
            {
                cpu->pc = pop_word(cpu);
                cpu->incr_cycles();
                cpu->pc++;
                cpu->incr_cycles();
                TRACE(cpu->trace_entry.operand = cpu->pc;)
            }
            break;

        /* NOP --------------------------------- */
        case OP_NOP_IMP: /* NOP */
            {
                cpu->incr_cycles();
            }
            break;

        /* Flags ---------------------------------  */

        case OP_CLD_IMP: /* CLD Implied */
            {
                cpu->D = 0;
                cpu->incr_cycles();
            }
            break;

        case OP_SED_IMP: /* SED Implied */
            {
                cpu->D = 1;
                cpu->incr_cycles();
            }
            break;

        case OP_CLC_IMP: /* CLC Implied */
            {
                cpu->C = 0;
                cpu->incr_cycles();
            }
            break;

        case OP_CLI_IMP: /* CLI Implied */
            {
                cpu->I = 0;
                cpu->incr_cycles();
            }
            break;

        case OP_CLV_IMP: /* CLV */
            {
                cpu->V = 0;
                cpu->incr_cycles();
            }
            break;

        case OP_SEC_IMP: /* SEC Implied */
            {
                cpu->C = 1;
                cpu->incr_cycles();
            }
            break;

        case OP_SEI_IMP: /* SEI Implied */
            {
                cpu->I = 1;
                cpu->incr_cycles();
            }
            break;

        /** Misc --------------------------------- */

        case OP_BIT_ZP: /* BIT Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                byte_t temp = cpu->a_lo & N;
                set_n_z_v_flags(cpu, temp, N);
            }
            break;

        case OP_BIT_ABS: /* BIT Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                byte_t temp = cpu->a_lo & N;
                set_n_z_v_flags(cpu, temp, N);
            }
            break;

        /* End of Opcodes -------------------------- */

        /* Fake opcodes for testing -------------------------- */

        case OP_HLT_IMP: /* HLT */
            {
                //cpu->halt = HLT_INSTRUCTION;
            }
            break;

        default:
            fprintf(stdout, "Unknown opcode: %04X: 0x%02X", cpu->pc-1, opcode);
            //cpu->halt = HLT_INSTRUCTION;
            break;
    }

    TRACE(if (cpu->trace) cpu->trace_buffer->add_entry(cpu->trace_entry);)

    return 0;
}

}