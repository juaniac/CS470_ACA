
import sys
from jsonParser import load_program_from_json
from codeAnalyser import analyse_code
from simpleCompiler import schedule_simple, rename_simple
from jsonOut import schedule_to_json

if __name__ == "__main__":
  if len(sys.argv) != 4:
    print("Usage: python3 src/main.py </path/to/input.json> </path/to/simple.json> </path/to/pip.json>")
    sys.exit(1)

  input_path = sys.argv[1]
  simple_path = sys.argv[2]
  pip_path = sys.argv[3]

  program_instructions = load_program_from_json(input_path)
  cao = analyse_code(program_instructions)
  sso = schedule_simple(cao)
  sso = rename_simple(cao, sso)
  schedule_to_json(sso.schedule, simple_path)
  schedule_to_json(sso.schedule, pip_path)
  