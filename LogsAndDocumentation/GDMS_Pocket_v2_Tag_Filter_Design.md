# GDMS:Pocket v2: Tag Filter ("Setting") Design Notes

**Status:** Idea / design draft, 2026-10-10. Reserved for GDMS-pocket 2. Nothing here is implemented in v1 (Alpha 10).
**Open questions are deliberately left open.** See the last section.

---

## Goal

Let a DM narrow the whole device to a theme (fantasy, modern, western, a dwarven campaign, orc names, ...) so that:

1. The main menu only shows categories and files that can actually produce relevant results.
2. Rolling a table only returns entries that match the chosen theme.
3. A roll never comes back blank and never returns off-theme results (no space table in a dwarf campaign).

Most of the work should happen on the device itself, not in a PC tool.

## Approaches considered

| Approach | Verdict |
|---|---|
| Per-setting config files listing included folders (YAML/JSON) | Simple, but coarse. Every new theme means hand-listing folders, and it can't narrow *within* a table. Tags subsume it (see "Named bundles"). |
| SQL / SQLite on the RP2040 | Rejected. Heavy on RAM and flash, slow over SD, and the data is flat lists with labels, so joins and normalization buy nothing. A tag column is the whole relation needed. |
| Duplicate filtered copies of every table to SD | Rejected. SD wear, filter-time delay, and storage cost, with no benefit over filtering at read time. |
| **Live tag filter inside the existing roll loop** | **Chosen.** `pickRandomCsvLine` already scans the whole file on every roll, so the filter is one extra predicate in that loop. Weighted reservoir sampling is unchanged. No copies are made. |

## CSV format (v2)

- **Column 1:** weight (always present; `1` for unweighted tables).
- **Column 2:** entry text.
- **Columns 3+:** tags, any number.
- Standard CSV quoting: a field containing a comma is wrapped in double quotes, and a literal quote inside a field is doubled (`""`). Spreadsheet apps (Excel, LibreOffice, Google Sheets) produce this on export automatically.

```
3,"Goblin ambusher, armed with a net",fantasy,monsters
1,Rust-eaten drone,scifi,monsters
```

Parser notes:
- Replaces the current `indexOf(',')` parsing with a small field-by-field parser.
- Requiring the weight column removes the v1 ambiguity where a text-only line might be mistaken for a weighted one.
- Skip empty tag cells (spreadsheet exports add trailing `,,,`), and trim and lowercase tags.
- The 220-byte line buffer in v1 will need to grow to fit tag columns.

## Filter semantics

- The active filter is a **set of tags held in RAM** and persisted in `settings.cfg`.
- **Multiple tags combine with AND**: an entry must carry every selected tag. Each added tag narrows the result, e.g. `fantasy` + `orc` gives fantasy orc names.
- The roll loop simply skips rows that fail the predicate, then continues normal weighted reservoir sampling.

## Tag index and the "Rebuild Tag Index" option

- `/DATA/_tags.txt` lists every tag found across the library. It feeds the tag-picker menu. (The leading underscore means v1's file lister already ignores it.)
- **Options menu** gets a Filter entry, with **Rebuild Tag Index** directly beneath it. Rebuild is an on-device blocking scan with a progress screen.
- The index holds only facts that don't depend on the active filter. Anything that does depend on the filter is computed when the filter is applied (below).

## Applying a filter (pre-screen)

When the user confirms a filter:

1. Show an "Applying filter..." screen and make one pass over every CSV.
2. Record, in RAM, which files have **at least one entry matching all active tags**.
3. A category is shown in the main menu only if it contains at least one visible file; a file is listed only if it passed step 2.

This is exact (it handles the AND case, which a per-file tag list cannot) and prevents blank rolls. The cost is one scan at confirm time; time it on a real library before optimizing. If it ever gets slow, the upgrade path is a per-file byte-offset index per tag, so rolls seek instead of scan. Not needed at current scale.

Optional "named bundles": a setting like `Fantasy` can be a one-line JSON file that is just a saved list of tags, so users pick a bundle in Options instead of assembling tags by hand. Tags are the mechanism and bundles are a convenience layer.

## JSON recipes

Recipes make two separate problems, which should be kept distinct:

1. **Descriptive tags** describe what a recipe *is*. An optional `"tags"` array in the recipe controls whether it appears in the menu (shown if its tags match the active filter, or if it has none).
2. **Imposed filters** are constraints a recipe puts on its own parts. An "Orc NPC" recipe always rolls with `orc` added on top of the user's filter.
3. **Derived visibility** is whether a recipe can be served at all under the user's current filter. This is the thorny one.

Derived visibility is **dynamic**, so it is not part of Rebuild Tag Index. It runs right after the CSV pre-screen: open each (small) recipe JSON, read the `roll` paths of its parts, and look them up in the pre-screen results. No CSV is rescanned.

Candidate visibility rule: hide a recipe if any part that always fires (`p` = 1, the default) points at a file with no matching entries. Parts with `p` < 1 are optional and simply skip when empty. An optional per-part `"filter": false` exempts a part from filtering (for example a generic weather roll).

Complication with imposed filters: pre-screen results were computed for the user's tags alone, so they can't say whether a file has an entry carrying the user's tags *and* the imposed tag. Such recipes need an exact rescan of just their own files with the combined tag set. There should be few of these. If imposed filters are dropped from v2, derived visibility becomes a pure lookup.

Smaller points:
- Roll targets in v2 are CSVs only. A recipe that calls another recipe needs recursion and cycle protection.
- A recipe pointing at a missing file looks identical to "no matches" and would vanish silently. Rebuild Tag Index already walks the files, so it can report broken references (e.g. `recipe X: missing /DATA/names/orc.csv`).
- Parse recipes at filter time first; only add a dependency cache if timing demands it, since a cache goes stale whenever a recipe is edited without rebuilding.

## v1 compatibility notes

- `loadSettings` ignores unknown keys, so a v2 `settings.cfg` loads on v1.
- `_`-prefixed files are already skipped by v1's lister.
- Gap: v1's `pickRandomCsvLine` would display v2 rows as raw text (quotes and tag columns visible). Only matters if v2 data cards will be used on v1 units. A small parser tweak in v1 would fix it.

## Hardware consideration

The filter pass and larger line buffers use more RAM. Settle early whether v2 stays on the RP2040 (264 KB SRAM) as planned, since this feature shapes the headroom.

---

## Open questions (undecided)

1. **Recipe parts with no matches:** strictly hide the recipe, or fall back to unfiltered output for that part? (Fallback gives a complete but possibly off-theme result.)
2. **Imposed filters:** include them in v2, or only descriptive tags?
3. **Untagged CSV rows:** should they match every filter (keeps existing files working, but can dilute a themed roll when a file mixes tagged and untagged rows), or appear only when no filter is active?
