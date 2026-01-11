# POC_V06 — Notes & Changes

This file documents the features and fixes added to the POC_V06 sketch.

## ✅ New features

- **Weighted CSV selection**
  - Lines can begin with an integer weight followed by a comma to increase selection probability:
    - Example: `3,Goblin` → Goblin has weight 3.
  - If no leading weight is present or the token is invalid, the line defaults to weight = 1.
  - Selection uses **single-pass weighted reservoir sampling** so files are read once and memory usage stays minimal (good for RP2040).

- **JSON recipe compatibility (V1)**
  - `.json` recipe files appear alongside `.csv` files and are executed when selected.
  - Recipes declare `parts[]` with `roll`, optional `label`, and optional probability `p` (0.0–1.0).
  - Relative (`roll": "names.csv"`) and root (`roll": "/items/treasure.csv"`) paths are supported.

## 🛠 Fixes & Robustness

- **Skip malformed / empty entries**
  - Lines that are empty or effectively a lone comma (e.g. `,` or `1,`) are now skipped and do not affect weights.
  - This prevents returning empty strings or stray commas as selection results.

- **Cross-folder recipe path normalization**
  - Recipe paths starting with `/data/...` (case-insensitive) or `/items/...` are normalized to the runtime `/DATA/...` root so cross-category references work reliably.

## ⚠️ Notes / Limitations

- Weight parsing expects an integer in the first token; non-integer tokens fall back to weight = 1.
- Non-positive parsed weights are clamped to `1`.
- Line buffer size is ~220 bytes; very long lines (>220 characters) may be truncated.
- Hidden files (leading `_`) are **not shown** in the UI but are available to recipes (intended behavior).

## 🧪 Testing suggestions

- Example test files are in `/data/Test/` (e.g. `encounter_gen.json`, `monsters.csv`, `_quant.csv`).
- On-device: select the `Test` category and run the `encounter_gen.json` recipe to verify output and rerolls.
- For debugging, enabling temporary Serial logging in `pickRandomCsvLine` can print skipped lines and parsed weights.

---

If you'd like, I can also add an `examples/` subfolder with sample weighted CSVs and a small serial test runner to demonstrate distributions. Let me know if you want that added. ✨
