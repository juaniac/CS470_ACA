import json
from typing import List
from dataStructures import *

def parse_instruction(iaddr: int, line: str) -> ProgramInstruction:
  tokens = line.replace(',', '').split()
  instr = tokens[0]

  if instr in {"add", "sub", "mulu"}:
    dest, opA, opB = tokens[1:4]
    return ProgramInstruction(iaddr, instr, dest, opA, opB, 0)

  elif instr == "addi":
    dest, opA, imm = tokens[1], tokens[2], tokens[3]
    return ProgramInstruction(iaddr, instr, dest, opA, "", int(imm, 0))

  elif instr == "ld":
    dest = tokens[1]
    offset, addr = tokens[2].split('(')
    addr = addr.rstrip(')')
    return ProgramInstruction(iaddr, instr, dest, addr, "", int(offset, 0))

  elif instr == "st":
    source = tokens[1]
    offset, addr = tokens[2].split('(')
    addr = addr.rstrip(')')
    return ProgramInstruction(iaddr, instr, "", source, addr, int(offset, 0))

  elif instr == "loop":
    return ProgramInstruction(iaddr, instr, "", "", "", int(tokens[1], 0))

  elif instr == "mov":
    dest = tokens[1]
    val = tokens[2]
    if val.startswith("0x") or val.lstrip("-").isdigit():
      imm_val = int(val, 0)
      return ProgramInstruction(iaddr, instr, dest, "", "", imm_val)
    else:
      return ProgramInstruction(iaddr, instr, dest, val, "", 0)

  elif instr == "nop":
    return ProgramInstruction(iaddr, instr, "", "", "", 0)

  else:
    raise ValueError(f"Unsupported instruction: {line}")

def load_program_from_json(json_file_path: str) -> List[ProgramInstruction]:
  with open(json_file_path, 'r') as f:
    lines = json.load(f)

  return [parse_instruction(i, line) for i, line in enumerate(lines)]