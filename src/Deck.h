// Deck model, CSV import, JSON save/load, deck library folder.
// One deck = one JSON file. Progress lives in the same file as the cards.
#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace flashcards
{

struct Card
{
    juce::String id;
    juce::String question;
    juce::String answer;
    juce::String context;       // topic label, optional
    bool starred = false;
    int box = 0;                // 0 = never seen, 1..5 = Leitner box
    juce::int64 dueMs = 0;      // epoch ms, card is due when dueMs <= now
    juce::int64 lastReviewedMs = 0;
    int timesSeen = 0;
    int timesCorrect = 0;
    int timesWrong = 0;
};

struct Deck
{
    juce::String title;
    juce::int64 createdMs = 0;
    std::vector<Card> cards;
};

// Split CSV text into rows of fields. Handles quotes, "" escapes, CRLF, BOM.
std::vector<juce::StringArray> parseCsv(const juce::String& text);

// New deck from a CSV file. Needs Question + Answer columns. Context optional.
// No header match: column 0 = question, 1 = answer, 2 = context.
std::optional<Deck> deckFromCsv(const juce::File& csv, juce::String& error);

struct MergeResult
{
    int added = 0;
    int kept = 0;
};

// Fold incoming cards into existing deck. Match on question text.
// Existing progress and stars stay. Answer/context text updates from incoming.
MergeResult mergeCards(Deck& existing, const Deck& incoming);

bool saveDeck(const Deck& deck, const juce::File& file);
std::optional<Deck> loadDeck(const juce::File& file);

// Distinct non-empty context values, in first-seen order.
juce::StringArray topicsOf(const Deck& deck);

struct LibraryEntry
{
    juce::File file;
    juce::String title;
    int cardCount = 0;
    int dueCount = 0;
    int starredCount = 0;
};

class DeckLibrary
{
public:
    explicit DeckLibrary(juce::File dir);

    const juce::File& directory() const { return dir; }

    // Every deck JSON in the folder, sorted by title.
    std::vector<LibraryEntry> list(juce::int64 nowMs) const;

    // File for a title. Reuses the file if that title exists already.
    juce::File fileForTitle(const juce::String& title) const;

    // Default folder in the user's app-data area.
    static juce::File defaultDirectory();

private:
    juce::File dir;
};

} // namespace flashcards
