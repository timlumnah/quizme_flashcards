#include "Scheduler.h"

#include <algorithm>
#include <random>

namespace flashcards
{

namespace
{
    constexpr juce::int64 kMsPerDay = 24LL * 60 * 60 * 1000;

    bool matchesFilter(const Card& c, StudyFilter filter, juce::int64 nowMs)
    {
        switch (filter)
        {
            case StudyFilter::All:     return true;
            case StudyFilter::Due:     return isDue(c, nowMs);
            case StudyFilter::Starred: return c.starred;
            case StudyFilter::Missed:  return c.timesWrong > 0 && c.box <= 2;
            case StudyFilter::New:     return c.box == 0;
        }
        return true;
    }
}

bool isDue(const Card& card, juce::int64 nowMs)
{
    return card.box == 0 || card.dueMs <= nowMs;
}

void applyGrade(Card& card, bool correct, juce::int64 nowMs)
{
    ++card.timesSeen;
    card.lastReviewedMs = nowMs;

    if (correct)
    {
        ++card.timesCorrect;
        // new card gets a head start, others climb one box
        card.box = card.box == 0 ? 2 : std::min(card.box + 1, kMaxBox);
    }
    else
    {
        ++card.timesWrong;
        card.box = 1;
    }
    card.dueMs = nowMs + kIntervalDays[card.box] * kMsPerDay;
}

std::vector<int> buildQueue(const Deck& deck, const StudyOptions& options, juce::int64 nowMs)
{
    std::vector<int> queue;

    for (int i = 0; i < static_cast<int>(deck.cards.size()); ++i)
    {
        const auto& c = deck.cards[static_cast<size_t>(i)];
        if (options.topic.isNotEmpty() && c.context != options.topic)
            continue;
        if (matchesFilter(c, options.filter, nowMs))
            queue.push_back(i);
    }

    if (options.shuffle)
    {
        std::mt19937 rng{std::random_device{}()};
        std::shuffle(queue.begin(), queue.end(), rng);
    }
    else if (options.filter == StudyFilter::Due)
    {
        // most overdue first, stable so ties keep deck order
        std::stable_sort(queue.begin(), queue.end(), [&](int a, int b)
                         { return deck.cards[static_cast<size_t>(a)].dueMs < deck.cards[static_cast<size_t>(b)].dueMs; });
    }

    if (options.limit > 0 && static_cast<int>(queue.size()) > options.limit)
        queue.resize(static_cast<size_t>(options.limit));

    return queue;
}

} // namespace flashcards
