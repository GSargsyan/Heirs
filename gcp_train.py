import os
import subprocess

# Configurations
NUM_GAMES = 100000 

def generate_data():
    if not os.path.exists("cli/generate_data.exe") and not os.path.exists("cli/generate_data"):
        print("Compiling data generator...")
        compile_cmd = ["g++", "-O3", "-std=c++17", "cli/generate_data.cpp", "src/engine_v1.cpp", "src/board.cpp", "-o", "cli/generate_data"]
        subprocess.run(compile_cmd, check=True)
    
    print("Running self-play generation...")
    exe_name = "cli/generate_data" if os.name != 'nt' else "cli/generate_data.exe"
    subprocess.run([exe_name, str(NUM_GAMES)], check=True)

if __name__ == "__main__":
    generate_data()