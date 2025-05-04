from dataclasses import dataclass
from typing import List
from enum import Enum

@dataclass
class ProgramInstruction:
  iaddr: int
  instr: str
  dest : str
  opA: str
  opB: str
  imm: int
  
INVALID_REGISTER_ID = -1
@dataclass
class Dependency:
  reg: str
  producerId: int 
  producerIdInterloop: int

@dataclass
class DependencyListEntry:
  id: int
  instr : str
  dest: str
  localDeps: List[Dependency]
  interloopDeps: List[Dependency]
  loopInvariantDeps : List[Dependency]
  postLoopDeps: List[Dependency]

@dataclass
class CodeAnalyserOutput:
  nbALUOps: int
  nbMulOps: int
  nbMemOps: int
  nbBrOps: int
  BB0: List[ProgramInstruction]
  BB1: List[ProgramInstruction]
  BB2: List[ProgramInstruction]
  dependencyList: List[DependencyListEntry]

OpType = Enum('OpType', ['ALU', 'MULT', 'MEM', 'BRANCH'])
def get_optype(instr: str) -> OpType:
  if instr in {"add", "addi", "sub", "mov", "nop"}:
    return OpType.ALU
  elif instr in {"mulu"}:
    return OpType.MULT
  elif instr in {"loop"}:
    return OpType.BRANCH
  elif instr in {"ld", "st"}:
    return OpType.MEM
  else:
    raise ValueError(f"Unknown instruction type: {instr}")
