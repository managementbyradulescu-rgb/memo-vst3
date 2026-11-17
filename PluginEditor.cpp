#include "PluginEditor.h"
#include "PromptParser.h"

using namespace memo_ui;

static juce::Font mkFont (float h, int style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions (h, style));
}

//==============================================================================
PinkLook::PinkLook()
{
    setColour (juce::TextButton::buttonColourId, pink);
    setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    setColour (juce::Label::textColourId, ink);
}

void PinkLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                 float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, endAngle, true);
    g.setColour (cream2);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, angle, true);
    g.setColour (pinkDark);
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r * 0.76f;
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillEllipse (c.x - kr, c.y - kr + 3.f, kr * 2.f, kr * 2.f);

    g.setGradientFill (juce::ColourGradient (pinkSoft, c.x - kr * 0.4f, c.y - kr * 0.6f,
                                             pink, c.x + kr * 0.5f, c.y + kr, true));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
    g.setColour (pinkDark.withAlpha (0.55f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.2f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-2.f, -kr + 3.f, 4.f, kr * 0.55f, 2.f);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

//==============================================================================
struct KnobDef { const char* id; const char* name; int kind; const char* suffix; }; // kind: 0=percent, 1=value+suffix

static const KnobDef kDefs[] = {
    // rand 1: curatare
    { "noise",   "NOISE",      0, "" }, { "upward",  "UPWARD",    0, "" }, { "peak",   "PEAK TAMER", 0, "" },
    { "reson",   "RESONANCE",  0, "" }, { "hpf",     "LOW CUT",   1, " Hz" }, { "boxy",   "BOXY CUT",  0, "" },
    // rand 2: ton + dinamica
    { "clarity", "CLARITY",    0, "" }, { "air",     "AIR",       0, "" }, { "sat",    "SATURATION", 0, "" },
    { "deess",   "DE-ESSER",   0, "" }, { "c1176",   "FET 1176",  0, "" }, { "la2a",   "OPTO LA-2A", 0, "" },
    // rand 3: spatiu + nivel
    { "dlytime", "DELAY TIME", 1, " ms" }, { "dlyfb", "FEEDBACK",  0, "" }, { "dlymix", "DELAY MIX", 0, "" },
    { "revsize", "REV SIZE",   0, "" }, { "revmix",  "REVERB MIX", 0, "" }, { "boost",  "BOOST",     1, " dB" }
};

static constexpr int kCols = 6;
static constexpr int kRowTop = 262;
static constexpr int kRowH = 108;

MemoAudioProcessorEditor::MemoAudioProcessorEditor (MemoAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    setSize (880, 600);

    for (const auto& d : kDefs)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                       juce::MathConstants<float>::pi * 2.8f, true);
        k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, d.id, k->slider);

        if (d.kind == 0)
            k->slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
        else
        {
            const juce::String suf (d.suffix);
            k->slider.textFromValueFunction = [suf] (double v) { return juce::String (juce::roundToInt (v * 10.0) / 10.0, 1) + suf; };
        }
        k->slider.setPopupDisplayEnabled (true, true, this);

        k->label.setText (d.name, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (mkFont (12.f, juce::Font::bold));
        k->label.setColour (juce::Label::textColourId, ink);

        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        knobs.push_back (std::move (k));
    }

    promptBox.setMultiLine (false);
    promptBox.setReturnKeyStartsNewLine (false);
    promptBox.setFont (mkFont (17.f));
    promptBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2a1823));
    promptBox.setColour (juce::TextEditor::textColourId, pinkSoft);
    promptBox.setColour (juce::TextEditor::outlineColourId, pinkDark);
    promptBox.setColour (juce::TextEditor::focusedOutlineColourId, pink);
    promptBox.setColour (juce::TextEditor::highlightColourId, pinkDark);
    promptBox.setColour (juce::CaretComponent::caretColourId, pink);
    promptBox.setTextToShowWhenEmpty ("ex: vocal curat, cald, cu aer si putin delay", pinkDark.brighter (0.2f));
    promptBox.setText (proc.promptText, false);
    promptBox.onReturnKey = [this] { applyPrompt(); };
    addAndMakeVisible (promptBox);

    mixButton.onClick = [this] { applyPrompt(); };
    addAndMakeVisible (mixButton);

    status.setFont (mkFont (14.f));
    status.setColour (juce::Label::textColourId, pinkSoft.withAlpha (0.85f));
    status.setJustificationType (juce::Justification::topLeft);
    status.setText ("Scrie cum vrei sa sune vocea si apasa Enter. Merge si in engleza: warm, clean, airy, aggressive...",
                    juce::dontSendNotification);
    addAndMakeVisible (status);
}

MemoAudioProcessorEditor::~MemoAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MemoAudioProcessorEditor::applyPrompt()
{
    const auto text = promptBox.getText();
    proc.promptText = text;
    const auto res = memo::PromptParser::parse (text);

    if (res.understood.isEmpty())
    {
        status.setText ("Nu am prins nimic. Incearca: curat, cald, aer, agresiv, fara sibilante, mai tare, delay, reverb, uscat...",
                        juce::dontSendNotification);
        return;
    }

    for (const auto& kv : res.values)
        if (auto* prm = proc.apvts.getParameter (kv.first))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (kv.second));
            prm->endChangeGesture();
        }

    status.setText ("Gata! Am aplicat: " + res.understood.joinIntoString (" + ")
                        + ". Poti rafina oricand din butoane.",
                    juce::dontSendNotification);
}

//==============================================================================
void MemoAudioProcessorEditor::drawDevil (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float cx = r.getCentreX(), top = r.getY(), bot = r.getBottom();

    g.setColour (pink.withAlpha (0.12f));
    g.fillEllipse (r.expanded (6.f));

    // coada
    juce::Path tail;
    tail.startNewSubPath (cx + 28.f, bot - 30.f);
    tail.cubicTo (cx + 64.f, bot - 22.f, cx + 68.f, bot - 62.f, cx + 50.f, bot - 74.f);
    g.setColour (pinkDark);
    g.strokePath (tail, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path tip;
    tip.addTriangle (cx + 42.f, bot - 70.f, cx + 58.f, bot - 84.f, cx + 60.f, bot - 66.f);
    g.fillPath (tip);

    // corpul
    g.setColour (pink);
    g.fillEllipse (cx - 34.f, top + 62.f, 68.f, 66.f);
    g.setColour (pinkSoft.withAlpha (0.55f));
    g.fillEllipse (cx - 20.f, top + 82.f, 40.f, 40.f);
    g.setColour (pinkDark);
    g.fillEllipse (cx - 30.f, bot - 18.f, 24.f, 14.f);
    g.fillEllipse (cx + 6.f, bot - 18.f, 24.f, 14.f);

    // coarne
    juce::Path hornL, hornR;
    hornL.addTriangle (cx - 40.f, top + 34.f, cx - 20.f, top + 22.f, cx - 44.f, top + 2.f);
    hornR.addTriangle (cx + 40.f, top + 34.f, cx + 20.f, top + 22.f, cx + 44.f, top + 2.f);
    g.setColour (pinkDark);
    g.fillPath (hornL);
    g.fillPath (hornR);

    // capul
    g.setColour (pink);
    g.fillEllipse (cx - 42.f, top + 14.f, 84.f, 72.f);
    g.setColour (pinkSoft.withAlpha (0.35f));
    g.fillEllipse (cx - 30.f, top + 20.f, 34.f, 16.f);

    // ochi
    g.setColour (juce::Colour (0xff22101a));
    g.fillEllipse (cx - 24.f, top + 42.f, 11.f, 15.f);
    g.fillEllipse (cx + 13.f, top + 42.f, 11.f, 15.f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (cx - 21.f, top + 44.f, 4.f, 5.f);
    g.fillEllipse (cx + 16.f, top + 44.f, 4.f, 5.f);

    // obraji + gura
    g.setColour (pinkDark.withAlpha (0.35f));
    g.fillEllipse (cx - 36.f, top + 58.f, 14.f, 8.f);
    g.fillEllipse (cx + 22.f, top + 58.f, 14.f, 8.f);
    juce::Path smile;
    smile.startNewSubPath (cx - 11.f, top + 62.f);
    smile.quadraticTo (cx, top + 74.f, cx + 11.f, top + 62.f);
    g.setColour (juce::Colour (0xff22101a));
    g.strokePath (smile, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path fang;
    fang.addTriangle (cx + 4.f, top + 67.f, cx + 9.f, top + 66.f, cx + 7.f, top + 73.f);
    g.setColour (juce::Colours::white);
    g.fillPath (fang);
}

void MemoAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto w = (float) getWidth(), h = (float) getHeight();

    g.fillAll (cream2);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, 0.f, 0.f, cream, 0.f, h, false));
    g.fillRoundedRectangle (6.f, 6.f, w - 12.f, h - 12.f, 22.f);
    g.setColour (cream2);
    g.drawRoundedRectangle (6.f, 6.f, w - 12.f, h - 12.f, 22.f, 2.f);

    // logo
    g.setColour (pink);
    g.setFont (mkFont (42.f, juce::Font::bold | juce::Font::italic));
    g.drawText ("memo", 30, 14, 220, 52, juce::Justification::centredLeft);
    g.setColour (ink.withAlpha (0.55f));
    g.setFont (mkFont (12.f, juce::Font::bold));
    g.drawText ("VOCAL MIX ASSISTANT  /  VST3", getWidth() - 320, 26, 290, 24, juce::Justification::centredRight);

    // ecranul retro
    const auto sc = screenArea.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.fillRoundedRectangle (sc.translated (0.f, 4.f), 16.f);
    g.setColour (screenBg);
    g.fillRoundedRectangle (sc, 16.f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (sc, 16.f);
        g.reduceClipRegion (clip);
        g.setColour (pink.withAlpha (0.07f));
        g.fillEllipse (sc.getX() - 40.f, sc.getY() - 60.f, 300.f, 240.f);
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        for (float yy = sc.getY(); yy < sc.getBottom(); yy += 3.f)
            g.fillRect (sc.getX(), yy, sc.getWidth(), 1.f);
    }
    g.setColour (pinkDark);
    g.drawRoundedRectangle (sc, 16.f, 2.f);

    drawDevil (g, juce::Rectangle<float> (sc.getX() + 20.f, sc.getY() + 16.f, 130.f, 140.f));

    g.setColour (pink);
    g.setFont (mkFont (15.f, juce::Font::bold));
    g.drawText ("> cum vrei sa sune vocea?", screenArea.getX() + 170, screenArea.getY() + 14, 400, 24,
                juce::Justification::centredLeft);

    // etichete de sectiune
    const char* sections[] = { "CLEAN UP", "TONE & DYNAMICS", "SPACE & LEVEL" };
    for (int i = 0; i < 3; ++i)
    {
        const int yy = kRowTop + i * kRowH;
        g.setColour (pinkDark);
        g.setFont (mkFont (11.f, juce::Font::bold));
        g.drawText (sections[i], 28, yy, 200, 16, juce::Justification::centredLeft);
        g.setColour (cream2);
        g.fillRect (150, yy + 8, getWidth() - 150 - 28, 1);
    }
}

void MemoAudioProcessorEditor::resized()
{
    screenArea = { 24, 74, getWidth() - 48, 170 };

    const int left = screenArea.getX() + 170;
    const int right = screenArea.getRight() - 24;
    promptBox.setBounds (left, screenArea.getY() + 48, right - left - 110, 40);
    mixButton.setBounds (promptBox.getRight() + 10, promptBox.getY(), 100, 40);
    status.setBounds (left, screenArea.getY() + 98, right - left, 60);

    const int cellW = (getWidth() - 48) / kCols;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int row = (int) i / kCols, col = (int) i % kCols;
        const int x = 24 + col * cellW;
        const int y = kRowTop + row * kRowH + 18;
        knobs[i]->slider.setBounds (x + (cellW - 76) / 2, y, 76, 76);
        knobs[i]->label.setBounds (x, y + 76, cellW, 16);
    }
}
