# generate_stress_test.py
import random
import string

FILENAME = "test.txt"
NUM_GLOBALS = 1000   # many globals to stress long global logic
NUM_PRINTS = 200     # number of globals to print

def random_name(length=5):
    return ''.join(random.choices(string.ascii_lowercase, k=length))

# Generate global variable names
globals_list = [f"{random_name()}_{i}" for i in range(NUM_GLOBALS)]

with open(FILENAME, "w") as f:
    # Define all globals
    for i, name in enumerate(globals_list):
        f.write(f"let {name} = {i};\n")

    # Print a selection of globals randomly
    for _ in range(NUM_PRINTS):
        name = random.choice(globals_list)
        f.write(f"print {name};\n")

print(f"Stress test '{FILENAME}' generated with {NUM_GLOBALS} globals and {NUM_PRINTS} prints.")
