// disasm.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 25.11.25.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include "disasm.h"

#include <vespera/mm/memory.h>
#include "symbols.h"
#include <klib/string.h>
#include <vespera/log.h>

struct OpcodeEntry {
    u8 opcode;
    const char* mnemonic;
    u8 size;
    bool needs_modrm;
    bool is_prefix;
};

static const char* reg64[] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};
static const char* reg32[] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
};
static const char* reg16[] = {
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di", "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
};
static const char* reg8[] = {
    "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil", "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
};

struct RexPrefix {
    bool present;
    bool w;  // 64-bit operand
    bool r;  // Extension of ModR/M reg field
    bool x;  // Extension of SIB index field
    bool b;  // Extension of ModR/M r/m field
};

struct ModRm {
    u8 mod;
    u8 reg;
    u8 rm;
};

ModRm decode_modrm(const u8 byte) {
    ModRm m{};
    m.mod = (byte >> 6) & 0x3;
    m.reg = (byte >> 3) & 0x7;
    m.rm = byte & 0x7;
    return m;
}

const char* get_reg_name(const u8 reg_idx, const int operand_size) {
    switch (operand_size) {
        case 1:
            return reg8[reg_idx & 15];
        case 2:
            return reg16[reg_idx & 15];
        case 4:
            return reg32[reg_idx & 15];
        case 8:
            return reg64[reg_idx & 15];
        default:
            return "???";
    }
}

// Returns extra bytes consumed AFTER the ModR/M byte (SIB, displacement, etc.)
usize decode_rm_operand(
    const u8* code, usize const offset, usize const max_len, ModRm const modrm, const RexPrefix rex, const int operand_size,
    u64 const instr_addr, usize const instr_len, char* output, const usize output_size, const bool show_size_prefix = true
) {
    const u8 rm = modrm.rm | (rex.b ? 8 : 0);

    // Register direct
    if (modrm.mod == 3) {
        snprintf(output, output_size, "%s", get_reg_name(rm, operand_size));
        return 0;
    }

    // Memory operand
    auto size_prefix = "";
    if (show_size_prefix) {
        if (operand_size == 1)
            size_prefix = "byte ptr ";
        else if (operand_size == 2)
            size_prefix = "word ptr ";
        else if (operand_size == 4)
            size_prefix = "dword ptr ";
        else if (operand_size == 8)
            size_prefix = "qword ptr ";
    }

    auto fmt_disp = [](char* buf, const usize buf_size, const i64 disp) {
        if (disp > 0)
            snprintf(buf, buf_size, "+0x%llx", static_cast<u64>(disp));
        else if (disp < 0)
            snprintf(buf, buf_size, "-0x%llx", static_cast<u64>(-disp));
        else
            buf[0] = '\0';
    };

    // mod = 0: no displacement (except special cases)
    if (modrm.mod == 0) {
        // RIP-relative: mod=0, rm=5
        if (modrm.rm == 5) {
            if (offset + 4 > max_len) {
                snprintf(output, output_size, "%s[rip+?]", size_prefix);
                return 0;
            }

            i32 disp = 0;
            memcpy(&disp, code + offset + 1, sizeof(i32));

            const u64 rip = instr_addr + instr_len;
            const u64 target = rip + disp;

            char disp_str[32];
            fmt_disp(disp_str, sizeof(disp_str), disp);

            auto [name, len, addr] = lookup_symbol(target);
            const u64 l_offset = target - addr;

            snprintf(
                output,
                output_size,
                "%s[rip%s] -> 0x%llx <%.*s+0x%llx>",
                size_prefix,
                disp_str,
                target,
                static_cast<int>(len),
                name,
                l_offset
            );
            return 4;
        }

        if (modrm.rm == 4) {
            if (offset >= max_len) {
                snprintf(output, output_size, "%s[?]", size_prefix);
                return 0;
            }

            const u8 sib = code[offset];
            const u8 scale = 1 << ((sib >> 6) & 3);
            const u8 index = ((sib >> 3) & 7) | (rex.x ? 8 : 0);
            const u8 base = (sib & 7) | (rex.b ? 8 : 0);

            i32 disp = 0;
            char disp_str[32] = "";
            if ((sib & 7) == 5) {
                memcpy(&disp, code + offset + 1, sizeof(i32));
                fmt_disp(disp_str, sizeof(disp_str), disp);
                if (index == 4)
                    snprintf(output, output_size, "%s[%s]", size_prefix, disp_str);
                else
                    snprintf(output, output_size, "%s[%s*%d%s]", size_prefix, get_reg_name(index, 8), scale, disp_str);
                return 5;  // 1 SIB + 4 disp
            }

            //  SIB without disp32
            if (index == 4)
                snprintf(output, output_size, "%s[%s]", size_prefix, get_reg_name(base, 8));
            else
                snprintf(
                    output,
                    output_size,
                    "%s[%s + %s*%d]",
                    size_prefix,
                    get_reg_name(base, 8),
                    get_reg_name(index, 8),
                    scale
                );

            return 1;  // only SIB Byte
        }

        // simple [reg]
        snprintf(output, output_size, "%s[%s]", size_prefix, get_reg_name(rm, 8));
        return 0;
    }

    // mod=1 or mod=2: [reg + disp8/32]
    if (modrm.mod == 1 || modrm.mod == 2) {
        const int disp_bytes = (modrm.mod == 1) ? 1 : 4;
        if (offset + 1 + disp_bytes > max_len) {
            // +1 wegen SIB
            snprintf(output, output_size, "%s[?]", size_prefix);
            return 0;
        }

        if (modrm.rm == 4) {
            const u8 sib = code[offset];
            const u8 scale = 1 << ((sib >> 6) & 3);
            const u8 index = ((sib >> 3) & 7) | (rex.x ? 8 : 0);
            const u8 base = (sib & 7) | (rex.b ? 8 : 0);

            i64 disp = 0;
            if (disp_bytes == 1) {
                i8 raw;
                memcpy(&raw, code + offset + 1, sizeof(raw));
                disp = static_cast<u8>(raw);
            } else {
                i32 raw;
                memcpy(&raw, code + offset + 1, sizeof(raw));
                disp = static_cast<i64>(raw);
            }

            char disp_str[32] = "";
            fmt_disp(disp_str, sizeof(disp_str), disp);

            if (index == 4)
                snprintf(output, output_size, "%s[%s%s]", size_prefix, get_reg_name(base, 8), disp_str);
            else
                snprintf(
                    output,
                    output_size,
                    "%s[%s + %s*%d%s]",
                    size_prefix,
                    get_reg_name(base, 8),
                    get_reg_name(index, 8),
                    scale,
                    disp_str
                );

            return 1 + disp_bytes;  // SIB + displacement
        }

        // simple reg + disp
        i64 disp = 0;
        if (disp_bytes == 1) {
            i8 raw;
            memcpy(&raw, code + offset + 1, sizeof(raw));
            disp = static_cast<u8>(raw);
        } else {
            i32 raw;
            memcpy(&raw, code + offset + 1, sizeof(raw));
            disp = static_cast<i64>(raw);
        }

        char disp_str[32] = "";
        fmt_disp(disp_str, sizeof(disp_str), disp);

        snprintf(output, output_size, "%s[%s%s]", size_prefix, get_reg_name(rm, 8), disp_str);
        return disp_bytes;
    }

    // Falls alles fehlschlägt
    snprintf(output, output_size, "???");
    return 0;
}

const char* get_group_mnemonic(const u8 opcode, const u8 reg_field) {
    if (opcode == 0xFE) {
        return (reg_field == 0) ? "inc" : (reg_field == 1) ? "dec" : "???";
    }
    if (opcode == 0xFF) {
        const char* grp5[] = {"inc", "dec", "call", "callf", "jmp", "jmpf", "push", "???"};
        return (reg_field <= 6) ? grp5[reg_field] : grp5[7];
    }
    if (opcode == 0xF6 || opcode == 0xF7) {
        const char* grp3[] = {"test", "test", "not", "neg", "mul", "imul", "div", "idiv"};
        return grp3[reg_field & 7];
    }
    if (opcode == 0x80 || opcode == 0x81 || opcode == 0x83) {
        const char* grp1[] = {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"};
        return grp1[reg_field & 7];
    }
    return "???";
}

Instruction disasm_next(const u8* code, usize max_len, u64 instr_addr) {
    Instruction result = {"invalid", 0};
    if (max_len == 0) return result;

    usize offset = 0;
    RexPrefix rex = {false, false, false, false, false};

    // Parse REX prefix (0x40-0x4F)
    if (code[offset] >= 0x40 && code[offset] <= 0x4F) {
        rex.present = true;
        rex.w = (code[offset] & 0x8) != 0;
        rex.r = (code[offset] & 0x4) != 0;
        rex.x = (code[offset] & 0x2) != 0;
        rex.b = (code[offset] & 0x1) != 0;
        offset++;
        if (offset >= max_len) {
            strcpy(result.mnemonic, "rex (incomplete)");
            result.size = 1;
            return result;
        }
    }

    // Check for two-byte opcode (0x0F prefix)
    bool is_two_byte = false;
    if (code[offset] == 0x0F) {
        is_two_byte = true;
        offset++;
        if (offset >= max_len) {
            strcpy(result.mnemonic, "0x0F (incomplete)");
            result.size = offset;
            return result;
        }
    }

    u8 opcode = code[offset];
    offset++;

    int default_size = rex.w ? 8 : 4;

    // ===== TWO-BYTE OPCODES =====
    if (is_two_byte) {
        if (opcode == 0x0B) {
            strcpy(result.mnemonic, "ud2");
            result.size = offset;
            return result;
        }

        // MOVZX / MOVSX: 0x0F 0xB6/B7/BE/BF
        if (opcode == 0xB6 || opcode == 0xB7 || opcode == 0xBE || opcode == 0xBF) {
            if (offset >= max_len) {
                snprintf(result.mnemonic, sizeof(result.mnemonic), "movzx/movsx (no modrm, opcode=0x%02x)", opcode);
                result.size = offset;
                return result;
            }

            ModRm modrm = decode_modrm(code[offset]);
            offset++;  // Consume ModR/M

            u8 reg = modrm.reg | (rex.r ? 8 : 0);
            int src_size = (opcode == 0xB6 || opcode == 0xBE) ? 1 : 2;
            int dst_size = rex.w ? 8 : 4;

            char rm_str[128];
            usize extra = decode_rm_operand(
                code,
                offset,
                max_len,
                modrm,
                rex,
                src_size,
                instr_addr,
                0,  // instr_len unknown yet → replaced later
                rm_str,
                sizeof(rm_str),
                true
            );
            offset += extra;

            // Now we know the full instr length → compute new rm_str including target
            // addr:
            decode_rm_operand(
                code,
                offset - extra,
                max_len,
                modrm,
                rex,
                src_size,
                instr_addr,
                offset,  // re-run with correct length!
                rm_str,
                sizeof(rm_str),
                true
            );

            snprintf(
                result.mnemonic,
                sizeof(result.mnemonic),
                "%s %s, %s",
                (opcode == 0xB6 || opcode == 0xB7) ? "movzx" : "movsx",
                get_reg_name(reg, dst_size),
                rm_str
            );

            result.size = offset;
            return result;
        }

        // Conditional jumps: 0x0F 0x80-0x8F
        if (opcode >= 0x80 && opcode <= 0x8F) {
            if (offset + 4 > max_len) {
                strcpy(result.mnemonic, "jcc (incomplete)");
                result.size = offset;
                return result;
            }
            i32 rel = 0;
            memcpy(&rel, code + offset, sizeof(rel));

            offset += 4;
            const char* jcc[] = {
                "jo", "jno", "jb", "jae", "je", "jne", "jbe", "ja", "js", "jns", "jp", "jnp", "jl", "jge", "jle", "jg"
            };
            snprintf(result.mnemonic, sizeof(result.mnemonic), "%s 0x%x", jcc[opcode & 0xF], rel);
            result.size = offset;
            return result;
        }

        // SETcc: 0x0F 0x90-0x9F
        if (opcode >= 0x90 && opcode <= 0x9F) {
            if (offset >= max_len) {
                strcpy(result.mnemonic, "setcc (incomplete)");
                result.size = offset;
                return result;
            }
            ModRm modrm = decode_modrm(code[offset]);
            offset++;

            const char* setcc[] = {
                "seto",
                "setno",
                "setb",
                "setae",
                "sete",
                "setne",
                "setbe",
                "seta",
                "sets",
                "setns",
                "setp",
                "setnp",
                "setl",
                "setge",
                "setle",
                "setg"
            };

            char rm_str[128];
            usize extra_bytes =
                decode_rm_operand(code, offset, max_len, modrm, rex, 1, instr_addr, 0, rm_str, sizeof(rm_str));
            offset += extra_bytes;

            snprintf(result.mnemonic, sizeof(result.mnemonic), "%s %s", setcc[opcode & 0xF], rm_str);
            result.size = offset;
            return result;
        }

        // IMUL r, r/m: 0x0F 0xAF
        if (opcode == 0xAF) {
            if (offset >= max_len) {
                strcpy(result.mnemonic, "imul (incomplete)");
                result.size = offset;
                return result;
            }
            ModRm modrm = decode_modrm(code[offset]);
            offset++;

            u8 reg = modrm.reg | (rex.r ? 8 : 0);
            char rm_str[128];
            usize extra_bytes = decode_rm_operand(
                code, offset, max_len, modrm, rex, default_size, instr_addr, 0, rm_str, sizeof(rm_str)
            );
            offset += extra_bytes;

            snprintf(result.mnemonic, sizeof(result.mnemonic), "imul %s, %s", get_reg_name(reg, default_size), rm_str);
            result.size = offset;
            return result;
        }

        // System instructions
        if (opcode == 0xA2) {
            strcpy(result.mnemonic, "cpuid");
            result.size = offset;
            return result;
        }
        if (opcode == 0x31) {
            strcpy(result.mnemonic, "rdtsc");
            result.size = offset;
            return result;
        }
        if (opcode == 0x05) {
            strcpy(result.mnemonic, "syscall");
            result.size = offset;
            return result;
        }

        snprintf(result.mnemonic, sizeof(result.mnemonic), "unknown 0x0F 0x%02x", opcode);
        result.size = offset;
        return result;
    }

    // ===== SINGLE-BYTE OPCODES =====

    // PUSH/POP register: 0x50-0x5F
    if (opcode >= 0x50 && opcode <= 0x5F) {
        u8 reg = (opcode & 0x7) | (rex.b ? 8 : 0);
        const char* mnemonic = (opcode < 0x58) ? "push" : "pop";
        snprintf(result.mnemonic, sizeof(result.mnemonic), "%s %s", mnemonic, get_reg_name(reg, 8));
        result.size = offset;
        return result;
    }

    // MOV immediate to register: 0xB0-0xBF
    if (opcode >= 0xB0 && opcode <= 0xBF) {
        u8 reg = (opcode & 0x7) | (rex.b ? 8 : 0);

        if ((opcode < 0xB8))  // is 8 bit?
        {
            if (offset < max_len) {
                u8 imm = code[offset++];
                snprintf(result.mnemonic, sizeof(result.mnemonic), "mov %s, 0x%x", get_reg_name(reg, 1), imm);
            } else {
                strcpy(result.mnemonic, "mov (incomplete)");
            }
        } else {
            if (rex.w) {
                if (offset + 8 <= max_len) {
                    u64 imm = 0;
                    memcpy(&imm, code + offset, sizeof(imm));
                    offset += 8;
                    snprintf(result.mnemonic, sizeof(result.mnemonic), "mov %s, 0x%lx", get_reg_name(reg, 8), imm);
                } else {
                    strcpy(result.mnemonic, "mov (incomplete)");
                }
            } else {
                if (offset + 4 <= max_len) {
                    u32 imm = 0;
                    memcpy(&imm, code + offset, sizeof(imm));
                    offset += 4;
                    snprintf(result.mnemonic, sizeof(result.mnemonic), "mov %s, 0x%x", get_reg_name(reg, 4), imm);
                } else {
                    strcpy(result.mnemonic, "mov (incomplete)");
                }
            }
        }
        result.size = offset;
        return result;
    }

    // RET: 0xC3
    if (opcode == 0xC3) {
        strcpy(result.mnemonic, "ret");
        result.size = offset;
        return result;
    }

    // CALL/JMP near: 0xE8/0xE9
    if (opcode == 0xE8 || opcode == 0xE9) {
        if (offset + 4 <= max_len) {
            i32 rel = code[offset] | (code[offset + 1] << 8) | (code[offset + 2] << 16) | (code[offset + 3] << 24);
            offset += 4;
            u64 target = instr_addr + 1 + 4 + rel;

            auto [name, len, addr] = lookup_symbol(target);

            if (u64 l_offset = target - addr; l_offset == 0) {
                snprintf(
                    result.mnemonic,
                    sizeof(result.mnemonic),
                    "%s 0x%llx -> <%.*s>",
                    (opcode == 0xE8) ? "call" : "jmp",
                    target,
                    static_cast<int>(len),
                    name
                );
            } else {
                snprintf(
                    result.mnemonic,
                    sizeof(result.mnemonic),
                    "%s 0x%llx -> <%.*s+0x%llx>",
                    (opcode == 0xE8) ? "call" : "jmp",
                    target,
                    static_cast<int>(len),
                    name,
                    l_offset
                );
            }
        } else {
            strcpy(result.mnemonic, "call/jmp (incomplete)");
        }
        result.size = 1 + 4;
        result.size = offset;
        return result;
    }

    // Short  /Jcc: 0xEB, 0x70-0x7F
    if (opcode == 0xEB || (opcode >= 0x70 && opcode <= 0x7F)) {
        if (offset < max_len) {
            i8 rel = 0;
            memcpy(&rel, code + offset++, sizeof(rel));
            u64 target = instr_addr + offset + rel;  // absolute Adresse berechnen

            auto [name, len, addr] = lookup_symbol(target);

            if (u64 l_offset = target - addr; l_offset == 0) {
                snprintf(
                    result.mnemonic,
                    sizeof(result.mnemonic),
                    "%s 0x%llx -> <%.*s>",
                    (opcode == 0xE8) ? "call" : "jmp",
                    target,
                    static_cast<int>(len),
                    name
                );
            } else {
                snprintf(
                    result.mnemonic,
                    sizeof(result.mnemonic),
                    "%s 0x%llx -> <%.*s+0x%llx>",
                    (opcode == 0xE8) ? "call" : "jmp",
                    target,
                    static_cast<int>(len),
                    name,
                    l_offset
                );
            }
        } else {
            strcpy(result.mnemonic, "jmp/jcc (incomplete)");
        }

        result.size = offset;
        return result;
    }

    // Instructions with ModR/M
    bool needs_modrm = false;
    const char* base_mnemonic = nullptr;
    int operand_size = default_size;
    bool is_group = false;

    switch (opcode) {
        case 0x88:
            base_mnemonic = "mov";
            needs_modrm = true;
            operand_size = 1;
            break;
        case 0x89:
            base_mnemonic = "mov";
            needs_modrm = true;
            break;
        case 0x8A:
            base_mnemonic = "mov";
            needs_modrm = true;
            operand_size = 1;
            break;
        case 0x8B:
            base_mnemonic = "mov";
            needs_modrm = true;
            break;
        case 0x8D:
            base_mnemonic = "lea";
            needs_modrm = true;
            break;
        case 0x03:
        case 0x01:
            base_mnemonic = "add";
            needs_modrm = true;
            break;
        case 0x2B:
        case 0x29:
            base_mnemonic = "sub";
            needs_modrm = true;
            break;

        case 0x23:
        case 0x21:
            base_mnemonic = "and";
            needs_modrm = true;
            break;

        case 0x09:
        case 0x0B:
            base_mnemonic = "or";
            needs_modrm = true;
            break;
        case 0x33:
        case 0x31:
            base_mnemonic = "xor";
            needs_modrm = true;
            break;
        case 0x3B:
        case 0x39:
            base_mnemonic = "cmp";
            needs_modrm = true;
            break;
        case 0x85:
            base_mnemonic = "test";
            needs_modrm = true;
            break;
        case 0xC0:
        case 0xC1:
            needs_modrm = true;
            is_group = true;
            operand_size = (opcode == 0xC0) ? 1 : default_size;  // C0 = 8-bit, C1 = 16/32/64-bit
            break;
        case 0xFE:
        case 0xFF:
        case 0xF6:
        case 0xF7:
        case 0x80:
        case 0x81:
        case 0x83:
        case 0xC6:
        case 0xC7:
            needs_modrm = true;
            is_group = true;
            operand_size = (opcode == 0xFE || opcode == 0x80 || opcode == 0xF6 || opcode == 0xC6) ? 1 : default_size;
            break;
        default:
            break;
    }

    if (needs_modrm) {
        bool single_operand = false;
        if (offset >= max_len) {
            snprintf(result.mnemonic, sizeof(result.mnemonic), "incomplete (need modrm)");
            result.size = offset;
            return result;
        }

        ModRm modrm = decode_modrm(code[offset]);
        offset++;  // Consume ModR/M byte

        if (is_group && (opcode == 0xC0 || opcode == 0xC1)) {
            const char* shift_ops[8] = {"rol", "ror", "rcl", "rcr", "shl", "shr", "sal", "sar"};
            const char* mnemonic = shift_ops[modrm.reg & 0x7];

            char rm_str[128];
            usize extra_bytes = decode_rm_operand(
                code, offset, max_len, modrm, rex, operand_size, instr_addr, 0, rm_str, sizeof(rm_str), true
            );
            offset += extra_bytes;

            // Immediate-Byte folgt
            if (offset < max_len) {
                u8 imm = code[offset++];
                snprintf(result.mnemonic, sizeof(result.mnemonic), "%s %s, 0x%x", mnemonic, rm_str, imm);
            } else {
                snprintf(result.mnemonic, sizeof(result.mnemonic), "%s %s, ?", mnemonic, rm_str);
            }

            result.size = offset;
            return result;
        }

        if (opcode == 0xC6) {
            if (modrm.reg != 0) {
                strcpy(result.mnemonic, "invalid movb / unknown");
                result.size = offset;
                return result;
            }

            char rm_str[128];
            usize extra_bytes =
                decode_rm_operand(code, offset, max_len, modrm, rex, 1, instr_addr, 0, rm_str, sizeof(rm_str), true);
            offset += extra_bytes;

            if (offset >= max_len) {
                strcpy(result.mnemonic, "movb (incomplete)");
                result.size = offset;
                return result;
            }

            u8 imm = code[offset++];
            snprintf(result.mnemonic, sizeof(result.mnemonic), "movb %s, 0x%x", rm_str, imm);
            result.size = offset;
            return result;
        }

        u8 reg = modrm.reg | (rex.r ? 8 : 0);

        if (is_group) {
            base_mnemonic = get_group_mnemonic(opcode, modrm.reg);
            single_operand = (opcode == 0xFE || opcode == 0xFF || opcode == 0xF6 || opcode == 0xF7) && modrm.reg >= 2;
        }

        char rm_str[128];
        usize extra_bytes = decode_rm_operand(
            code, offset, max_len, modrm, rex, operand_size, instr_addr, 0, rm_str, sizeof(rm_str), true
        );
        offset += extra_bytes;

        if (single_operand) {
            snprintf(result.mnemonic, sizeof(result.mnemonic), "%s %s", base_mnemonic, rm_str);
        } else {
            snprintf(
                result.mnemonic,
                sizeof(result.mnemonic),
                "%s %s, %s",
                base_mnemonic,
                get_reg_name(reg, operand_size),
                rm_str
            );
        }

        // Handle immediates for group opcodes
        if ((opcode == 0x80 || opcode == 0x83 || opcode == 0xC6) && offset < max_len) {
            u8 imm = code[offset++];
            snprintf(
                result.mnemonic + strlen(result.mnemonic),
                sizeof(result.mnemonic) - strlen(result.mnemonic),
                ", 0x%x",
                imm
            );
        } else if ((opcode == 0x81 || opcode == 0xC7) && offset + 4 <= max_len) {
            u32 imm = 0;
            memcpy(&imm, code + offset, sizeof(imm));
            offset += 4;
            snprintf(
                result.mnemonic + strlen(result.mnemonic),
                sizeof(result.mnemonic) - strlen(result.mnemonic),
                ", 0x%x",
                imm
            );
        }

        result.size = offset;
        return result;
    }

    // Simple opcodes
    switch (opcode) {
        case 0x90:
            strcpy(result.mnemonic, "nop");
            break;

        // Interrupts und Traps
        case 0xCC:
            strcpy(result.mnemonic, "int3");
            break;
        case 0xCD:
            if (offset < max_len) {
                u8 int_num = code[offset++];
                snprintf(result.mnemonic, sizeof(result.mnemonic), "int 0x%x", int_num);
            } else {
                strcpy(result.mnemonic, "int (incomplete)");
            }
            break;
        case 0xCE:
            strcpy(result.mnemonic, "into");
            break;
        case 0xCF:
            strcpy(result.mnemonic, "iret");
            break;

        // System Control
        case 0xF4:
            strcpy(result.mnemonic, "hlt");
            break;
        case 0xFA:
            strcpy(result.mnemonic, "cli");
            break;
        case 0xFB:
            strcpy(result.mnemonic, "sti");
            break;
        case 0xF5:
            strcpy(result.mnemonic, "cmc");
            break;
        case 0xF8:
            strcpy(result.mnemonic, "clc");
            break;
        case 0xF9:
            strcpy(result.mnemonic, "stc");
            break;
        case 0xFC:
            strcpy(result.mnemonic, "cld");
            break;
        case 0xFD:
            strcpy(result.mnemonic, "std");
            break;

        // Conversion Instructions
        case 0x98:
            if (rex.w)
                strcpy(result.mnemonic, "cltq");  // RAX ← sign-extend EAX (alias: cdqe)
            else
                strcpy(result.mnemonic, "cwde");  // EAX ← sign-extend AX (oder cbtw)
            break;
        case 0x99:
            if (rex.w)
                strcpy(result.mnemonic, "cqto");  // RDX:RAX ← sign-extend RAX (alias: cqo)
            else
                strcpy(result.mnemonic, "cltd");  // EDX:EAX ← sign-extend EAX (alias: cdq)
            break;

        // Stack Operations
        case 0x9C:
            if (rex.w)
                strcpy(result.mnemonic, "pushfq");
            else
                strcpy(result.mnemonic, "pushf");
            break;
        case 0x9D:
            if (rex.w)
                strcpy(result.mnemonic, "popfq");
            else
                strcpy(result.mnemonic, "popf");
            break;
        case 0x60:
            strcpy(result.mnemonic, "pusha");
            break;  // nur 32-bit
        case 0x61:
            strcpy(result.mnemonic, "popa");
            break;  // nur 32-bit

        // String Operations
        case 0xA4:
            strcpy(result.mnemonic, "movsb");
            break;
        case 0xA5:
            if (rex.w)
                strcpy(result.mnemonic, "movsq");
            else
                strcpy(result.mnemonic, "movsd");
            break;
        case 0xA6:
            strcpy(result.mnemonic, "cmpsb");
            break;
        case 0xA7:
            if (rex.w)
                strcpy(result.mnemonic, "cmpsq");
            else
                strcpy(result.mnemonic, "cmpsd");
            break;
        case 0xAA:
            strcpy(result.mnemonic, "stosb");
            break;
        case 0xAB:
            if (rex.w)
                strcpy(result.mnemonic, "stosq");
            else
                strcpy(result.mnemonic, "stosd");
            break;
        case 0xAC:
            strcpy(result.mnemonic, "lodsb");
            break;
        case 0xAD:
            if (rex.w)
                strcpy(result.mnemonic, "lodsq");
            else
                strcpy(result.mnemonic, "lodsd");
            break;
        case 0xAE:
            strcpy(result.mnemonic, "scasb");
            break;
        case 0xAF:
            if (rex.w)
                strcpy(result.mnemonic, "scasq");
            else
                strcpy(result.mnemonic, "scasd");
            break;

        // I/O Instructions
        case 0xE4:
            if (offset < max_len) {
                u8 port = code[offset++];
                snprintf(result.mnemonic, sizeof(result.mnemonic), "in al, 0x%x", port);
            } else {
                strcpy(result.mnemonic, "in (incomplete)");
            }
            break;
        case 0xE5:
            if (offset < max_len) {
                u8 port = code[offset++];
                snprintf(result.mnemonic, sizeof(result.mnemonic), "in eax, 0x%x", port);
            } else {
                strcpy(result.mnemonic, "in (incomplete)");
            }
            break;
        case 0xE6:
            if (offset < max_len) {
                u8 port = code[offset++];
                snprintf(result.mnemonic, sizeof(result.mnemonic), "out 0x%x, al", port);
            } else {
                strcpy(result.mnemonic, "out (incomplete)");
            }
            break;
        case 0xE7:
            if (offset < max_len) {
                u8 port = code[offset++];
                snprintf(result.mnemonic, sizeof(result.mnemonic), "out 0x%x, eax", port);
            } else {
                strcpy(result.mnemonic, "out (incomplete)");
            }
            break;
        case 0xEC:
            strcpy(result.mnemonic, "in al, dx");
            break;
        case 0xED:
            strcpy(result.mnemonic, "in eax, dx");
            break;
        case 0xEE:
            strcpy(result.mnemonic, "out dx, al");
            break;
        case 0xEF:
            strcpy(result.mnemonic, "out dx, eax");
            break;

        // Arithmetic
        case 0x27:
            strcpy(result.mnemonic, "daa");
            break;
        case 0x2F:
            strcpy(result.mnemonic, "das");
            break;
        case 0x37:
            strcpy(result.mnemonic, "aaa");
            break;
        case 0x3F:
            strcpy(result.mnemonic, "aas");
            break;
        case 0xD4:
            if (offset < max_len) {
                u8 base = code[offset++];
                snprintf(result.mnemonic, sizeof(result.mnemonic), "aam 0x%x", base);
            } else {
                strcpy(result.mnemonic, "aam");
            }
            break;
        case 0xD5:
            if (offset < max_len) {
                u8 base = code[offset++];
                snprintf(result.mnemonic, sizeof(result.mnemonic), "aad 0x%x", base);
            } else {
                strcpy(result.mnemonic, "aad");
            }
            break;

        // Misc
        case 0x9B:
            strcpy(result.mnemonic, "wait");
            break;
        case 0x9E:
            strcpy(result.mnemonic, "sahf");
            break;
        case 0x9F:
            strcpy(result.mnemonic, "lahf");
            break;
        case 0xC9:
            strcpy(result.mnemonic, "leave");
            break;
        case 0xD6:
            strcpy(result.mnemonic, "salc");
            break;  // undokumentiert
        case 0xD7:
            strcpy(result.mnemonic, "xlat");
            break;
        case 0xF1:
            strcpy(result.mnemonic, "int1");
            break;  // undokumentiert

        // Repeat Prefixes
        case 0xF2:
            strcpy(result.mnemonic, "repne");
            break;
        case 0xF3:
            strcpy(result.mnemonic, "rep/repe");
            break;

        default:
            snprintf(result.mnemonic, sizeof(result.mnemonic), "unknown 0x%02x", opcode);
            break;
    }

    result.size = offset;
    return result;
}

void disassemble_frame(const u64 addr, const usize bytes) {
    const auto* ptr = reinterpret_cast<const u8*>(addr);
    usize offset = 0;

    while (offset < bytes) {
        const u64 instr_addr = addr + offset;
        auto [mnemonic, size] = disasm_next(ptr + offset, bytes, instr_addr);
        Log::print_ln("    %p: %s", reinterpret_cast<void*>(instr_addr), mnemonic);
        offset += size;
    }
}
