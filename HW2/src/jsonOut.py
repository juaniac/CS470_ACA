import json
from typing import List
from dataStructures import *

def instr_to_str(instr: ProgramInstruction) -> str:
  if not instr:
    return " nop"
    
  if instr.instrType in {"add", "sub", "mulu"}:
    return f" {instr.instrType} {instr.dest}, {instr.opA}, {instr.opB}"
  elif instr.instrType in {"addi"}:
    return f" {instr.instrType} {instr.dest}, {instr.opA}, {instr.imm}"
  elif instr.instrType == "ld":
    return f" {instr.instrType} {instr.dest}, {instr.imm}({instr.opA})"
  elif instr.instrType == "st":
    return f" {instr.instrType} {instr.opB}, {instr.imm}({instr.opA})"
  elif instr.instrType == "loop":
    return f" {instr.instrType} {instr.imm}"
  elif instr.instrType == "mov":
    if instr.opA == "":
      return f" mov {instr.dest}, {instr.imm}"
    else:
      return f" mov {instr.dest}, {instr.opA}"
  else:
    return f" {instr.instrType}"

def schedule_to_json(schedule: List[Bundle], output_filename: str) -> str:
  output = []
  for bundle in schedule:
    row = [
      instr_to_str(bundle.ALU0),
      instr_to_str(bundle.ALU1),
      instr_to_str(bundle.Mult),
      instr_to_str(bundle.Mem),
      instr_to_str(bundle.Branch),
    ]
    output.append(row)
  with open(output_filename, 'w') as f:
    json.dump(output, f, indent=2)
  return
