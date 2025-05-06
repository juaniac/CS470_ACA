from typing import List, Dict, Union
from dataStructures import *
import copy

def print_schedule(schedule: List[Bundle]) -> None:
  print("\n Final Instruction Schedule:\n")
  for i, bundle in enumerate(schedule):
      if any([bundle.ALU0, bundle.ALU1, bundle.Mult, bundle.Mem, bundle.Branch]):
        print(f"Cycle {i:>2}:")
        print(f"   {bundle}")

def check_to_add_empty_bundles(schedule: List[Bundle], cycle: int) -> None:
  while len(schedule) <= cycle:
    schedule.append(Bundle(None, None, None, None, None))
  return

def schedule_instruction(instr: ProgramInstruction, instrDepList: List[Union[Dependency, InterloopDependency]], bb_start_cycle: int, schedule: List[Bundle], scheduled_times: Dict[int, TimesIntervale]) -> None:
  # Branch is scheduled in another function
  if(get_optype(instr.instrType) == OpType.BRANCH):
    return
  
  start_cycle = bb_start_cycle
  for dep in instrDepList:
    start_cycle = max(start_cycle, scheduled_times[dep.producerId].finish)

  instr_scheduled = False
  while not instr_scheduled:
    instr_scheduled = True
    check_to_add_empty_bundles(schedule, start_cycle)

    optype = get_optype(instr.instrType)
    bundle = schedule[start_cycle]
    if optype == OpType.ALU and not bundle.ALU0:
      bundle.ALU0 = copy.deepcopy(instr)
    elif optype == OpType.ALU and not bundle.ALU1:
      bundle.ALU1 = copy.deepcopy(instr)
    elif optype == OpType.MULT and not bundle.Mult:
      bundle.Mult = copy.deepcopy(instr)
    elif optype == OpType.MEM and not bundle.Mem:
      bundle.Mem = copy.deepcopy(instr)
    else:
      start_cycle += 1
      instr_scheduled = False
  
  scheduled_times[instr.iaddr] = TimesIntervale(start_cycle, start_cycle + get_latency(instr.instrType))
  return

def schedule_bb(bb: List[ProgramInstruction], depList: List[DependencyListEntry], bb_start_cycle: int, schedule: List[Bundle], scheduled_times: Dict[int, TimesIntervale]) -> int:
  for instr in bb:
    instrDepList = depList[instr.iaddr].localDeps + depList[instr.iaddr].interloopDeps + depList[instr.iaddr].loopInvariantDeps + depList[instr.iaddr].postLoopDeps
    schedule_instruction(instr, instrDepList, bb_start_cycle, schedule, scheduled_times)
  
  return len(schedule) - 1

def check_for_interloop_contraints(ii: int, depList: List[DependencyListEntry], scheduled_times: Dict[int, TimesIntervale]) -> bool:
  for entry in depList:
    for dep in entry.interloopDeps:
      prod_cycle = scheduled_times[dep.producerIdInterloop].start
      cons_cycle = scheduled_times[entry.id].start
      latency = get_latency(depList[dep.producerIdInterloop].instrType)
      if prod_cycle + latency > cons_cycle + ii:
        return False
  return True

def schedule_loop_instruction(instr: ProgramInstruction, start_cycle: int, schedule: List[Bundle], scheduled_times: Dict[int, TimesIntervale]) -> None:
  check_to_add_empty_bundles(schedule, start_cycle)
  bundle = schedule[start_cycle]
  bundle.Branch = copy.deepcopy(instr)
  scheduled_times[instr.iaddr] = TimesIntervale(start_cycle, start_cycle + get_latency(instr.instrType))
  return

def get_start_end_cycles_for_bb(bb: List[ProgramInstruction], scheduled_times: Dict[int, TimesIntervale]) -> TimesIntervale:
  start_cycle = scheduled_times[bb[0].iaddr].start
  end_cycle = scheduled_times[bb[0].iaddr].finish
  for instr in bb:
    if instr.iaddr in scheduled_times:
      times = scheduled_times[instr.iaddr]
      start_cycle = min(start_cycle, times.start)
      end_cycle = max(end_cycle, times.start)
  return TimesIntervale(start_cycle, end_cycle)

def schedule_simple(cao: CodeAnalyserOutput) -> SimpleSchedulerOutput:
  sso = SimpleSchedulerOutput([], {})
  bb0_finish_cycle = schedule_bb(cao.BB0, cao.dependencyList, 0, sso.schedule, sso.scheduled_times)
  if cao.BB1:
    schedule_bb(cao.BB1, cao.dependencyList, bb0_finish_cycle + 1, sso.schedule, sso.scheduled_times)
    bb1_times = get_start_end_cycles_for_bb(cao.BB1, sso.scheduled_times)
    ii = bb1_times.finish - bb1_times.start + 1
    while not check_for_interloop_contraints(ii, cao.dependencyList,sso.scheduled_times):
      ii += 1
    loop_cycle = bb1_times.start + ii - 1
    schedule_loop_instruction(cao.BB1[-1], loop_cycle, sso.schedule, sso.scheduled_times)
    sso.schedule[loop_cycle].Branch.imm = bb1_times.start
    if cao.BB2:
      schedule_bb(cao.BB2, cao.dependencyList, bb1_times.start + ii, sso.schedule, sso.scheduled_times)
  
  print_schedule(sso.schedule)
  return sso

def allocate_fresh_registers(depList: List[DependencyListEntry], schedule: List[Bundle]) -> Dict[int, str]:
  regMap = {}
  register_rename_id = 1
  for bundle in schedule:
    for instr in [bundle.ALU0, bundle.ALU1, bundle.Mem, bundle.Mult]:
      if instr and instr.dest and instr.dest != "LC" and instr.dest != "EC":
        instr.dest = f"x{register_rename_id}"
        regMap[instr.iaddr] = f"x{register_rename_id}"
        register_rename_id += 1
  return regMap

def link_operands(regMap: Dict[int, str], depList: List[DependencyListEntry], schedule: List[Bundle]) -> None:
  for bundle in schedule:
    for instr in [bundle.ALU0, bundle.ALU1, bundle.Mem, bundle.Mult]:
      if instr:
        instrDepList = depList[instr.iaddr].localDeps + depList[instr.iaddr].interloopDeps + depList[instr.iaddr].loopInvariantDeps + depList[instr.iaddr].postLoopDeps
        opA_linked = False
        opB_linked = False
        for dep in instrDepList:
          if instr.opA and dep.reg == instr.opA:
            instr.opA = regMap[dep.producerId]
            opA_linked = True
          if instr.opB and dep.reg == instr.opB:
            instr.opB = regMap[dep.producerId]
            opB_linked = True
        if instr.opA and not opA_linked:
          instr.opA = "UNUSED_REGISTER"
        if instr.opB and not opB_linked:
          instr.opB = "UNUSED_REGISTER"
  return

def find_loop_cycle(schedule: List[Bundle]) -> int:
  for i, bundle in enumerate(schedule):
    if bundle.Branch:
      return i
  return -1

def shift_schedule_by_one_after_loop(schedule: List[Bundle], loop_cycle: int):
  nb_bundles = len(schedule)
  prev_bundle = Bundle(None, None, None, None, None)
  schedule.append(prev_bundle)
  for i in range(loop_cycle + 1, nb_bundles + 1):
    bundle = schedule[i]
    schedule[i] = prev_bundle
    prev_bundle = bundle

  loop_instr = schedule[loop_cycle].Branch
  schedule[loop_cycle].Branch = None
  schedule[loop_cycle + 1].Branch = loop_instr
  return

def fix_interloop_dependencies(regMap: Dict[int, str], depList: List[DependencyListEntry], sso: SimpleSchedulerOutput) -> None:
  movs_seen = set()
  for entry in depList:
    for dep in entry.interloopDeps:
      bb0_reg = regMap[dep.producerId]
      bb1_reg = regMap[dep.producerIdInterloop]
      if bb0_reg and bb1_reg and bb0_reg != bb1_reg:
        reg_pair = (bb0_reg, bb1_reg)
        if reg_pair not in movs_seen:
          movs_seen.add(reg_pair) 
          mov_instr = ProgramInstruction(
                        iaddr=-1, 
                        instrType="mov",
                        dest=bb0_reg,
                        opA=bb1_reg,
                        opB='',
                        imm=0
                      )
          mov_scheduled = False
          loop_cycle = find_loop_cycle(sso.schedule)
          while not mov_scheduled:
            last_bundle = sso.schedule[loop_cycle]
            if sso.scheduled_times[dep.producerIdInterloop].finish <= loop_cycle:
              if not last_bundle.ALU0:
                last_bundle.ALU0 = mov_instr
                mov_scheduled = True
              elif not last_bundle.ALU1:
                last_bundle.ALU1 = mov_instr
                mov_scheduled = True
            if not mov_scheduled:
              shift_schedule_by_one_after_loop(sso.schedule, loop_cycle)
              loop_cycle += 1
  return           

def assign_unused_registers(schedule: List[Bundle], register_rename_next: int) -> None:
  register_rename_id = register_rename_next
  for bundle in schedule:
    for instr in [bundle.ALU0, bundle.ALU1, bundle.Mem, bundle.Mult]:
      if instr:
        if instr.opA == "UNUSED_REGISTER":
          instr.opA = f"x{register_rename_id}"
          register_rename_next += 1
        if instr.opB == "UNUSED_REGISTER":
          instr.opB = f"x{register_rename_id}"
          register_rename_next += 1


def rename_simple(cao: CodeAnalyserOutput, sso: SimpleSchedulerOutput) -> SimpleSchedulerOutput:
  regMap = allocate_fresh_registers(cao.dependencyList, sso.schedule)
  link_operands(regMap, cao.dependencyList, sso.schedule)
  fix_interloop_dependencies(regMap, cao.dependencyList, sso)
  assign_unused_registers(sso.schedule, max(regMap.keys()) + 1)

  print_schedule(sso.schedule)

  return sso
