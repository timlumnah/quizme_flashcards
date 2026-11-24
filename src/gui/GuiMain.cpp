// Quiz Me Flashcards GUI. One window, four views:
//   Library -> Setup -> Study -> Summary -> (back to Setup)
// Engine does all the real work (Deck, Scheduler, Epub). GUI only wires it up.

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Deck.h"
#include "../Epub.h"
#include "../Scheduler.h"
#include <BinaryData.h>
#include <map>
#include <vector>

using namespace flashcards;

namespace
{
    const juce::Colour kBg(0xff1e1f24), kPanel(0xff2a2c33), kText(0xffe8e8ea), kDim(0xff9a9ca5), kAccent(0xff4f8cff);

    // Big wrapped text box for question / answer.
    // Question side gets a faint grid of ? marks. Odd rows shift half a step.
    class CardView : public juce::Component
    {
    public:
        void setText(const juce::String& t, bool isAnswer)
        {
            text = t;
            answer = isAnswer;
            repaint();
        }
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour(kPanel);
            g.fillRoundedRectangle(r, 12.0f);

            if (!answer)
            {
                juce::Path shape;
                shape.addRoundedRectangle(r, 12.0f);
                g.saveState();
                g.reduceClipRegion(shape);
                g.setColour(kText.withAlpha(0.07f));
                g.setFont(juce::FontOptions(24.0f));
                const float stepX = 64.0f, stepY = 46.0f;
                int row = 0;
                for (float y = stepY / 2; y < r.getHeight() + stepY; y += stepY, ++row)
                    for (float x = (row % 2 == 0 ? stepX / 2 : 0.0f); x < r.getWidth() + stepX; x += stepX)
                        g.drawText("?", juce::Rectangle<float>(x - 14.0f, y - 16.0f, 28.0f, 32.0f), juce::Justification::centred, false);
                g.restoreState();
            }

            g.setColour(answer ? kAccent : kDim);
            g.drawRoundedRectangle(r.reduced(1.0f), 12.0f, 2.0f);
            g.setColour(kText);
            g.setFont(juce::FontOptions(26.0f));
            g.drawFittedText(text, getLocalBounds().reduced(28), juce::Justification::centred, 12, 1.0f);
        }

    private:
        juce::String text;
        bool answer = false;
    };

    class MainComponent : public juce::Component, private juce::ListBoxModel
    {
    public:
        MainComponent() : library(DeckLibrary::defaultDirectory())
        {
            setSize(900, 620);
            setWantsKeyboardFocus(true);

            // library view
            for (auto* c : std::initializer_list<juce::Component*>{&libTitle, &deckList, &importBtn, &openBtn, &exportBtn, &deleteBtn, &libHint})
                addChildComponent(c);
            libTitle.setText("Quiz Me Flashcards", juce::dontSendNotification);
            libTitle.setFont(juce::FontOptions(28.0f, juce::Font::bold));
            libHint.setText("Import a CSV with Question and Answer columns (optional Context column for topics).", juce::dontSendNotification);
            libHint.setColour(juce::Label::textColourId, kDim);
            deckList.setModel(this);
            deckList.setRowHeight(36);
            importBtn.setButtonText("Import CSV...");
            openBtn.setButtonText("Study");
            exportBtn.setButtonText("Export EPUB...");
            deleteBtn.setButtonText("Delete");
            importBtn.onClick = [this] { importCsv(); };
            openBtn.onClick = [this] { openSelected(); };
            exportBtn.onClick = [this] { exportSelected(); };
            deleteBtn.onClick = [this] { deleteSelected(); };

            // setup view
            for (auto* c : std::initializer_list<juce::Component*>{&setTitle, &filterBox, &topicBox, &sizeBox, &shuffleToggle, &setInfo, &startBtn, &setBackBtn, &setExportBtn, &clearStarsBtn,
                                                                    &filterLbl, &topicLbl, &sizeLbl})
                addChildComponent(c);
            setTitle.setFont(juce::FontOptions(24.0f, juce::Font::bold));
            filterLbl.setText("Which cards", juce::dontSendNotification);
            topicLbl.setText("Topic", juce::dontSendNotification);
            sizeLbl.setText("Session size", juce::dontSendNotification);
            filterBox.addItem("All cards", 1);
            filterBox.addItem("Due now (spaced repetition)", 2);
            filterBox.addItem("Starred only", 3);
            filterBox.addItem("Previously wrong", 4);
            filterBox.addItem("New (never seen)", 5);
            filterBox.setSelectedId(2, juce::dontSendNotification);
            sizeBox.addItem("Everything", 1);
            sizeBox.addItem("10 cards", 2);
            sizeBox.addItem("20 cards", 3);
            sizeBox.addItem("50 cards", 4);
            sizeBox.setSelectedId(1, juce::dontSendNotification);
            shuffleToggle.setButtonText("Shuffle order");
            shuffleToggle.setToggleState(true, juce::dontSendNotification);
            startBtn.setButtonText("Start");
            setBackBtn.setButtonText("Back");
            setExportBtn.setButtonText("Export these cards as EPUB...");
            clearStarsBtn.setButtonText("Clear all stars");
            filterBox.onChange = topicBox.onChange = sizeBox.onChange = [this] { refreshSetupInfo(); };
            shuffleToggle.onClick = [this] { refreshSetupInfo(); };
            startBtn.onClick = [this] { startSession(); };
            setBackBtn.onClick = [this] { showLibrary(); };
            setExportBtn.onClick = [this] { exportCurrentSelection(); };
            clearStarsBtn.onClick = [this]
            {
                for (auto& c : deck.cards) c.starred = false;
                saveDeck(deck, deckFile);
                refreshSetupInfo();
            };

            // study view
            for (auto* c : std::initializer_list<juce::Component*>{&progressLbl, &contextLbl, &cardView, &starBtn, &revealBtn, &missBtn, &gotBtn, &endBtn, &exitBtn, &backArrow, &fwdArrow})
                addChildComponent(c);
            progressLbl.setColour(juce::Label::textColourId, kDim);
            contextLbl.setColour(juce::Label::textColourId, kDim);
            contextLbl.setJustificationType(juce::Justification::centredRight);
            revealBtn.setButtonText("Continue (space)");
            missBtn.setButtonText("Wrong (1)");
            gotBtn.setButtonText("Correct (2)");
            endBtn.setButtonText("End session (esc)");
            exitBtn.setButtonText("X");
            // Wrong is the bright red. End/X are the same hue, darker, so they read as related but quieter.
            missBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb23a3a));
            endBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7a2f2f));
            exitBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7a2f2f));
            gotBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2f7a4a));
            revealBtn.onClick = [this] { forward(); };
            backArrow.onClick = [this] { back(); };
            fwdArrow.onClick = [this] { forward(); };
            missBtn.onClick = [this] { grade(false); };
            gotBtn.onClick = [this] { grade(true); };
            starBtn.onClick = [this] { toggleStar(); };
            endBtn.onClick = [this] { endSession(); };
            exitBtn.onClick = [this] { endSession(); };

            // summary view
            addChildComponent(sumLbl);
            addChildComponent(sumBackBtn);
            sumLbl.setJustificationType(juce::Justification::centred);
            sumLbl.setFont(juce::FontOptions(26.0f));
            sumBackBtn.setButtonText("Back to deck");
            sumBackBtn.onClick = [this] { showSetup(); };

            for (auto* b : std::initializer_list<juce::Button*>{&importBtn, &openBtn, &exportBtn, &deleteBtn, &startBtn, &setBackBtn, &setExportBtn, &clearStarsBtn,
                                                                 &starBtn, &revealBtn, &missBtn, &gotBtn, &endBtn, &exitBtn, &backArrow, &fwdArrow, &sumBackBtn, &shuffleToggle})
                b->setWantsKeyboardFocus(false);

            showLibrary();
        }

        void paint(juce::Graphics& g) override { g.fillAll(kBg); }

        void resized() override
        {
            auto r = getLocalBounds().reduced(24);

            // library
            {
                auto a = r;
                libTitle.setBounds(a.removeFromTop(44));
                libHint.setBounds(a.removeFromTop(26));
                a.removeFromTop(8);
                auto row = a.removeFromBottom(40);
                a.removeFromBottom(10);
                deckList.setBounds(a);
                importBtn.setBounds(row.removeFromLeft(140));
                row.removeFromLeft(8);
                exportBtn.setBounds(row.removeFromLeft(140));
                row.removeFromLeft(8);
                deleteBtn.setBounds(row.removeFromLeft(100));
                openBtn.setBounds(row.removeFromRight(140));
            }
            // setup
            {
                auto a = r;
                setTitle.setBounds(a.removeFromTop(44));
                a.removeFromTop(12);
                auto row = [&](juce::Label& l, juce::Component& c)
                {
                    auto line = a.removeFromTop(32);
                    l.setBounds(line.removeFromLeft(130));
                    c.setBounds(line.removeFromLeft(320));
                    a.removeFromTop(10);
                };
                row(filterLbl, filterBox);
                row(topicLbl, topicBox);
                row(sizeLbl, sizeBox);
                shuffleToggle.setBounds(a.removeFromTop(30).withTrimmedLeft(130));
                a.removeFromTop(14);
                setInfo.setBounds(a.removeFromTop(60));
                auto bottom = a.removeFromBottom(40);
                setBackBtn.setBounds(bottom.removeFromLeft(100));
                bottom.removeFromLeft(8);
                setExportBtn.setBounds(bottom.removeFromLeft(240));
                bottom.removeFromLeft(8);
                clearStarsBtn.setBounds(bottom.removeFromLeft(140));
                startBtn.setBounds(bottom.removeFromRight(160));
            }
            // study
            {
                auto a = r;
                auto top = a.removeFromTop(34);
                exitBtn.setBounds(top.removeFromRight(40));
                progressLbl.setBounds(top.removeFromLeft(top.getWidth() * 2 / 3));
                contextLbl.setBounds(top);
                a.removeFromTop(8);
                auto bottom = a.removeFromBottom(52);
                a.removeFromBottom(12);
                backArrow.setBounds(a.removeFromLeft(56).withSizeKeepingCentre(26, 46));
                fwdArrow.setBounds(a.removeFromRight(56).withSizeKeepingCentre(26, 46));
                a.reduce(8, 0);
                cardView.setBounds(a);
                starBtn.setBounds(bottom.removeFromLeft(110));
                bottom.removeFromLeft(8);
                endBtn.setBounds(bottom.removeFromLeft(150));
                gotBtn.setBounds(bottom.removeFromRight(150));
                bottom.removeFromRight(8);
                missBtn.setBounds(bottom.removeFromRight(150));
                revealBtn.setBounds(bottom.reduced(8, 0));
            }
            // summary
            {
                auto a = r;
                sumLbl.setBounds(a.removeFromTop(a.getHeight() / 2 + 40));
                sumBackBtn.setBounds(a.removeFromTop(44).withSizeKeepingCentre(200, 44));
            }
        }

        bool keyPressed(const juce::KeyPress& k) override
        {
            if (view != View::Study)
                return false;
            if (k == juce::KeyPress::escapeKey) { endSession(); return true; }
            if (k.getTextCharacter() == 's' || k.getTextCharacter() == 'S') { toggleStar(); return true; }
            if (k == juce::KeyPress::spaceKey || k == juce::KeyPress::returnKey || k == juce::KeyPress::rightKey) { forward(); return true; }
            if (k == juce::KeyPress::backspaceKey || k == juce::KeyPress::leftKey) { back(); return true; }
            if (k.getTextCharacter() == '1') { grade(false); return true; }
            if (k.getTextCharacter() == '2') { grade(true); return true; }
            return false;
        }

    private:
        enum class View { Library, Setup, Study, Summary };

        // ---------- list model ----------
        int getNumRows() override { return static_cast<int>(entries.size()); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (row < 0 || row >= getNumRows())
                return;
            const auto& e = entries[static_cast<size_t>(row)];
            if (selected) { g.setColour(kAccent.withAlpha(0.35f)); g.fillRect(0, 0, w, h); }
            g.setColour(kText);
            g.setFont(juce::FontOptions(17.0f));
            g.drawText(e.title, 12, 0, w / 2, h, juce::Justification::centredLeft);
            g.setColour(kDim);
            g.setFont(juce::FontOptions(14.0f));
            g.drawText(juce::String(e.cardCount) + " cards   " + juce::String(e.dueCount) + " due   " + juce::String(e.starredCount) + " starred",
                       w / 2, 0, w / 2 - 12, h, juce::Justification::centredRight);
        }
        void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override { openSelected(); }

        // ---------- view switching ----------
        void setView(View v)
        {
            view = v;
            for (auto* c : std::initializer_list<juce::Component*>{&libTitle, &deckList, &importBtn, &openBtn, &exportBtn, &deleteBtn, &libHint})
                c->setVisible(v == View::Library);
            for (auto* c : std::initializer_list<juce::Component*>{&setTitle, &filterBox, &topicBox, &sizeBox, &shuffleToggle, &setInfo, &startBtn, &setBackBtn, &setExportBtn,
                                                                    &clearStarsBtn, &filterLbl, &topicLbl, &sizeLbl})
                c->setVisible(v == View::Setup);
            for (auto* c : std::initializer_list<juce::Component*>{&progressLbl, &contextLbl, &cardView, &starBtn, &revealBtn, &missBtn, &gotBtn, &endBtn, &exitBtn, &backArrow, &fwdArrow})
                c->setVisible(v == View::Study);
            sumLbl.setVisible(v == View::Summary);
            sumBackBtn.setVisible(v == View::Summary);
            grabKeyboardFocus();
        }

        void showLibrary()
        {
            entries = library.list(juce::Time::currentTimeMillis());
            deckList.updateContent();
            deckList.repaint();
            if (!entries.empty() && deckList.getSelectedRow() < 0)
                deckList.selectRow(0);
            setView(View::Library);
        }

        void showSetup()
        {
            setTitle.setText(deck.title, juce::dontSendNotification);
            topicBox.clear(juce::dontSendNotification);
            topicBox.addItem("All topics", 1);
            int id = 2;
            for (auto& t : topicsOf(deck))
                topicBox.addItem(t, id++);
            topicBox.setSelectedId(1, juce::dontSendNotification);
            // if nothing is due, default to All so Start is not empty
            StudyOptions dueOnly;
            dueOnly.filter = StudyFilter::Due;
            filterBox.setSelectedId(buildQueue(deck, dueOnly, juce::Time::currentTimeMillis()).empty() ? 1 : 2, juce::dontSendNotification);
            refreshSetupInfo();
            setView(View::Setup);
        }

        // ---------- library actions ----------
        bool loadSelected()
        {
            int row = deckList.getSelectedRow();
            if (row < 0 || row >= getNumRows())
                return false;
            auto loaded = loadDeck(entries[static_cast<size_t>(row)].file);
            if (!loaded)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Load failed", "Could not read that deck file.");
                return false;
            }
            deck = std::move(*loaded);
            deckFile = entries[static_cast<size_t>(row)].file;
            return true;
        }

        void openSelected()
        {
            if (loadSelected())
                showSetup();
        }

        void exportSelected()
        {
            if (loadSelected())
                askExport(deck, {});
        }

        void deleteSelected()
        {
            int row = deckList.getSelectedRow();
            if (row < 0 || row >= getNumRows())
                return;
            auto entry = entries[static_cast<size_t>(row)];
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "Delete deck",
                                               "Delete \"" + entry.title + "\" and all its progress? Your original CSV is not touched.", "Delete", "Cancel", this,
                                               juce::ModalCallbackFunction::create(
                                                   [this, entry](int result)
                                                   {
                                                       if (result == 1)
                                                       {
                                                           entry.file.deleteFile();
                                                           showLibrary();
                                                       }
                                                   }));
        }

        void importCsv()
        {
            chooser = std::make_unique<juce::FileChooser>("Choose a CSV of questions and answers", importStartDir(), "*.csv");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this](const juce::FileChooser& fc)
                                 {
                                     auto f = fc.getResult();
                                     if (f == juce::File())
                                         return;
                                     juce::String err;
                                     lastImportDir = f.getParentDirectory();
                                     auto incoming = deckFromCsv(f, err);
                                     if (!incoming)
                                     {
                                         juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Import failed", err);
                                         return;
                                     }
                                     auto target = library.fileForTitle(incoming->title);
                                     juce::String msg;
                                     if (auto existing = target.existsAsFile() ? loadDeck(target) : std::nullopt)
                                     {
                                         auto m = mergeCards(*existing, *incoming);
                                         saveDeck(*existing, target);
                                         msg = "Updated existing deck: " + juce::String(m.added) + " new cards, " + juce::String(m.kept) + " kept with progress.";
                                     }
                                     else
                                     {
                                         saveDeck(*incoming, target);
                                         msg = "Imported " + juce::String(static_cast<int>(incoming->cards.size())) + " cards.";
                                     }
                                     showLibrary();
                                     juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Import done", msg);
                                 });
        }

        void askExport(const Deck& d, std::vector<int> indices)
        {
            auto name = d.title.replaceCharacter(' ', '_') + ".epub";
            chooser = std::make_unique<juce::FileChooser>("Save EPUB", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(name), "*.epub");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                 [this, d, indices](const juce::FileChooser& fc)
                                 {
                                     auto f = fc.getResult();
                                     if (f == juce::File())
                                         return;
                                     juce::String err;
                                     bool ok = exportEpub(d, indices, f.withFileExtension("epub"), err);
                                     juce::AlertWindow::showMessageBoxAsync(ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                                                                            ok ? "EPUB saved" : "Export failed", ok ? f.getFullPathName() : err);
                                 });
        }

        // ---------- setup ----------
        StudyOptions optionsFromUi() const
        {
            StudyOptions o;
            static const StudyFilter filters[] = {StudyFilter::All, StudyFilter::Due, StudyFilter::Starred, StudyFilter::Missed, StudyFilter::New};
            o.filter = filters[std::max(1, filterBox.getSelectedId()) - 1];
            o.topic = topicBox.getSelectedId() > 1 ? topicBox.getText() : juce::String();
            o.shuffle = shuffleToggle.getToggleState();
            static const int limits[] = {0, 10, 20, 50};
            o.limit = limits[std::max(1, sizeBox.getSelectedId()) - 1];
            return o;
        }

        void refreshSetupInfo()
        {
            auto now = juce::Time::currentTimeMillis();
            auto n = buildQueue(deck, optionsFromUi(), now).size();
            int mastered = 0;
            for (auto& c : deck.cards) if (c.box >= kMaxBox) ++mastered;
            setInfo.setText(juce::String(static_cast<int>(n)) + " cards in this session.   Deck: " + juce::String(static_cast<int>(deck.cards.size())) + " cards, "
                                + juce::String(mastered) + " in top box.",
                            juce::dontSendNotification);
            startBtn.setEnabled(n > 0);
        }

        void exportCurrentSelection()
        {
            auto o = optionsFromUi();
            o.shuffle = false;
            o.limit = 0;
            askExport(deck, buildQueue(deck, o, juce::Time::currentTimeMillis()));
        }

        // ---------- study ----------
        // Session is a timeline, same as the EPUB: card 1 question, card 1 answer, card 2 question...
        // Forward (space, right arrow) steps along it. Back (backspace, left arrow) steps back.
        // Wrong/Correct grade the card and jump to the next question.
        void startSession()
        {
            auto q = buildQueue(deck, optionsFromUi(), juce::Time::currentTimeMillis());
            if (q.empty())
                return;
            order = q;
            pos = 0;
            revealed = false;
            graded.clear();
            sessionTotal = static_cast<int>(q.size());
            gotCount = wrongCount = 0;
            setView(View::Study);
            showCard();
        }

        bool atEnd() const { return pos >= static_cast<int>(order.size()); }
        Card& current() { return deck.cards[static_cast<size_t>(order[static_cast<size_t>(pos)])]; }

        void showCard()
        {
            if (atEnd())
            {
                finishSession();
                return;
            }
            auto& c = current();
            cardView.setText(revealed ? c.answer : c.question, revealed);
            contextLbl.setText(c.context, juce::dontSendNotification);

            juce::String info = pos < sessionTotal ? "Card " + juce::String(pos + 1) + " of " + juce::String(sessionTotal) : juce::String("Retry");
            info += "   |   box " + juce::String(c.box);
            auto g = graded.find(order[static_cast<size_t>(pos)]);
            if (revealed && g != graded.end())
                info += g->second ? "   |   marked correct" : "   |   marked wrong";
            progressLbl.setText(info, juce::dontSendNotification);

            starBtn.setButtonText(c.starred ? "Starred (s)" : "Star (s)");
            missBtn.setEnabled(revealed);
            gotBtn.setEnabled(revealed);
            backArrow.setEnabled(pos > 0 || revealed);
        }

        void forward()
        {
            if (atEnd())
                return;
            if (!revealed)
                revealed = true;
            else
            {
                ++pos;
                revealed = false;
            }
            showCard();
        }

        void back()
        {
            if (revealed)
                revealed = false;
            else if (pos > 0)
            {
                --pos;
                revealed = true;
            }
            else
                return;
            showCard();
        }

        void grade(bool correct)
        {
            if (atEnd() || !revealed)
                return;
            int idx = order[static_cast<size_t>(pos)];

            // only first try of a card counts for scheduling. later passes are practice.
            if (graded.find(idx) == graded.end())
            {
                graded[idx] = correct;
                applyGrade(deck.cards[static_cast<size_t>(idx)], correct, juce::Time::currentTimeMillis());
                correct ? ++gotCount : ++wrongCount;
                saveDeck(deck, deckFile);
                if (!correct)
                    order.push_back(idx);
            }
            ++pos;
            revealed = false;
            showCard();
        }

        void toggleStar()
        {
            if (atEnd())
                return;
            auto& c = current();
            c.starred = !c.starred;
            starBtn.setButtonText(c.starred ? "Starred (s)" : "Star (s)");
            saveDeck(deck, deckFile);
        }

        void endSession() { finishSession(); }

        void finishSession()
        {
            saveDeck(deck, deckFile);
            int notGraded = sessionTotal - static_cast<int>(graded.size());
            sumLbl.setText("Session done\n\nCorrect on first try: " + juce::String(gotCount) + "\nWrong: " + juce::String(wrongCount)
                               + "\nNot graded: " + juce::String(notGraded),
                           juce::dontSendNotification);
            setView(View::Summary);
        }

        // Import dialog starts in sample_decks (next to the app, or in the repo when run from build/).
        juce::File importStartDir() const
        {
            if (lastImportDir.isDirectory())
                return lastImportDir;
            auto dir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
            for (int i = 0; i < 6; ++i)
            {
                auto candidate = dir.getChildFile("sample_decks");
                if (candidate.isDirectory())
                    return candidate;
                dir = dir.getParentDirectory();
            }
            return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        }

        DeckLibrary library;
        std::vector<LibraryEntry> entries;
        Deck deck;
        juce::File deckFile;
        View view = View::Library;

        std::vector<int> order;
        int pos = 0;
        std::map<int, bool> graded;
        int sessionTotal = 0, gotCount = 0, wrongCount = 0;
        bool revealed = false;
        juce::File lastImportDir;

        std::unique_ptr<juce::FileChooser> chooser;

        juce::Label libTitle, libHint;
        juce::ListBox deckList;
        juce::TextButton importBtn, openBtn, exportBtn, deleteBtn;

        juce::Label setTitle, setInfo, filterLbl, topicLbl, sizeLbl;
        juce::ComboBox filterBox, topicBox, sizeBox;
        juce::ToggleButton shuffleToggle;
        juce::TextButton startBtn, setBackBtn, setExportBtn, clearStarsBtn;

        juce::Label progressLbl, contextLbl;
        CardView cardView;
        juce::TextButton starBtn, revealBtn, missBtn, gotBtn, endBtn, exitBtn;
        juce::ArrowButton backArrow{"back", 0.5f, kDim}, fwdArrow{"forward", 0.0f, kDim};

        juce::Label sumLbl;
        juce::TextButton sumBackBtn;
    };
}

class FlashcardApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Quiz Me Flashcards"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    void initialise(const juce::String&) override { window = std::make_unique<Window>(getApplicationName()); }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class Window : public juce::DocumentWindow
    {
    public:
        explicit Window(const juce::String& name) : DocumentWindow(name, kBg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, false);
            setResizeLimits(720, 520, 2000, 1400);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            auto icon = juce::ImageCache::getFromMemory(BinaryData::icon_256_png, BinaryData::icon_256_pngSize);
            if (icon.isValid())
                setIcon(icon);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION(FlashcardApp)
