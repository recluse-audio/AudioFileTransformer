# AudioFileTransformer — TODO Dashboard

> **System**: Work items live in `ITEMS/`, `IN_PROGRESS/`, and `DONE/`.
> Epics are subfolders inside any of these directories; all stories within an epic folder in `IN_PROGRESS/` are considered in progress.
> Next ticket number: **0008**

---

## In Progress

| ID | Title | File |
|----|-------|------|
| 0002 | Test for window application to grain | [[0002_test-window-application-to-grain]] |

---

## Items (Backlog)

| ID | Title | File |
|----|-------|------|
| 0003 | End to End | [[0003_end-to-end]] |
| 0004 | Hook up TD_Grain stuff to PitchManager in testing | [[0004_hook-up-td-grain-to-pitch-manager]] |
| 0005 | TDPSOLA_Processor owns all of this | [[0005_tdpsola-processor-ownership]] |
| 0006 | Handle processor graph tail length *(deferred)* | [[0006_handle-processor-graph-tail-length]] |
| 0007 | Window must be 2x period | [[0007_window-must-be-2x-period]] |

---

## Done

| ID | Title | File |
|----|-------|------|
| 0001 | Make TD_Granulator/TD_Grain with synth mark array support | [[0001_td-granulator-grain-synth-marks]] |

---

## Epics

> Epics are folders containing related stories. Place the epic folder in `IN_PROGRESS/` to mark all stories as active.

*No epics yet.*

---

## Conventions

- **New ticket**: Copy `_TEMPLATE.md`, increment the counter above, name the file `NNNN_short-title.md`.
- **Start work**: Move file to `IN_PROGRESS/`, set `status: in_progress`, update this dashboard.
- **Complete**: Move file to `DONE/`, set `status: done`, add `completed:` date, update this dashboard.
- **Epic**: Create a subfolder in the target status directory (e.g. `IN_PROGRESS/SOME_EPIC/`) and place story files inside.
- **Links**: Use Obsidian-style `[[filename]]` (no extension needed) for cross-references between items.
