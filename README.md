# Quiz Me Flashcards

A free desktop flashcard app. Load a deck from a spreadsheet (CSV), quiz yourself
with spaced repetition, star the cards you keep missing, or export the deck as an
ebook (EPUB) to read on a Kindle or phone.

Works on Windows, macOS (Intel and Apple Silicon), and Linux.

## Download and run

1. Go to the **Releases** page of this repo and download the zip for your system.
2. Unzip it. Keep the `sample_decks` folder next to the app.
3. Open **Flashcard Quiz**.

The app is not code-signed, so your computer will warn you the first time.

**Windows:** if SmartScreen says "Windows protected your PC", click **More info**,
then **Run anyway**.

**macOS:** Gatekeeper will block the app, and right-click > Open may not get you
past it. This works every time. Open Terminal, go to the unzipped folder, and run:

    xattr -dr com.apple.quarantine "Flashcard Quiz.app"

Then open the app normally. (You can also try System Settings > Privacy & Security
> **Open Anyway**, which appears after a blocked launch.)

**Linux:** if it will not start, make it executable: `chmod +x "Flashcard Quiz"`.

## Your first deck

1. Click **Import CSV...**. The dialog opens in `sample_decks`.
2. Pick a deck, for example `comptia_security_plus.csv`.
3. Select the deck in the list and click **Study** (or double-click it).
4. Choose which cards to study, then click **Start**.

Bundled decks: CompTIA A+ (Core 1 and Core 2), CompTIA Security+ (SY0-701),
AI Foundations, and Networking basics. The CompTIA decks are original study cards
written from the public exam objective topics, not copied from any exam dump.
Check them against the current official objectives before relying on them for an exam.

## Studying

The setup screen lets you pick:

- **Which cards:** all, due now, starred only, previously wrong, or new
- **Topic:** only cards from one section of the deck
- **Session size:** everything, or 10, 20, or 50 cards
- **Shuffle order:** on or off

During a session you see the question, then the answer. Grade yourself with
**Wrong** or **Correct**. Spaced repetition does the rest: cards you get right
come back after longer and longer gaps (1, 3, 7, then 21 days). Cards you get wrong
come back right away, and again at the end of the same session.

| Key | Action |
| --- | --- |
| Space, Enter, or Right arrow | Continue (question, then answer, then next card) |
| Backspace or Left arrow | Go back |
| 1 | Wrong |
| 2 | Correct |
| S | Star or unstar the card |
| Esc | End the session |

The arrow buttons beside the card and the **X** in the corner do the same as the keys.

**Star** is a bookmark that is saved with the card. Pick "Starred only" on the setup
screen to drill just those cards. **Clear all stars** resets them.

Progress is saved automatically after every answer, so you can quit any time.

## Making your own deck

Make a CSV with these columns. The first row is the header:

    Question,Answer,Context

- **Question** and **Answer** are required.
- **Context** is optional. It becomes the topic you can filter by.

Quote any field that contains a comma, for example `"Port 80, 443",Web traffic,Networking`.
Any spreadsheet app can export CSV.

Importing a CSV with the same title as an existing deck updates it. New questions are
added, edited answers are updated, and your progress and stars are kept.

## Export as an ebook

Select a deck and click **Export EPUB...** to save the whole deck. On the setup screen,
**Export these cards as EPUB...** saves only the cards that match your current filters
(for example, just the starred ones). Each question links to its answer, so it works
in any EPUB reader and on Kindle.

## Where your decks are saved

Decks and progress are plain JSON files, one per deck:

- Windows: `%APPDATA%\FlashcardQuiz\decks`
- macOS: `~/Library/FlashcardQuiz/decks`
- Linux: `~/.config/FlashcardQuiz/decks`

Deleting a deck in the app removes its progress, not your original CSV.

## Build from source

You need CMake 3.22 or newer and a C++20 compiler. JUCE is downloaded automatically.

    cmake -S . -B build
    cmake --build build
    build/flashcard_engine_test_artefacts/Release/flashcard_engine_test

On Linux you also need the X11, ALSA, FreeType, and fontconfig development packages.
See `.github/workflows/release.yml` for the exact list.

## License

AGPL-3.0. See `LICENSE`. This app is built with [JUCE](https://juce.com), which is also
available under the AGPL.
