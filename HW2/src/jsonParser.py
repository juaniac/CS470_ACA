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
    dest, opA, imm = tokens[1], tokens[2], int(tokens[3])
    return ProgramInstruction(iaddr, instr, dest, opA, "", imm)

  elif instr == "ld":
    dest = tokens[1]
    offset, addr = tokens[2].split('(')
    addr = addr.rstrip(')')
    return ProgramInstruction(iaddr, instr, dest, addr, "", int(offset))

  elif instr == "st":
    source = tokens[1]
    offset, addr = tokens[2].split('(')
    addr = addr.rstrip(')')
    return ProgramInstruction(iaddr, instr, "", addr, source, int(offset))

  elif instr == "loop":
    return ProgramInstruction(iaddr, instr, "", "", "", int(tokens[1]))

  elif instr == "mov":
    dest = tokens[1]
    val = tokens[2]
    try:
      imm = int(val, 0)
      return ProgramInstruction(iaddr, instr, dest, "", "", imm)
    except ValueError:
      return ProgramInstruction(iaddr, instr, dest, val, "", 0)

  elif instr == "nop":
    return ProgramInstruction(iaddr, instr, "", "", "", 0)

  else:
    raise ValueError(f"Unsupported instruction: {line}")

def load_program_from_json(json_file_path: str) -> List[ProgramInstruction]:
  with open(json_file_path, 'r') as f:
    lines = json.load(f)

  return [parse_instruction(i, line) for i, line in enumerate(lines)]