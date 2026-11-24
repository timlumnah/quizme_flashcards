// Engine tests. Plain asserts, no framework. Exit code 0 = all pass.
#include "Deck.h"
#include "Epub.h"
#include "Scheduler.h"
#include <iostream>

using namespace flashcards;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

int main()
{
    // csv parser: quotes, escaped quote, embedded newline, CRLF, BOM
    auto rows = parseCsv(juce::String::fromUTF8("\xEF\xBB\xBF" "Question,Answer\r\n\"a, b\",\"say \"\"hi\"\"\"\r\nq2,\"line1\nline2\"\r\n"));
    CHECK(rows.size() == 3);
    CHECK(rows[1][0] == "a, b");
    CHECK(rows[1][1] == "say \"hi\"");
    CHECK(rows[2][1] == "line1\nline2");

    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("flashcard_engine_test");
    dir.deleteRecursively();
    dir.createDirectory();

    // import real sample csvs
    juce::String err;
    auto csv = juce::File(juce::String(SAMPLE_DIR)).getChildFile("deck_001_ai_foundations.csv");
    auto deck = deckFromCsv(csv, err);
    CHECK(deck.has_value());
    if (!deck) return 1;
    CHECK(deck->cards.size() > 50);
    CHECK(topicsOf(*deck).size() > 1);

    auto net = deckFromCsv(juce::File(juce::String(SAMPLE_DIR)).getChildFile("networking_flashcards.csv"), err);
    CHECK(net.has_value() && net->cards.size() > 50);
    CHECK(topicsOf(*net).isEmpty());

    // scheduler
    auto now = juce::Time::currentTimeMillis();
    auto& c0 = deck->cards[0];
    CHECK(isDue(c0, now));
    applyGrade(c0, true, now);
    CHECK(c0.box == 2 && !isDue(c0, now));
    applyGrade(c0, false, now);
    CHECK(c0.box == 1 && isDue(c0, now) && c0.timesWrong == 1);
    for (int i = 0; i < 10; ++i) applyGrade(c0, true, now);
    CHECK(c0.box == kMaxBox);

    deck->cards[1].starred = true;
    StudyOptions o;
    o.shuffle = false;
    o.filter = StudyFilter::Starred;
    CHECK(buildQueue(*deck, o, now) == std::vector<int>{1});
    o.filter = StudyFilter::Due;
    CHECK(buildQueue(*deck, o, now).size() == deck->cards.size() - 1);
    o.filter = StudyFilter::All; o.limit = 5;
    CHECK(buildQueue(*deck, o, now).size() == 5);
    o.limit = 0; o.topic = topicsOf(*deck)[0];
    CHECK(!buildQueue(*deck, o, now).empty() && buildQueue(*deck, o, now).size() < deck->cards.size());

    // save/load round trip keeps progress
    auto file = dir.getChildFile("deck.json");
    CHECK(saveDeck(*deck, file));
    auto back = loadDeck(file);
    CHECK(back.has_value() && back->cards.size() == deck->cards.size());
    CHECK(back->cards[0].box == kMaxBox && back->cards[1].starred && back->cards[0].timesWrong == 1);

    // merge: same csv again adds nothing, keeps progress
    auto again = deckFromCsv(csv, err);
    auto merged = mergeCards(*back, *again);
    CHECK(merged.added == 0 && merged.kept == static_cast<int>(again->cards.size()));
    CHECK(back->cards[0].box == kMaxBox);

    // library listing
    DeckLibrary lib(dir);
    auto entries = lib.list(now);
    CHECK(entries.size() == 1 && entries[0].starredCount == 1);

    // epub: valid zip, mimetype first, expected entries
    auto epub = dir.getChildFile("out.epub");
    CHECK(exportEpub(*deck, {}, epub, err));
    juce::ZipFile zip(epub);
    CHECK(zip.getNumEntries() == 6 + 2 * static_cast<int>(deck->cards.size()));
    CHECK(zip.getEntry(0)->filename == "mimetype");
    juce::ZipFile::ZipEntry* e = nullptr;
    e = const_cast<juce::ZipFile::ZipEntry*>(zip.getEntry("OEBPS/q001.xhtml"));
    CHECK(e != nullptr);
    std::unique_ptr<juce::InputStream> in(zip.createStreamForEntry(*zip.getEntry("OEBPS/q001.xhtml")));
    CHECK(in != nullptr && in->readEntireStreamAsString().contains(deck->cards[0].question.substring(0, 10)));

    CHECK(exportEpub(*deck, {3, 1}, dir.getChildFile("subset.epub"), err));
    juce::ZipFile sub(dir.getChildFile("subset.epub"));
    CHECK(sub.getNumEntries() == 6 + 4);

    // every bundled sample deck must import with cards and a topic or none, never an error
    for (auto& f : juce::File(juce::String(SAMPLE_DIR)).findChildFiles(juce::File::findFiles, false, "*.csv"))
    {
        auto d = deckFromCsv(f, err);
        CHECK(d.has_value());
        if (d) std::cout << f.getFileName() << ": " << d->cards.size() << " cards\n";
    }

    std::cout << (failures == 0 ? "ALL PASS\n" : "FAILURES\n");
    dir.deleteRecursively();
    return failures == 0 ? 0 : 1;
}
