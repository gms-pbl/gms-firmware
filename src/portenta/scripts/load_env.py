import os

Import("env")

env_file = os.path.join(env.get("PROJECT_DIR"), ".env")

if os.path.isfile(env_file):
    print(f"Loading environment variables from {env_file}")
    with open(env_file, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            
            key, value = line.split("=", 1)
            # Remove optional surrounding quotes
            value = value.strip('"').strip("'")
            
            # Pass to compiler as -DKEY="VALUE"
            # Note the escaped quotes for string macros
            env.Append(CPPDEFINES=[(key, env.StringifyMacro(value))])
else:
    print(f"Warning: {env_file} not found. Make sure to create it.")
