import sys
sys.setrecursionlimit(10000)
# Insert python-interpreter dir so untildeath package is importable
import os
script_dir = os.path.dirname(os.path.abspath(__file__))
interp_dir = os.path.join(script_dir, '..', 'python-interpreter')
sys.path.insert(0, interp_dir)
# Now run untildeath as main
import runpy
runpy.run_path(os.path.join(interp_dir, 'untildeath.py'), run_name='__main__')
