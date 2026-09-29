import runpy
import os
import sys
import traceback

script = sys.argv[1]
sys.argv = [script]
sys.path.insert(0, os.path.dirname(os.path.abspath(script)))
exit_code = 0
try:
    runpy.run_path(script, run_name='__main__')
except SystemExit as error:
    exit_code = error.code or 0
except Exception:
    exit_code = 1
    traceback.print_exc()
finally:
    try:
        input('\nПрограмма завершена. Нажмите Enter, чтобы закрыть окно…')
    except (EOFError, KeyboardInterrupt):
        pass
sys.exit(exit_code)
