from dataStructures import *
from typing import List, Dict

def count_instruction_type(program: List[ProgramInstruction], cao: CodeAnalyserOutput) -> None:
  for i in program:
    optype = get_optype(i.instr)
    if optype == OpType.ALU:
      cao.nbALUOps += 1
    elif optype == OpType.MULT:
      cao.nbMulOps += 1
    elif optype == OpType.MEM:
      cao.nbMemOps += 1
    elif optype == OpType.BRANCH:
      cao.nbBrOps += 1
  return

def get_basic_blocks(program: List[ProgramInstruction], cao: CodeAnalyserOutput) -> None:
  if(cao.nbBrOps == 0):
    for i in program:
      cao.BB0.append(i)
    return
  
  loop_from_id = 0
  loop_to_id = 0
  for i in program:
    if(get_optype(i.instr) == OpType.BRANCH):
      loop_from_id = i.iaddr
      loop_to_id = i.imm
  
  for i in range(0, loop_to_id):
    cao.BB0.append(program[i])
  for i in range(loop_to_id, loop_from_id+1):
    cao.BB1.append(program[i])
  for i in range(loop_from_id+1, len(program)):
    cao.BB2.append(program[i])
  return

def extract_used_registers(instr: ProgramInstruction) -> List[str]:
    regs = []
    if instr.opA:
        regs.append(instr.opA)
    if instr.opB:
        regs.append(instr.opB)
    return regs

def analyze_dependencies(cao: CodeAnalyserOutput) -> None:
  
  # Local: reg produced and consumed in the same BB
  prod_bb = []
  for bb in [cao.BB0, cao.BB1, cao.BB2]:
    latest_prod_bb = {}

    for instr in bb:
      entry = cao.dependencyList[instr.iaddr]
      for reg in extract_used_registers(instr):
        if reg in latest_prod_bb:
          entry.localDeps.append(Dependency(reg, latest_prod_bb[reg], INVALID_REGISTER_ID))
      if instr.dest:
        latest_prod_bb[instr.dest] = instr.iaddr
    prod_bb.append(latest_prod_bb)

  # Interloop: reg produced a previous BB1 (and optinnally in BB0) and consumed in a BB1
  latest_prod_bb1 = {}
  for instr in cao.BB1:
    entry = cao.dependencyList[instr.iaddr]
    for reg in extract_used_registers(instr):
      if reg in prod_bb[0] and reg in prod_bb[1] and reg not in latest_prod_bb1:
        entry.interloopDeps.append(Dependency(reg, prod_bb[0][reg], prod_bb[1][reg]))
      elif reg in prod_bb[1] and reg not in latest_prod_bb1:
        entry.interloopDeps.append(Dependency(reg, INVALID_REGISTER_ID, prod_bb[1][reg]))
    if instr.dest:
      latest_prod_bb1[instr.dest] = instr.iaddr

  # Loop invariant: reg only produced BB0 and consumed in BB1
  for instr in cao.BB1:
    entry = cao.dependencyList[instr.iaddr]
    for reg in extract_used_registers(instr):
      if reg in prod_bb[0] and reg not in prod_bb[1]:
        entry.loopInvariantDeps.append(Dependency(reg, prod_bb[0][reg], INVALID_REGISTER_ID))

  # Post-loop: used in BB2, defined in BB1
  for instr in cao.BB2:
    entry = cao.dependencyList[instr.iaddr]
    for reg in extract_used_registers(instr):
      if reg in prod_bb[1]:
        entry.postLoopDeps.append(Dependency(reg, prod_bb[1][reg], INVALID_REGISTER_ID))
  return

def print_dependency_table(output: CodeAnalyserOutput) -> None:
  print("=" * 80)
  print(f"{'ID':<5} {'Instruction':<20} {'Dest':<6} {'Local':<15} {'Interloop':<15} {'Invariant':<15} {'PostLoop':<15}")
  print("=" * 80)

  for entry in output.dependencyList:
    local = ', '.join([f"{d.reg}(#{d.producerId})" for d in entry.localDeps]) or "-"
    interloop = ', '.join([f"{d.reg}(#{d.producerIdInterloop})" if d.producerId == INVALID_REGISTER_ID else f"{d.reg}(#{d.producerId} or #{d.producerIdInterloop}')" for d in entry.interloopDeps]) or "-"
    invariant = ', '.join([f"{d.reg}(#{d.producerId})" for d in entry.loopInvariantDeps]) or "-"
    postloop = ', '.join([f"{d.reg}(#{d.producerId})" for d in entry.postLoopDeps]) or "-"

    print(f"{entry.id:<5} {entry.instr:<20} {entry.dest:<6} {local:<15} {interloop:<15} {invariant:<15} {postloop:<15}")
    
  print("=" * 80)

def analyse_code(program: List[ProgramInstruction]) -> CodeAnalyserOutput:
  cao = CodeAnalyserOutput(
          0, 0, 0, 0, [], [], [], 
          [DependencyListEntry(
            id=i.iaddr,
            instr=i.instr,
            dest=i.dest,
            localDeps=[],
            interloopDeps=[],
            loopInvariantDeps=[],
            postLoopDeps=[]
          ) for i in program]
        )
  count_instruction_type(program, cao)
  get_basic_blocks(program, cao)
  analyze_dependencies(cao)
  print_dependency_table(cao)

  return cao