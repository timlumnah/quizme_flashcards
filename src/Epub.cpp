#include "Epub.h"

namespace flashcards
{

namespace
{
    juce::String esc(const juce::String& s)
    {
        return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;");
    }

    juce::String page(const juce::String& title, const juce::String& body)
    {
        return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
               "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n<head>\n  <title>" + esc(title) + "</title>\n"
               "  <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"/>\n</head>\n<body>\n"
               + body + "\n</body>\n</html>\n";
    }

    juce::String pad(int n) { return juce::String(n).paddedLeft('0', 3); }

    const char* kStyle =
        "body { font-family: serif; line-height: 1.45; margin: 1em; }\n"
        "h1, h2 { margin-top: 1em; }\n"
        ".card { margin-top: 2em; }\n"
        ".context { font-size: 0.9em; opacity: 0.75; }\n"
        ".answer { font-size: 1.2em; font-weight: bold; }\n"
        ".nav { margin-top: 2em; }\n"
        "a { text-decoration: underline; }\n";
}

bool exportEpub(const Deck& deck, const std::vector<int>& indicesIn, const juce::File& out, juce::String& error)
{
    std::vector<int> indices = indicesIn;
    if (indices.empty())
        for (int i = 0; i < static_cast<int>(deck.cards.size()); ++i)
            indices.push_back(i);

    if (indices.empty())
    {
        error = "Deck has no cards.";
        return false;
    }

    juce::ZipFile::Builder zip;
    auto add = [&](const juce::String& path, const juce::String& text, int level)
    {
        zip.addEntry(std::make_unique<juce::MemoryInputStream>(text.toRawUTF8(), text.getNumBytesAsUTF8(), true),
                     level, path, juce::Time::getCurrentTime());
    };

    // mimetype first, stored not compressed. EPUB readers insist.
    add("mimetype", "application/epub+zip", 0);

    add("META-INF/container.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
        "  <rootfiles>\n    <rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
        "  </rootfiles>\n</container>\n", 9);

    add("OEBPS/style.css", kStyle, 9);

    const int total = static_cast<int>(indices.size());
    add("OEBPS/title.xhtml",
        page(deck.title,
             "<h1>" + esc(deck.title) + "</h1><p>" + juce::String(total) + " cards</p>"
             "<p><a href=\"q001.xhtml\">Start deck</a></p>"), 9);

    juce::String manifest = "<item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>"
                            "<item id=\"style\" href=\"style.css\" media-type=\"text/css\"/>"
                            "<item id=\"title\" href=\"title.xhtml\" media-type=\"application/xhtml+xml\"/>";
    juce::String spine = "<itemref idref=\"title\"/>";
    juce::String navPoints = "<navPoint id=\"np0\" playOrder=\"1\"><navLabel><text>" + esc(deck.title)
                             + "</text></navLabel><content src=\"title.xhtml\"/></navPoint>";

    for (int n = 1; n <= total; ++n)
    {
        const auto& card = deck.cards[static_cast<size_t>(indices[static_cast<size_t>(n - 1)])];
        const juce::String qFile = "q" + pad(n) + ".xhtml";
        const juce::String aFile = "a" + pad(n) + ".xhtml";
        const juce::String nextQ = n < total ? "q" + pad(n + 1) + ".xhtml" : juce::String("title.xhtml");
        const juce::String ctx = card.context.isNotEmpty() ? "<p class=\"context\">" + esc(card.context) + "</p>" : "";

        add("OEBPS/" + qFile,
            page("Question " + juce::String(n),
                 "<div class=\"card\">" + ctx + "<h2>Question " + juce::String(n) + "</h2><p>" + esc(card.question)
                 + "</p><p class=\"nav\"><a href=\"" + aFile + "\">Show answer</a></p></div>"), 9);
        add("OEBPS/" + aFile,
            page("Answer " + juce::String(n),
                 "<div class=\"card\">" + ctx + "<h2>Answer " + juce::String(n) + "</h2><p class=\"answer\">" + esc(card.answer)
                 + "</p><p class=\"nav\"><a href=\"" + qFile + "\">Back to question</a><br/><a href=\"" + nextQ
                 + "\">Next question</a></p></div>"), 9);

        manifest += "<item id=\"q" + pad(n) + "\" href=\"" + qFile + "\" media-type=\"application/xhtml+xml\"/>"
                    "<item id=\"a" + pad(n) + "\" href=\"" + aFile + "\" media-type=\"application/xhtml+xml\"/>";
        spine += "<itemref idref=\"q" + pad(n) + "\"/><itemref idref=\"a" + pad(n) + "\"/>";
        navPoints += "<navPoint id=\"np" + juce::String(n) + "\" playOrder=\"" + juce::String(n + 1)
                     + "\"><navLabel><text>Question " + juce::String(n) + "</text></navLabel><content src=\"" + qFile
                     + "\"/></navPoint>";
    }

    const juce::String bookId = juce::Uuid().toString();

    add("OEBPS/toc.ncx",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\">\n"
        "<head><meta name=\"dtb:uid\" content=\"" + bookId + "\"/></head>\n"
        "<docTitle><text>" + esc(deck.title) + "</text></docTitle>\n<navMap>" + navPoints + "</navMap>\n</ncx>\n", 9);

    add("OEBPS/content.opf",
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<package xmlns=\"http://www.idpf.org/2007/opf\" unique-identifier=\"BookId\" version=\"2.0\">\n"
        "  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
        "    <dc:title>" + esc(deck.title) + "</dc:title>\n    <dc:language>en</dc:language>\n"
        "    <dc:identifier id=\"BookId\">" + bookId + "</dc:identifier>\n"
        "    <dc:creator>Flashcard Quiz</dc:creator>\n  </metadata>\n"
        "  <manifest>" + manifest + "</manifest>\n  <spine toc=\"ncx\">" + spine + "</spine>\n</package>\n", 9);

    out.deleteFile();
    juce::FileOutputStream stream(out);
    if (stream.failedToOpen())
    {
        error = "Cannot write " + out.getFullPathName();
        return false;
    }
    if (!zip.writeToStream(stream, nullptr))
    {
        error = "Zip write failed.";
        return false;
    }
    return true;
}

} // namespace flashcards
