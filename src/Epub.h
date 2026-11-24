// EPUB export. Q page links to A page, A page links to next Q. Works on Kindle.
#pragma once

#include "Deck.h"

namespace flashcards
{

// Write chosen cards (indices into deck.cards) as an EPUB. Empty list = all cards.
bool exportEpub(const Deck& deck, const std::vector<int>& indices, const juce::File& out, juce::String& error);

} // namespace flashcards
