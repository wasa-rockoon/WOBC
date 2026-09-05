"""Run real LoRa/E220/Packet C++ sources with simulated UART/AUX (no hardware)."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--cxx", default="g++", help="Host C++14 compiler")
parser.add_argument("--emsdk", type=Path, help="Optional Emscripten SDK root")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = root / ".pio" / "lora-tests"
out.mkdir(parents=True, exist_ok=True)
sources = [
    "tests/lora/test_lora.cpp",
    "src/components/LoRa/lora.cpp",
    "src/components/LoRa/e220.cpp",
    "src/library/wcpp/cpp/Packet.cpp",
    "src/library/wcpp/cpp/float16.cpp",
]
options = ["-std=c++14", "-DARDUINO", "-Itests/lora/stubs", "-Isrc"]
env = os.environ.copy()
if args.emsdk:
    import sys
    sdk = args.emsdk.resolve()
    env["EM_CONFIG"] = str(sdk / ".emscripten")
    output = out / "test_lora.js"
    compile_cmd = [sys.executable, str(sdk / "emscripten" / "em++.py")]
    options += ["-sENVIRONMENT=node", "-sSINGLE_FILE=1"]
    node = sdk / "node" / ("node.exe" if os.name == "nt" else "node")
    run_cmd = [str(node), str(output)]
else:
    output = out / ("test_lora.exe" if os.name == "nt" else "test_lora")
    compile_cmd = [args.cxx]
    run_cmd = [str(output)]
subprocess.run(compile_cmd + options + sources + ["-o", str(output)], cwd=root, env=env, check=True)
subprocess.run(run_cmd, cwd=root, env=env, check=True)
