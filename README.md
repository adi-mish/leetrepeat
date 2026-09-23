# LeetRepeat

A local C++20 / Qt 6 desktop utility for memorizing a fixed corpus of problems. Learn a few new problems each day, reproduce due solutions from memory, and mark each review **PASS** or **FAIL**. Everything stays in a local SQLite database. No accounts, servers, scraping, or online services.

## Build on Ubuntu / WSL2

Keep the repository in the Linux filesystem, for example `~/projects/leetrepeat`. WSLg supplies the display; there is no WSL-specific application code.

On Ubuntu 22.04 or later:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qt6-base-dev-tools \
  qt6-declarative-dev-tools libqt6sql6-sqlite qt6-qpa-plugins qt6-wayland \
  qml6-module-qtquick qml6-module-qtquick-window \
  qml6-module-qtquick-controls qml6-module-qtquick-templates \
  qml6-module-qtquick-layouts qml6-module-qtquick-dialogs \
  qml6-module-qtqml-workerscript

cmake -S . -B build -G Ninja
cmake --build build
./build/leetrepeat
```

Requires Qt **6.2+**, CMake **3.21+**, and a C++20 compiler. Qt Quick Controls uses the Fusion style by default, honoring Qt's palette. You may select another installed Qt Quick Controls style using `QT_QUICK_CONTROLS_STYLE`.

If a QML module or plugin is reported missing, install the complete runtime list above. In particular, `qt6-declarative-dev` alone does not install the QML Controls, Templates, Dialogs, and WorkerScript plugins. Qt SQL also requires the separate SQLite driver package.

## Daily use

1. Open **Problems**, then **Import CSV** or **Add problem**.
2. **Today** shows new problems, all reviews due on or before today, and the remaining total.
3. **Start Session** presents due reviews first, then the day's new problems. Reviews shuffle by default; unseen problems follow import/add order.
4. For a new problem, open its URL, study externally, and optionally write notes and a canonical solution. **Mark Learned** saves the notes and schedules tomorrow's first review. Initial learning is not a PASS attempt.
5. For a review, reproduce the solution cold. Reveal notes if needed, then choose PASS or FAIL. Each result is saved immediately before advancing.

The daily new target defaults to **3**. Already learned problems count toward the day's target across restarts and resets. Unused new-problem allowance does not accumulate. A target of 0 pauses new intake while retaining all due reviews. When the corpus is exhausted, sessions contain only reviews; when nothing is due, there is no extra review queue.

| Review shortcut | Action |
| --- | --- |
| `P` | PASS |
| `F` | FAIL |
| `Space` | Reveal / hide notes and solution |
| `O` | Open the problem URL in your browser |
| `Esc` | Return to Today |
| `Enter` | Start a session from Today / leave a completed session |
| `Ctrl+S` | Save edits on the problem detail page |

Single-letter grading shortcuts are disabled while studying a new problem so typing notes cannot grade anything. Tab navigation and standard buttons also work. Leaving a page or closing the window with unsaved edits asks before discarding them.

The library searches titles independently of status, difficulty, and tag filters. Multiple semicolon-separated tag filters require **all** selected tags. The tag picker helps select existing tags; custom tags can be entered in problem details. Tags are metadata and never affect scheduling.

## Scheduling

Default intervals: **1, 2, 4, 7, 14, 30, 60, 120, 240 days**.

- Initial learning starts at stage 1 (zero-based stage 0 in stored state), due tomorrow.
- PASS advances one stage, capped at the last stage, and schedules from the date of the attempt.
- FAIL resets to the first stage, due tomorrow.
- At the final stage, PASS keeps scheduling the final interval indefinitely.
- Every learned problem with `next_review <= today` remains due, including after skipped days.

Settings allow 1–32 strictly increasing positive integer intervals, capped at 36500 days. Even with custom intervals, initial learning and FAIL remain due tomorrow. Interval changes apply when the next result is recorded; existing due dates do not move. If the list is shortened, future scheduling clamps the saved stage to the new range. Optional jitter varies intervals of at least 14 days by up to `floor(interval / 10)` days; initial learning and FAIL are never jittered.

Dates use the computer's local calendar. Timestamps are saved in UTC, alongside the local date of each learning/review event, so daily totals do not change when a timestamp is reinterpreted in another timezone. The dashboard refreshes on navigation and after midnight (within 30 seconds). An in-progress session retains its selected queue; return to Today and start another session to include newly due work.

## CSV import and export

UTF-8 CSV, with these headers:

```csv
title,url,difficulty,notes,solution,tags
Two Sum,https://leetcode.com/problems/two-sum/,Easy,"Remember the complement invariant",,"Arrays;Hashing"
```

- Required fields: nonempty `title` and a valid `http`/`https` `url`.
- Optional: `difficulty` (`Easy`, `Medium`, `Hard`, `Unknown`), `notes`, `solution`, and `tags` or `topics`.
- Tags use **semicolons**. New tags are created automatically. Tag names are whitespace-normalized and case-insensitively unique. A tag name cannot contain a semicolon.
- Standard quoted fields support embedded commas, line breaks, and doubled quotes. UTF-8 BOM and CRLF are accepted. Malformed quoting, invalid UTF-8, missing fields, invalid difficulty, and mismatched field counts produce errors.
- Preview shows every record's status. Duplicates match a whitespace-normalized, case-folded title **or** URL with query/fragment and trailing slash removed. Duplicates already in the library or earlier in the same file are skipped, explicitly shown in the preview, and never overwrite existing notes or progress.
- **Any invalid record blocks import.** Once confirmed, every ready record and its tags are inserted in one transaction. A write error rolls back the entire import.
- Other columns are ignored. Export includes metadata and a snapshot of scheduling fields, but CSV import deliberately imports the **corpus only**, as unlearned problems. Use database backups to preserve/restore progress and attempt history.

Try [examples/problems.csv](examples/problems.csv). Importing does not fetch problem statements or solutions from LeetCode.

## Database and backups

**Settings → Your data** shows the exact database path and opens its folder. The default uses Qt's `AppLocalDataLocation`, independent of the working directory (under `$XDG_DATA_HOME` / `~/.local/share` on Linux, and the user's local application data directory on Windows).

Use **Back up database…** to create a consistent standalone SQLite snapshot, including committed data still in the write-ahead log. Choose a new filename; backups never overwrite existing files. A CSV export is not a full backup.

To restore:

1. Close every LeetRepeat window.
2. Preserve the current database and any matching `-wal` / `-shm` files together in a separate backup folder.
3. Put the saved SQLite snapshot at the database path shown in Settings. Do not leave old `-wal` / `-shm` files alongside a restored database.
4. Reopen LeetRepeat. Alternatively, inspect a snapshot without replacing your normal data:

```bash
./build/leetrepeat --database /absolute/path/to/backup.sqlite
```

The app takes an instance lock for its database. Review state and attempt insertion are committed together. Schema versioning, foreign keys, indexes, prepared queries, WAL, and full synchronous writes are enabled. Future unsupported schema versions are rejected.

Attempt history is append-only and includes timestamp, result, scheduler identity/version, complete state before/after, and previous/resulting due dates. Scheduler snapshots include the intervals, jitter setting, and actual scheduled days. Database triggers prevent attempt updates or deletion. Resetting progress clears the active schedule and counters while retaining past attempts and daily learning events. Deleting a problem hides it from the library/queues; its database record remains as a tombstone to retain its historical attempts. Confirmations explain these actions.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

Qt Test covers scheduling boundaries, repeated passes and failures, custom interval validation, jitter bounds, empty and partially learned corpora, skipped days, daily quota enforcement, immutable history, atomic write failure rollback, reopen persistence, snapshot restoration, CSV round trips/validation, and a QML integration workflow with keyboard grading and unsaved-edit protection. Tests use temporary databases and never touch your normal data.

The workflow test defaults to Qt's offscreen software renderer. For a one-shot application startup check:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/leetrepeat --database /tmp/leetrepeat-check.sqlite --smoke-test
```

`--smoke-test` exits after loading the UI. For a production build without tests, configure with `-DBUILD_TESTING=OFF`.

## Source layout and extension points

- `src/domain`: problem, settings, and serializable scheduler state.
- `src/database`: connection lifetime, transactional migration, and consistent snapshots.
- `src/repositories`: prepared persistence operations and transaction boundaries.
- `src/scheduling`: `IScheduler` and the sole v1 implementation, `FixedIntervalScheduler`.
- `src/services`: date-based queue generation and CSV parsing/preview/export.
- `src/models`: the searchable/filterable `QAbstractListModel`.
- `src/controllers`: application actions and review-session state exposed to QML.
- `qml`: presentation, navigation, dialogs, and keyboard interactions. No SQL or scheduling rules.
- `tests`: headless core tests and the desktop workflow test.

Schema v1 contains `problems`, `attempts`, `settings`, `schema_version`, normalized `tags` / `problem_tags`, and a small `learning_events` table for durable daily intake accounting. Future migrations belong in `Database::migrate()`.

To experiment with a future scheduler, implement `IScheduler` and select it where the review controller constructs the scheduler. Per-problem state already stores scheduler identity/version and an extensible JSON metadata object. Advanced algorithms are intentionally not implemented.

## Native Windows later

Install Qt 6.2+ for your chosen MSVC or MinGW compiler, with Qt Declarative / Quick Controls and SQLite SQL driver support. From a matching compiler environment:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build
ctest --test-dir build --output-on-failure
.\build\leetrepeat.exe
```

Use your actual Qt version/compiler directory. For packaging, use the matching Qt installation's `windeployqt` with `--qmldir qml` to collect the QML runtime dependencies. The native build uses Windows data paths and requires no WSL runtime. Native Windows packaging has not been tested here.

Qt references: [CMake integration](https://doc.qt.io/qt-6/cmake-build-qml-application.html), [Qt Quick Dialogs](https://doc.qt.io/qt-6/qtquickdialogs-index.html).
