#include "Deck.h"

#include <algorithm>

namespace flashcards
{

std::vector<juce::StringArray> parseCsv(const juce::String& rawText)
{
    // BOM off the front
    juce::String text = rawText.startsWithChar(0xFEFF) ? rawText.substring(1) : rawText;

    std::vector<juce::StringArray> rows;
    juce::StringArray row;
    juce::String field;
    bool inQuotes = false;
    bool fieldStarted = false;

    auto endField = [&]
    {
        row.add(field);
        field.clear();
        fieldStarted = false;
    };
    auto endRow = [&]
    {
        endField();
        // blank line = one empty field, skip it
        if (!(row.size() == 1 && row[0].trim().isEmpty()))
            rows.push_back(row);
        row.clear();
    };

    for (int i = 0; i < text.length(); ++i)
    {
        const juce::juce_wchar c = text[i];

        if (inQuotes)
        {
            if (c == '"')
            {
                if (text[i + 1] == '"') { field += '"'; ++i; }
                else inQuotes = false;
            }
            else field += c;
        }
        else if (c == '"' && !fieldStarted) { inQuotes = true; fieldStarted = true; }
        else if (c == ',') endField();
        else if (c == '\r') { if (text[i + 1] == '\n') ++i; endRow(); }
        else if (c == '\n') endRow();
        else { field += c; fieldStarted = true; }
    }

    // last row with no trailing newline
    if (fieldStarted || !field.isEmpty() || row.size() > 0)
        endRow();

    return rows;
}

namespace
{
    int findColumn(const juce::StringArray& header, std::initializer_list<const char*> names)
    {
        for (int i = 0; i < header.size(); ++i)
            for (auto* name : names)
                if (header[i].trim().equalsIgnoreCase(name))
                    return i;
        return -1;
    }

    juce::String cell(const juce::StringArray& row, int index)
    {
        return (index >= 0 && index < row.size()) ? row[index].trim() : juce::String();
    }

    juce::String normKey(const juce::String& q) { return q.trim().toLowerCase(); }
}

std::optional<Deck> deckFromCsv(const juce::File& csv, juce::String& error)
{
    if (!csv.existsAsFile())
    {
        error = "File not found: " + csv.getFullPathName();
        return std::nullopt;
    }

    auto rows = parseCsv(csv.loadFileAsString());
    if (rows.empty())
    {
        error = "CSV is empty.";
        return std::nullopt;
    }

    int qCol = findColumn(rows[0], {"Question", "Q", "Term", "Front"});
    int aCol = findColumn(rows[0], {"Answer", "A", "Definition", "Back"});
    int cCol = findColumn(rows[0], {"Context", "Topic", "Category", "Section"});
    size_t firstData = 1;

    if (qCol < 0 || aCol < 0)
    {
        // no header, go by position
        qCol = 0;
        aCol = 1;
        cCol = rows[0].size() > 2 ? 2 : -1;
        firstData = 0;
    }

    Deck deck;
    deck.title = csv.getFileNameWithoutExtension().replaceCharacter('_', ' ');
    deck.createdMs = juce::Time::currentTimeMillis();

    for (size_t r = firstData; r < rows.size(); ++r)
    {
        Card card;
        card.question = cell(rows[r], qCol);
        card.answer = cell(rows[r], aCol);
        card.context = cell(rows[r], cCol);
        if (card.question.isEmpty() || card.answer.isEmpty())
            continue;
        card.id = juce::Uuid().toString();
        deck.cards.push_back(std::move(card));
    }

    if (deck.cards.empty())
    {
        error = "No cards found. Need Question and Answer columns.";
        return std::nullopt;
    }
    return deck;
}

MergeResult mergeCards(Deck& existing, const Deck& incoming)
{
    MergeResult result;

    for (const auto& card : incoming.cards)
    {
        auto key = normKey(card.question);
        auto match = std::find_if(existing.cards.begin(), existing.cards.end(),
                                  [&](const Card& c) { return normKey(c.question) == key; });
        if (match != existing.cards.end())
        {
            match->answer = card.answer;
            match->context = card.context;
            ++result.kept;
        }
        else
        {
            existing.cards.push_back(card);
            ++result.added;
        }
    }
    return result;
}

bool saveDeck(const Deck& deck, const juce::File& file)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("version", 1);
    root->setProperty("title", deck.title);
    root->setProperty("createdMs", deck.createdMs);

    juce::Array<juce::var> cards;
    for (const auto& c : deck.cards)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("id", c.id);
        o->setProperty("question", c.question);
        o->setProperty("answer", c.answer);
        o->setProperty("context", c.context);
        o->setProperty("starred", c.starred);
        o->setProperty("box", c.box);
        o->setProperty("dueMs", c.dueMs);
        o->setProperty("lastReviewedMs", c.lastReviewedMs);
        o->setProperty("timesSeen", c.timesSeen);
        o->setProperty("timesCorrect", c.timesCorrect);
        o->setProperty("timesWrong", c.timesWrong);
        cards.add(juce::var(o));
    }
    root->setProperty("cards", cards);

    file.getParentDirectory().createDirectory();

    // write temp then move, so a crash mid-save cannot wreck the deck
    juce::TemporaryFile temp(file);
    if (!temp.getFile().replaceWithText(juce::JSON::toString(juce::var(root))))
        return false;
    return temp.overwriteTargetFileWithTemporary();
}

std::optional<Deck> loadDeck(const juce::File& file)
{
    auto parsed = juce::JSON::parse(file);
    if (!parsed.isObject() || !parsed["cards"].isArray())
        return std::nullopt;

    Deck deck;
    deck.title = parsed["title"].toString();
    deck.createdMs = static_cast<juce::int64>(parsed["createdMs"]);

    for (const auto& v : *parsed["cards"].getArray())
    {
        Card c;
        c.id = v["id"].toString();
        c.question = v["question"].toString();
        c.answer = v["answer"].toString();
        c.context = v["context"].toString();
        c.starred = static_cast<bool>(v["starred"]);
        c.box = static_cast<int>(v["box"]);
        c.dueMs = static_cast<juce::int64>(v["dueMs"]);
        c.lastReviewedMs = static_cast<juce::int64>(v["lastReviewedMs"]);
        c.timesSeen = static_cast<int>(v["timesSeen"]);
        c.timesCorrect = static_cast<int>(v["timesCorrect"]);
        c.timesWrong = static_cast<int>(v["timesWrong"]);
        if (c.id.isEmpty())
            c.id = juce::Uuid().toString();
        deck.cards.push_back(std::move(c));
    }
    return deck;
}

juce::StringArray topicsOf(const Deck& deck)
{
    juce::StringArray topics;
    for (const auto& c : deck.cards)
        if (c.context.isNotEmpty())
            topics.addIfNotAlreadyThere(c.context);
    return topics;
}

DeckLibrary::DeckLibrary(juce::File d) : dir(std::move(d)) {}

juce::File DeckLibrary::defaultDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FlashcardQuiz")
        .getChildFile("decks");
}

std::vector<LibraryEntry> DeckLibrary::list(juce::int64 nowMs) const
{
    std::vector<LibraryEntry> entries;

    for (const auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.json"))
    {
        auto deck = loadDeck(f);
        if (!deck)
            continue;

        LibraryEntry e;
        e.file = f;
        e.title = deck->title;
        e.cardCount = static_cast<int>(deck->cards.size());
        for (const auto& c : deck->cards)
        {
            if (c.box == 0 || c.dueMs <= nowMs) ++e.dueCount;
            if (c.starred) ++e.starredCount;
        }
        entries.push_back(std::move(e));
    }

    std::sort(entries.begin(), entries.end(),
              [](const LibraryEntry& a, const LibraryEntry& b) { return a.title.compareIgnoreCase(b.title) < 0; });
    return entries;
}

juce::File DeckLibrary::fileForTitle(const juce::String& title) const
{
    juce::String slug;
    for (auto c : title.toLowerCase())
        slug += juce::CharacterFunctions::isLetterOrDigit(c) ? juce::String::charToString(c) : "_";
    if (slug.isEmpty())
        slug = "deck";
    return dir.getChildFile(slug + ".json");
}

} // namespace flashcards
