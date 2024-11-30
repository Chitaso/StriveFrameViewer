#include "bbscript.h"
#include "arcsys.h"
#include <cctype>
#include <cstring>

#include "sigscan.h"

namespace bbscript {
  short *instruction_sizes;

  using get_func_addr_t = char *(*)(char *bbs_file, char *func_name[32], char *table);
  get_func_addr_t get_func_addr;

  using get_action_addr_t = char *(*)(char *obj, char *action_name[32], int *out_index);
  get_action_addr_t get_action_addr;

  using get_operand_value_t = char *(*)(char *obj, char *operand);
  get_operand_value_t get_operand_value;

  void BBSInitializeFunctions() {
    instruction_sizes = reinterpret_cast<short *>(sigscan::get().scan("\x24\x00\x04\x00\x28\x00", "xxxxxx"));
    get_func_addr     = reinterpret_cast<get_func_addr_t>(sigscan::get().scan("\x8D\x3C\x2E\xD1\xFF", "xxxxx") - 0x40);
    get_action_addr   = reinterpret_cast<get_action_addr_t>(sigscan::get().scan("\x0F\x11\x4C\x24\x38\x0F\x85\x00\x00\x00\x00", "xxxxxxx????") - 0x33);
    get_operand_value = reinterpret_cast<get_operand_value_t>(sigscan::get().scan("\x40\x55\x48\x83\xEC\x30\x83\x3A\x00", "xxxxxxxxx"));
  }

  void code_pointer::read_script() {
    state_remaining_time = 0;
    opcode *value        = reinterpret_cast<opcode *>(ptr);
    while (*value != opcode::end_state && *value != opcode::exit_state) {
      bool is_begin = false;
      for (const auto BeginEndPair : BeginEndPairs) {
        if (*reinterpret_cast<int *>(value) == BeginEndPair[0]) {
          get_skip_begin_end_addr();
          value    = reinterpret_cast<opcode *>(ptr);
          is_begin = true;
          break;
        }
      }
      if (is_begin)
        continue;
      execute_instruction(*reinterpret_cast<int *>(value));
      ptr += instruction_sizes[*reinterpret_cast<unsigned int *>(ptr)];
      value = reinterpret_cast<opcode *>(ptr);
      if (nandemo) return;
    }
  }

  void code_pointer::execute_instruction(int code) {
    switch (code) {
    case static_cast<int>(opcode::set_sprite): {
      const int sprite_time = last_sprite_time = *reinterpret_cast<int *>(ptr + 36);
      state_remaining_time += sprite_time;
      break;
    }
    case static_cast<int>(opcode::call_subroutine): {
      char *subroutine_name = ptr + 4;
      if (strstr(subroutine_name, "Nandemo") != nullptr) {
        nandemo = true;
        state_remaining_time -= last_sprite_time;
      }
      break;
    }
    case static_cast<int>(opcode::set_sprite_time): {
      char *operand = ptr + 8;
      if (get_operand_value(owner, operand)) {
        state_remaining_time -= last_sprite_time;
        last_sprite_time = *reinterpret_cast<int *>(ptr + 4);
        state_remaining_time += last_sprite_time;
      }
      break;
    }
    case static_cast<int>(opcode::super_freeze): {
      const int self_freeze = last_sprite_time = *reinterpret_cast<int *>(ptr + 12);
      last_sprite_time -= self_freeze;
      state_remaining_time -= self_freeze;
      break;
    }
    case static_cast<int>(opcode::sprite_time_add): {
      last_sprite_time += *reinterpret_cast<int *>(ptr + 4);
      state_remaining_time += *reinterpret_cast<int *>(ptr + 4);
      break;
    }
    default:
      break;
    }
  }

  void code_pointer::get_skip_begin_end_addr() {
    unsigned int indent = 0;
    do {
      bool begin = true;
      for (const auto BeginEndPair : BeginEndPairs) {
        if (*reinterpret_cast<int *>(ptr) == BeginEndPair[0]) {
          begin = false;
        }
      }
      unsigned int temp_indent = indent + 1;
      if (!begin)
        temp_indent = indent;
      bool end = true;
      for (const auto BeginEndPair : BeginEndPairs) {
        if (*reinterpret_cast<int *>(ptr) == BeginEndPair[1]) {
          end = false;
        }
      }
      const unsigned int opcode_size = instruction_sizes[*reinterpret_cast<unsigned int *>(ptr)];
      indent                         = temp_indent - 1;
      if (!end)
        indent = temp_indent;
      ptr += opcode_size;
    } while (indent);
  }

  char *code_pointer::get_func_addr_base(char *bbs_file, char *func_name) {
    return get_func_addr(bbs_file, &func_name, (char *)*bbs_file + 48);
  }

  char *code_pointer::get_action_addr_base(char *obj, char *action_name, int *out_index) {
    return get_action_addr(obj, &action_name, out_index);
  }
} // bbscript