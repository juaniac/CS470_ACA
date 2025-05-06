from dataclasses import dataclass
from typing import List,  Dict
from enum import Enum

@dataclass
class ProgramInstruction:
  iaddr: int
  instrType: str
  dest : str
  opA: str
  opB: str
  imm: str

@dataclass
class Dependency:
  reg: str
  producerId: int 
@dataclass
class InterloopDependency:
  reg: str
  producerId: int 
  producerIdInterloop: int

@dataclass
class DependencyListEntry:
  id: int
  instrType: str
  dest: str
  localDeps: List[Dependency]
  interloopDeps: List[InterloopDependency]
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
def get_optype(instrType: str) -> OpType:
  if instrType in {"add", "addi", "sub", "mov", "nop"}:
    return OpType.ALU
  elif instrType in {"mulu"}:
    return OpType.MULT
  elif instrType in {"loop"}:
    return OpType.BRANCH
  elif instrType in {"ld", "st"}:
    return OpType.MEM
  else:
    raise ValueError(f"Unknown instruction type: {instrType}")

def get_latency(instrType: str) -> int:
  return 3 if get_optype(instrType) == OpType.MULT else 1

def extract_used_registers(instr: ProgramInstruction) -> List[str]:
    regs = []
    if instr.opA:
        regs.append(instr.opA)
    if instr.opB:
        regs.append(instr.opB)
    return regs

@dataclass
class TimesIntervale:
  start: int
  finish: int

# == None if a nop is placed
@dataclass
class Bundle:
  ALU0: ProgramInstruction
  ALU1: ProgramInstruction
  Mult: ProgramInstruction
  Mem: ProgramInstruction
  Branch: ProgramInstruction

  def __str__(self) -> str:
    parts = []
    if self.ALU0:   parts.append(f"  ALU0:   {self.ALU0}")
    if self.ALU1:   parts.append(f"  ALU1:   {self.ALU1}")
    if self.Mult:   parts.append(f"  Mult:   {self.Mult}")
    if self.Mem:    parts.append(f"  Mem:    {self.Mem}")
    if self.Branch: parts.append(f"  Branch: {self.Branch}")
    return "\n".join(parts) if parts else "  (NOPs only)"
  
@dataclass
class SimpleSchedulerOutput:
  schedule: List[Bundle]
  scheduled_times: Dict[int, TimesIntervale]
