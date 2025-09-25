import sys

# Usage: python generate_script.py PlayerController

if len(sys.argv) < 2:
    print("Usage: python generate_script.py <ClassName>")
    sys.exit(1)

classname = sys.argv[1]

# Read the template
with open("ScriptTemplate.inl", "r") as f:
    template = f.read()

# Replace the placeholder with the actual class name
output = template.replace("$CLASSNAME$", classname)

# Write to a new header file
output_file = f"../../Assets/Scripts/{classname}.cpp"
with open(output_file, "w") as f:
    f.write(output)

print(f"Generated script: {output_file}")