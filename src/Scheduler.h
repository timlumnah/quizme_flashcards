// Leitner box scheduler and study-queue builder.
// Box 1 = missed or just learned, box 5 = well known.
#pragma once

#include "Deck.h"

namespace flashcards
{

inline constexpr int kMaxBox = 5;

// Days until next review, indexed by box. Box 1 is due again right away.
inline constexpr int kIntervalDays[kMaxBox + 1] = {0, 0, 1, 3, 7, 21};

enum class StudyFilter
{
    All,
    Due,      // box 0 or past due date
    Starred,
    Missed,   // missed before and still in box 1-2
    New       // never seen
};

struct StudyOptions
{
    StudyFilter filter = StudyFilter::All;
    juce::String topic;      // empty = every topic
    bool shuffle = true;
    int limit = 0;           // 0 = no cap
};

bool isDue(const Card& card, juce::int64 nowMs);

// Record one graded answer. Right: up one box (new card jumps to box 2). Wrong: back to box 1.
void applyGrade(Card& card, bool correct, juce::int64 nowMs);

// Card indices for a session. Shuffle off: deck order, except Due sorts most overdue first.
std::vector<int> buildQueue(const Deck& deck, const StudyOptions& options, juce::int64 nowMs);

} // namespace flashcards
