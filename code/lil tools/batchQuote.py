#Walk each immediate subfolder
#Target .csv and .txt files
#Rewrite each file so every non-empty line becomes:
#"original line contents"
#It will not double existing quotes yet
#It will not try to parse CSV columns
#This is a blunt, mechanical transformation by design

from pathlib import Path

base_dir = Path(".")

for folder in base_dir.iterdir():
    if not folder.is_dir():
        continue

    print(f"Entering folder: {folder.name}")

    for file_path in folder.iterdir():
        if file_path.suffix.lower() not in (".txt", ".csv"):
            continue

        print(f"  Processing: {file_path.name}")

        lines = file_path.read_text(encoding="utf-8").splitlines()

        new_lines = []
        for line in lines:
            if line.strip() == "":
                new_lines.append(line)
            else:
                new_lines.append(f"\"{line}\"")

        file_path.write_text("\n".join(new_lines) + "\n", encoding="utf-8")

print("Done.")
