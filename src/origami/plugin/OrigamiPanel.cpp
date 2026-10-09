// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "OrigamiPanel.h"
#include "OrigamiUiLogic.h"

#include <cmath>

using namespace origami;

namespace {

const juce::Colour kInk (0xffefece2), kDim (0xff9c998e), kAcc (0xffc9b98f), kInd (0xff8f9be8);
const juce::Colour kAudio (0xffe53b2f), kCv (0xfff7e77d);    // JCS R14 role colours (AUDIO, CV)

constexpr float kJackX[OrigamiPanel::kJackCount] = { 80, 190, 300, 410, 726, 836, 946, 1056 };
constexpr float kJackY = 284.0f;
constexpr const char* kJackIds[OrigamiPanel::kJackCount] = { "IN:IN L", "IN:IN R", "VCA:VCA CV", "VC:VC 1", "VC:VC 2", "VC:VC 3", "OUT:OUT L", "OUT:OUT R" };
constexpr const char* kJackLabels[OrigamiPanel::kJackCount] = { "IN L", "IN R", "VCA CV", "VC 1", "VC 2", "VC 3", "OUT L", "OUT R" };
constexpr float kScales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };

juce::Font font (float size, bool bold = true, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions {}.withPointHeight (size).withStyle (bold ? "Bold" : "Regular")).withExtraKerningFactor (kerning);
}

// The panel being painted collects its help text here (for the hover tooltips and the text size check).
std::vector<OrigamiPanel::Label>* gDrawn = nullptr;

void note (juce::Rectangle<float> area, const juce::String& s, float size)
{
    if (gDrawn != nullptr && s.trim().isNotEmpty())
        gDrawn->push_back ({ area, s, size });
}

// SVG-style text: x is the left edge, centre or right edge; y is the baseline.
void text (juce::Graphics& g, float x, float y, const juce::String& s, float size, juce::Justification j = juce::Justification::horizontallyCentred,
           juce::Colour c = kInk, bool bold = true, float kerning = 0.05f)
{
    const auto f = font (size, bold, kerning);
    g.setColour (c);
    g.setFont (f);
    g.drawSingleLineText (s, juce::roundToInt (x), juce::roundToInt (y), j);
}

// Help text (captions, hints, footnotes): never below the minimum size, and hovering it shows it enlarged.
// Control labels (knobs, jacks, buttons, switches) use text() at their own sizes.
void help (juce::Graphics& g, float x, float y, const juce::String& s, float size, juce::Justification j = juce::Justification::horizontallyCentred,
           juce::Colour c = kDim, bool bold = true)
{
    size = juce::jmax (OrigamiPanel::kMinTextSize, size);
    text (g, x, y, s, size, j, c, bold);
    const auto f = font (size, bold, 0.05f);
    const float w = juce::GlyphArrangement::getStringWidth (f, s);
    const float left = j.testFlags (juce::Justification::right) ? x - w : (j.testFlags (juce::Justification::left) ? x : x - w * 0.5f);
    note ({ left - 2.0f, y - f.getAscent() - 2.0f, w + 4.0f, f.getHeight() + 4.0f }, s, size);
}

void box (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    g.setColour (kAcc);
    g.drawRoundedRectangle (r, 2.0f, 1.1f);
    if (title.isNotEmpty())
    {
        const auto f = font (9.5f, true, 0.13f);
        const float w = juce::GlyphArrangement::getStringWidth (f, title) + 10.0f;
        g.setColour (juce::Colour (0xff131316));
        g.fillRect (r.getX() + 8.0f, r.getY() - 6.0f, w, 12.0f);
        text (g, r.getX() + 13.0f, r.getY() + 3.5f, title, 9.5f, juce::Justification::left, kInk, true, 0.13f);
    }
}

void radial (juce::Graphics& g, float cx, float cy, float r, juce::Colour a, juce::Colour m, juce::Colour b)
{
    juce::ColourGradient gr (a, cx - 0.2f * r, cy - 0.4f * r, b, cx + r * 0.9f, cy + r * 1.2f, true);
    gr.addColour (0.55, m);
    g.setGradientFill (gr);
    g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
}

void screw (juce::Graphics& g, float cx, float cy)
{
    radial (g, cx, cy, 7.0f, juce::Colour (0xffe2e2dc), juce::Colour (0xff8f8f8a), juce::Colour (0xff3a3a38));
    g.setColour (juce::Colours::black);
    g.drawEllipse (cx - 7, cy - 7, 14, 14, 1.0f);
    g.setColour (juce::Colour (0xff2a2a28));
    g.drawLine (cx - 4.5f, cy - 4.5f, cx + 4.5f, cy + 4.5f, 1.6f);
    g.drawLine (cx - 4.5f, cy + 4.5f, cx + 4.5f, cy - 4.5f, 1.6f);
}

void led (juce::Graphics& g, float cx, float cy, bool lit, juce::Colour c)
{
    g.setColour (lit ? c.darker (0.6f) : juce::Colour (0xff3a1010));
    g.fillEllipse (cx - 5, cy - 5, 10, 10);
    g.setColour (juce::Colours::black);
    g.drawEllipse (cx - 5, cy - 5, 10, 10, 1.0f);
    g.setColour (c.withAlpha (lit ? 1.0f : 0.35f));
    g.fillEllipse (cx - 2.4f, cy - 2.4f, 4.8f, 4.8f);
    if (lit)
    {
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (cx - 9, cy - 9, 18, 18);
    }
}

juce::String valueText (int p, double v)
{
    const auto& i = paramInfo (p);
    if (i.choices > 0)
        return juce::String (i.name) + "  " + i.choiceNames[juce::jlimit (0, i.choices - 1, (int) v)];
    juce::String s;
    if (juce::String (i.unit) == "dB") s = juce::String (v, 1) + " dB";
    else if (juce::String (i.unit) == "ms") s = juce::String (v, v < 10 ? 2 : 0) + " ms";
    else if (juce::String (i.unit) == "x") s = juce::String (v, 2) + " x";
    else if (i.min < 0) s = (v > 0 ? "+" : "") + juce::String (v, 2);
    else s = juce::String (juce::roundToInt (v * 100.0)) + " %";
    return juce::String (i.name) + "  " + s;
}

} // namespace

OrigamiPanel::OrigamiPanel (Access& access, Mode mode) : access_ (access), mode_ (mode)
{
    setOpaque (true);
    rebuild();
    startTimerHz (30);
}

OrigamiPanel::~OrigamiPanel() = default;

const char* OrigamiPanel::pageName (Page p)
{
    switch (p)
    {
        case Page::Main: return "MAIN";
        case Page::Stages: return "STAGES";
        case Page::Dynamics: return "DYNAMICS";
        case Page::Setup: return "SETUP";
    }
    return "";
}

const char* OrigamiPanel::jackId (int jack) { return jack >= 0 && jack < kJackCount ? kJackIds[jack] : ""; }

void OrigamiPanel::setMode (Mode m)
{
    if (m == mode_) return;
    mode_ = m;
    drag_ = -1;
    rebuild();
    repaint();
}

void OrigamiPanel::setPage (Page p)
{
    if (p == page_) return;
    page_ = p;
    drag_ = -1;
    rebuild();
    repaint();
    if (onPageChange) onPageChange (p);
}

juce::Point<float> OrigamiPanel::designSize() const
{
    if (mode_ == Mode::Plugin) return { kPluginW, kPluginH };
    if (mode_ == Mode::RackOpen) return { kFaceW, kFaceH };
    return { kFaceW, kClosedH };
}

float OrigamiPanel::scale() const
{
    const auto d = designSize();
    return juce::jmax (0.01f, juce::jmin ((float) getWidth() / d.x, (float) getHeight() / d.y));
}

juce::Point<float> OrigamiPanel::origin() const
{
    const auto d = designSize() * scale();
    return { ((float) getWidth() - d.x) * 0.5f, ((float) getHeight() - d.y) * 0.5f };
}

juce::Point<float> OrigamiPanel::toDesign (juce::Point<float> local) const { return (local - origin()) / scale(); }
juce::Point<float> OrigamiPanel::toLocal (juce::Point<float> design) const { return design * scale() + origin(); }
juce::Point<float> OrigamiPanel::faceOrigin() const { return mode_ == Mode::Plugin ? juce::Point<float> (32.0f, 28.0f) : juce::Point<float>(); }

juce::Point<float> OrigamiPanel::jackCentre (int jack) const
{
    if (mode_ == Mode::RackClosed || jack < 0 || jack >= kJackCount) return {};
    return toLocal (faceOrigin() + juce::Point<float> (kJackX[jack], kJackY));
}

float OrigamiPanel::jackRadius() const { return 11.5f * scale(); }

int OrigamiPanel::jackAt (juce::Point<float> local) const
{
    if (mode_ == Mode::RackClosed) return -1;
    for (int j = 0; j < kJackCount; ++j)
        if (local.getDistanceFrom (jackCentre (j)) <= juce::jmax (8.0f, 14.0f * scale()))    // >= 16 px target
            return j;
    return -1;
}

// ---------------------------------------------------------------------------------------------- controls

void OrigamiPanel::rebuild()
{
    controls_.clear();
    if (mode_ == Mode::RackClosed)
    {
        const float cx = kFaceW * 0.5f;
        controls_.push_back ({ kMix, Kind::Knob, { cx - 150, 38 }, 18, "MIX", {}, {}, false });
        controls_.push_back ({ kWave, Kind::Knob, { cx, 38 }, 26, "", "", "", true });
        controls_.push_back ({ kLevel, Kind::Knob, { cx + 150, 38 }, 18, "LEVEL", {}, {}, false });
        controls_.push_back ({ kBypass, Kind::Button, { 270, 41 }, 30, "BYPASS", {}, {}, false });
        return;
    }
    addFaceControls (faceOrigin());
}

void OrigamiPanel::addFaceControls (juce::Point<float> o)
{
    auto add = [this, o] (int p, Kind k, float x, float y, float r, juce::String label, juce::String l = {}, juce::String rt = {}, bool big = false) {
        controls_.push_back ({ p, k, o + juce::Point<float> (x, y), r, label, l, rt, big });
    };
    switch (page_)
    {
        case Page::Main:
            add (kGain, Kind::Knob, 80, 82, 22, "GAIN");
            add (kVcaDepth, Kind::Knob, 198, 82, 22, "VCA DEPTH");
            add (kVcaSource, Kind::Toggle, 139, 158, 10, "", "FOLLOW", "CV");
            add (kMix, Kind::Knob, 938, 82, 22, "MIX");
            add (kLevel, Kind::Knob, 1056, 82, 22, "LEVEL");
            add (kLevelComp, Kind::Toggle, 997, 158, 10, "", "", "LEVEL COMP");
            add (kWave, Kind::Knob, 568, 90, 42, "", {}, {}, true);
            add (kEmph, Kind::Knob, 418, 82, 15, "EMPH");
            add (kSym, Kind::Knob, 718, 82, 15, "SYM");
            for (int i = 0; i < 3; ++i)
                add (kStage1 + i, Kind::Knob, 418.0f + 150.0f * (float) i, 198, 15, "STAGE " + juce::String (i + 1));
            break;
        case Page::Stages:
            // Each box, mirror-symmetric about its centre line: STAGE n | graph | SYM n over VC > AMT | source | VC > SYM.
            // STAGE n is the MAIN page's parameter (ui::stageAmountParam), shown here as a knob and as the graph.
            for (int i = 0; i < 3; ++i)
            {
                const float bx = 14.0f + 374.0f * (float) i;
                const juce::String n (i + 1);
                add (ui::stageAmountParam (i), Kind::Knob, bx + 52, 80, 18, "STAGE " + n);
                add (kSym1 + i, Kind::Knob, bx + 308, 80, 18, "SYM " + n);
                add (kVc1Amt + i, Kind::Knob, bx + 70, 184, 16, "VC " + n + " > AMT");
                add (kVc1Sym + i, Kind::Knob, bx + 290, 184, 16, "VC " + n + " > SYM");
                add (kVc1Src + i, Kind::Selector, bx + 180, 184, 70, "VC " + n);
            }
            for (int i = 0; i < 3; ++i)
            {
                const auto gr = stageGraph (i) - o;
                add (ui::stageAmountParam (i), Kind::Graph, gr.getCentreX(), gr.getCentreY(), gr.getWidth() * 0.5f, "STAGE " + juce::String (i + 1));
            }
            break;
        case Page::Dynamics:
            add (kAttack, Kind::Knob, 177, 90, 22, "ATTACK");
            add (kRelease, Kind::Knob, 397, 90, 22, "RELEASE");
            add (kCvScale, Kind::Knob, 738, 90, 22, "CV SCALE");
            add (kVcaLaw, Kind::Toggle, 958, 90, 10, "", "LIN", "EXP");
            add (kVcaSource, Kind::Toggle, 848, 168, 10, "", "FOLLOW", "CV");
            break;
        case Page::Setup:
            add (kQuality, Kind::Toggle, 194, 80, 10, "", juce::String (juce::CharPointer_UTF8 ("1\xc3\x97")), juce::String (juce::CharPointer_UTF8 ("2\xc3\x97")));
            add (kLevelComp, Kind::Toggle, 942, 80, 10, "", "OFF", "ON");
            add (kBypass, Kind::Toggle, 942, 140, 10, "", "OFF", "ON");
            break;
    }
}

juce::Rectangle<float> OrigamiPanel::hitBox (const Control& c) const
{
    switch (c.kind)
    {
        case Kind::Knob: return juce::Rectangle<float> (c.c.x - c.r - 6, c.c.y - c.r - 6, 2 * c.r + 12, 2 * c.r + 12);
        case Kind::Toggle: return juce::Rectangle<float> (c.c.x - 24, c.c.y - 10, 48, 20);
        case Kind::Selector: return juce::Rectangle<float> (c.c.x - c.r, c.c.y - 9, 2 * c.r, 18);
        case Kind::Button: return juce::Rectangle<float> (c.c.x - c.r, c.c.y - 11, 2 * c.r, 22);
        case Kind::Graph: return stageGraph (c.param - kStage1);
    }
    return {};
}

juce::Rectangle<float> OrigamiPanel::stageGraph (int stage) const
{
    return { faceOrigin().x + 14.0f + 374.0f * (float) stage + 105.0f, faceOrigin().y + 28.0f, 150.0f, 104.0f };
}

juce::Point<float> OrigamiPanel::stageGraphCentre (int stage) const
{
    for (auto& c : controls_)
        if (c.kind == Kind::Graph && c.param == ui::stageAmountParam (stage))
            return toLocal (c.c);
    return { -1.0f, -1.0f };
}

int OrigamiPanel::controlAt (juce::Point<float> d) const
{
    for (size_t i = 0; i < controls_.size(); ++i)
        if (hitBox (controls_[i]).contains (d))
            return (int) i;
    return -1;
}

juce::Point<float> OrigamiPanel::controlCentre (int param) const
{
    for (auto& c : controls_)
        if (c.param == param)
            return toLocal (c.c);
    return { -1.0f, -1.0f };
}

juce::Point<float> OrigamiPanel::tabCentre (Page p) const
{
    return toLocal ({ kPluginW * 0.5f - 200.0f + 100.0f * (float) (int) p + 46.0f, 14.0f });
}

int OrigamiPanel::tabAt (juce::Point<float> d) const
{
    if (mode_ != Mode::Plugin || d.y < 2 || d.y > 26) return -1;
    for (int i = 0; i < 4; ++i)
    {
        const float x = kPluginW * 0.5f - 200.0f + 100.0f * (float) i;
        if (d.x >= x && d.x <= x + 92) return i;
    }
    return -1;
}

int OrigamiPanel::scaleButtonAt (juce::Point<float> d) const
{
    if (mode_ != Mode::Plugin || page_ != Page::Setup) return -1;
    const auto f = d - faceOrigin();
    for (int i = 0; i < 5; ++i)
        if (juce::Rectangle<float> (568.0f + (float) (i - 2) * 64.0f - 28.0f, 69.0f, 56.0f, 22.0f).contains (f))
            return i;
    return -1;
}

double OrigamiPanel::normal (int p) const { return toNormal (p, access_.get (p)); }

void OrigamiPanel::setNormal (int p, double n)
{
    access_.set (p, fromNormal (p, juce::jlimit (0.0, 1.0, n)));
    repaint();
}

// ---------------------------------------------------------------------------------------------- mouse

void OrigamiPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto d = toDesign (e.position);
    if (const int t = tabAt (d); t >= 0) { setPage ((Page) t); return; }
    if (const auto pp = presetPartAt (d); pp != PresetPart::None)
    {
        if (pp == PresetPart::Prev) stepPreset (-1);
        else if (pp == PresetPart::Next) stepPreset (1);
        else
        {
            juce::Component::SafePointer<OrigamiPanel> safe (this);
            presetMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                            .withTargetScreenArea (localAreaToGlobal (juce::Rectangle<float> (
                                                presetPartCentre (PresetPart::Prev), presetPartCentre (PresetPart::Next)).toNearestInt().expanded (0, 10))),
                                        [safe] (int id) { if (id > 0 && safe != nullptr) { safe->access_.loadPreset (id - 1); safe->repaint(); } });
        }
        return;
    }
    if (const int s = scaleButtonAt (d); s >= 0) { uiScale = kScales[s]; if (onScale) onScale (kScales[s]); repaint(); return; }
    const int i = controlAt (d);
    if (i < 0) return;
    const auto& c = controls_[(size_t) i];
    const auto& info = paramInfo (c.param);
    if (c.kind == Kind::Knob || c.kind == Kind::Graph)
    {
        drag_ = i;
        dragStartY_ = e.position.y;
        dragStartNormal_ = normal (c.param);
        access_.gesture (c.param, true);
        return;
    }
    // List controls: a right-click opens the whole list. Switches and lists step on a click (Shift-click: back).
    if (c.kind == Kind::Selector && e.mods.isPopupMenu())
    {
        showChoiceMenu (c);
        return;
    }
    const int n = juce::jmax (2, info.choices);
    const int v = ui::stepChoice ((int) std::lround (access_.get (c.param)), n, e.mods.isShiftDown() || e.mods.isPopupMenu());
    access_.gesture (c.param, true);
    access_.set (c.param, (double) v);
    access_.gesture (c.param, false);
    repaint();
}

void OrigamiPanel::mouseMove (const juce::MouseEvent& e)
{
    const int i = controlAt (toDesign (e.position));
    setMouseCursor (i >= 0 && controls_[(size_t) i].kind == Kind::Graph ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void OrigamiPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (drag_ < 0) return;
    setNormal (controls_[(size_t) drag_].param,
               ui::dragToNormal (dragStartNormal_, (double) (dragStartY_ - e.position.y), (double) scale(), e.mods.isShiftDown()));
}

void OrigamiPanel::mouseUp (const juce::MouseEvent&)
{
    if (drag_ >= 0)
        access_.gesture (controls_[(size_t) drag_].param, false);
    drag_ = -1;
    repaint();
}

void OrigamiPanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = controlAt (toDesign (e.position));
    if (i < 0 || (controls_[(size_t) i].kind != Kind::Knob && controls_[(size_t) i].kind != Kind::Graph)) return;
    const int p = controls_[(size_t) i].param;
    access_.gesture (p, true);
    access_.set (p, paramInfo (p).def);
    access_.gesture (p, false);
    repaint();
}

void OrigamiPanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int i = controlAt (toDesign (e.position));
    if (i < 0 || (controls_[(size_t) i].kind != Kind::Knob && controls_[(size_t) i].kind != Kind::Graph)) return;
    const int p = controls_[(size_t) i].param;
    access_.gesture (p, true);
    setNormal (p, normal (p) + (w.deltaY > 0 ? 1.0 : -1.0) * (e.mods.isShiftDown() ? 0.004 : 0.02));
    access_.gesture (p, false);
}

juce::PopupMenu OrigamiPanel::choiceMenu (int param) const
{
    juce::PopupMenu menu;
    const auto list = ui::choiceList (param, access_.get (param));
    for (size_t k = 0; k < list.names.size(); ++k)
        menu.addItem ((int) k + 1, juce::String (list.names[k]), true, (int) k == list.current);
    return menu;
}

void OrigamiPanel::applyChoice (int param, int menuId)
{
    const int choice = ui::choiceFromMenuResult (param, menuId);
    if (choice < 0) return;
    access_.gesture (param, true);
    access_.set (param, (double) choice);
    access_.gesture (param, false);
    repaint();
}

void OrigamiPanel::showChoiceMenu (const Control& c)
{
    juce::Component::SafePointer<OrigamiPanel> safe (this);
    const int param = c.param;
    const auto area = hitBox (c);
    const auto options = juce::PopupMenu::Options().withTargetComponent (this)
                             .withTargetScreenArea (localAreaToGlobal (juce::Rectangle<float> (toLocal (area.getTopLeft()), toLocal (area.getBottomRight())).toNearestInt()));
    std::function<void (int)> done = [safe, param] (int id) { if (safe != nullptr) safe->applyChoice (param, id); };
    if (showMenu)
        showMenu (choiceMenu (param), options, std::move (done));
    else
        choiceMenu (param).showMenuAsync (options, std::move (done));
}

juce::String OrigamiPanel::getTooltip() { return tooltipAt (getMouseXYRelative().toFloat()); }

float OrigamiPanel::smallestTextSize() const
{
    float m = 0.0f;
    for (auto& l : labels_)
        m = m <= 0.0f ? l.size : juce::jmin (m, l.size);
    return m;
}

juce::String OrigamiPanel::tooltipAt (juce::Point<float> local) const
{
    if (presetPartAt (toDesign (local)) != PresetPart::None)
        return juce::String (juce::CharPointer_UTF8 ("FACTORY PRESETS  \xc2\xb7  arrows step through all banks, click the name for the menu (INIT, RONIN, SHOGUN, BUSHIDO, GENERIC)"));
    if (const int j = jackAt (local); j >= 0)
    {
        juce::String s = juce::String (kJackIds[j]) + (jackIsAudio (j) ? juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  AUDIO")) : juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  CV")));
        if (j >= Vc1 && j <= Vc3) s << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  audio rate OK (unsmoothed)"));
        if (j == VcaCv) s << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  0..5 V sets the VCA when VCA SOURCE = CV"));
        if (mode_ == Mode::Plugin)
        {
            if (j == InL || j == InR) s << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  normalled to the host input"));
            else if (jackIsOutput (j)) s << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  goes to the host output"));
            else s << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  in the plugin: internal source or sidechain (STAGES)"));
        }
        return s;
    }
    const int i = controlAt (toDesign (local));
    if (i >= 0)
    {
        const auto& c = controls_[(size_t) i];
        auto s = valueText (c.param, access_.get (c.param));
        if (c.kind == Kind::Graph)
            s << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  drag up / down to set it (Shift: fine), double-click resets"));
        else if (c.kind == Kind::Selector)
            s << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  click: next, Shift-click: previous, right-click: the whole list"));
        return s;
    }
    // Help text: itself, enlarged.
    const auto d = toDesign (local);
    for (auto it = labels_.rbegin(); it != labels_.rend(); ++it)
        if (it->area.contains (d))
            return it->text;
    return {};
}

// ---------------------------------------------------------------------------------------------- painting

juce::String OrigamiPanel::statusText() const
{
    const bool two = access_.get (kQuality) >= 0.5;
    return juce::String (juce::CharPointer_UTF8 (two ? "2\xc3\x97 ON \xc2\xb7 LAT " : "2\xc3\x97 OFF \xc2\xb7 LAT ")) + juce::String (access_.latencySamples());
}

void OrigamiPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a0c));
    labels_.clear();
    gDrawn = &labels_;
    g.saveState();
    g.addTransform (juce::AffineTransform::scale (scale()).translated (origin()));
    if (mode_ == Mode::RackClosed)
        paintClosed (g);
    else
    {
        if (mode_ == Mode::Plugin) paintPluginChrome (g);
        paintFace (g, faceOrigin());
        if (mode_ == Mode::RackOpen && showsPresets()) paintPresetBox (g);
    }
    for (auto& c : controls_) paintControl (g, c);
    if (drag_ >= 0)
    {
        const auto& c = controls_[(size_t) drag_];
        const auto s = valueText (c.param, access_.get (c.param));
        const float w = juce::GlyphArrangement::getStringWidth (font (9.0f), s) + 14.0f;
        const float top = c.kind == Kind::Graph ? hitBox (c).getY() + 4.0f : c.c.y - c.r - 30.0f;
        const juce::Rectangle<float> r (c.c.x - w * 0.5f, top, w, 18.0f);
        g.setColour (juce::Colour (0xf00b0b14));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (0xff33354a));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        text (g, r.getCentreX(), r.getBottom() - 5.0f, s, 9.0f, juce::Justification::horizontallyCentred, kInd);
    }
    g.restoreState();
    gDrawn = nullptr;
}

void OrigamiPanel::paintPluginChrome (juce::Graphics& g)
{
    for (float x : { 0.0f, kPluginW - 30.0f })
    {
        juce::ColourGradient ear (juce::Colour (0xff2c2e2c), x, 0, juce::Colour (0xff262826), x + 30, 0, false);
        ear.addColour (0.5, juce::Colour (0xff4a4d4a));
        g.setGradientFill (ear);
        g.fillRect (x, 0.0f, 30.0f, kPluginH);
        g.setColour (juce::Colours::black);
        g.drawRect (x, 0.0f, 30.0f, kPluginH, 1.0f);
        for (float y : { 22.0f, kPluginH * 0.5f, kPluginH - 22.0f })
            screw (g, x + 15.0f, y);
    }
    // Device tab strip (in the rack the rack's own strip replaces it).
    g.setColour (juce::Colour (0xff0d0d10));
    g.fillRect (32.0f, 0.0f, kFaceW, 28.0f);
    text (g, 46, 19, "ORIGAMI", 13, juce::Justification::left, kInk, true, 0.3f);
    text (g, 154, 19, juce::String (juce::CharPointer_UTF8 ("TRIPLE WAVE SHAPER \xc2\xb7 STEREO")), 8, juce::Justification::left, kDim, true, 0.18f);
    for (int i = 0; i < 4; ++i)
    {
        const float x = kPluginW * 0.5f - 200.0f + 100.0f * (float) i;
        const bool on = (int) page_ == i;
        g.setColour (on ? kInd : juce::Colour (0xff16161b));
        g.fillRoundedRectangle (x, 4, 92, 20, 3);
        g.setColour (juce::Colour (0xff30324a));
        g.drawRoundedRectangle (x, 4, 92, 20, 3, 1.0f);
        text (g, x + 46, 18, pageName ((Page) i), 8.5f, juce::Justification::horizontallyCentred, on ? juce::Colour (0xff0b0b12) : kInk);
    }
    text (g, kPluginW - 46, 19, statusText(), 8, juce::Justification::right, kDim);
    if (showsPresets()) paintPresetBox (g);
}

// ---------------------------------------------------------------------------------------------- presets

namespace {
constexpr float kPresetW = 236.0f, kPresetArrow = 20.0f;
}

juce::Rectangle<float> OrigamiPanel::presetBox() const
{
    if (mode_ == Mode::Plugin)
        return { 804.0f, 4.0f, kPresetW, 20.0f };
    // Rack: centred in the jack row, between VC 1 and VC 2.
    return { (kJackX[Vc1] + kJackX[Vc2]) * 0.5f - kPresetW * 0.5f, kJackY - 22.0f, kPresetW, 20.0f };
}

void OrigamiPanel::paintPresetBox (juce::Graphics& g)
{
    const juce::Rectangle<float> box = presetBox();
    const float kPresetX = box.getX(), top = box.getY();
    g.setColour (juce::Colour (0xff16161b));
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (juce::Colour (0xff30324a));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    g.drawVerticalLine ((int) (kPresetX + kPresetArrow), top + 1.0f, top + 19.0f);
    g.drawVerticalLine ((int) (kPresetX + kPresetW - kPresetArrow), top + 1.0f, top + 19.0f);
    for (int side = 0; side < 2; ++side)
    {
        const float cx = side == 0 ? kPresetX + kPresetArrow * 0.5f : kPresetX + kPresetW - kPresetArrow * 0.5f;
        juce::Path tri;
        if (side == 0) tri.addTriangle (cx + 3.0f, top + 6.0f, cx + 3.0f, top + 14.0f, cx - 3.0f, top + 10.0f);
        else           tri.addTriangle (cx - 3.0f, top + 6.0f, cx - 3.0f, top + 14.0f, cx + 3.0f, top + 10.0f);
        g.setColour (kInk);
        g.fillPath (tri);
    }
    const int cur = access_.currentPreset();
    juce::String label = "PRESET";
    if (cur >= 0 && cur < access_.presetCount())
    {
        const auto bank = access_.presetBank (cur);
        label = (bank.isEmpty() || bank == "INIT") ? access_.presetName (cur)
                                                   : bank + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + access_.presetName (cur);
    }
    g.setColour (kInk);
    g.setFont (font (8.5f));
    g.drawFittedText (label, juce::Rectangle<int> ((int) (kPresetX + kPresetArrow + 4.0f), (int) top, (int) (kPresetW - 2.0f * kPresetArrow - 8.0f), 20),
                      juce::Justification::centred, 1, 0.8f);
}

OrigamiPanel::PresetPart OrigamiPanel::presetPartAt (juce::Point<float> d) const
{
    if (! showsPresets())
        return PresetPart::None;
    const auto box = presetBox();
    if (d.y < box.getY() - 2.0f || d.y > box.getBottom() + 2.0f || d.x < box.getX() || d.x > box.getRight()) return PresetPart::None;
    if (d.x < box.getX() + kPresetArrow) return PresetPart::Prev;
    if (d.x > box.getRight() - kPresetArrow) return PresetPart::Next;
    return PresetPart::Name;
}

juce::Point<float> OrigamiPanel::presetPartCentre (PresetPart part) const
{
    const auto box = presetBox();
    const float x = part == PresetPart::Prev ? box.getX() + kPresetArrow * 0.5f
                  : part == PresetPart::Next ? box.getRight() - kPresetArrow * 0.5f
                                             : box.getCentreX();
    return toLocal ({ x, box.getCentreY() });
}

juce::PopupMenu OrigamiPanel::presetMenu() const
{
    juce::PopupMenu menu;
    juce::StringArray banks;
    const int n = access_.presetCount(), cur = access_.currentPreset();
    for (int i = 0; i < n; ++i)
    {
        const auto b = access_.presetBank (i);
        if (b.isEmpty() || b == "INIT")
            menu.addItem (i + 1, access_.presetName (i), true, i == cur);
        else
            banks.addIfNotAlreadyThere (b);
    }
    for (const auto& b : banks)
    {
        juce::PopupMenu sub;
        bool holdsCurrent = false;
        for (int i = 0; i < n; ++i)
            if (access_.presetBank (i) == b)
            {
                sub.addItem (i + 1, access_.presetName (i), true, i == cur);
                holdsCurrent = holdsCurrent || i == cur;
            }
        menu.addSubMenu (b, sub, true, nullptr, holdsCurrent);
    }
    return menu;
}

void OrigamiPanel::stepPreset (int delta)
{
    const int n = access_.presetCount();
    if (n <= 0) return;
    const int cur = juce::jmax (0, access_.currentPreset());
    access_.loadPreset (((cur + delta) % n + n) % n);
    repaint();
}

void OrigamiPanel::paintFace (juce::Graphics& g, juce::Point<float> o)
{
    const float top = mode_ == Mode::Plugin ? 28.0f : 0.0f;
    juce::ColourGradient pl (juce::Colour (0xff1b1b20), 0, o.y - top, juce::Colour (0xff101014), 0, o.y + kFaceH, false);
    g.setGradientFill (pl);
    g.fillRect (o.x, o.y, kFaceW, kFaceH);
    switch (page_)
    {
        case Page::Main: paintMain (g, o); break;
        case Page::Stages: paintStages (g, o); break;
        case Page::Dynamics: paintDynamics (g, o); break;
        case Page::Setup: paintSetup (g, o); break;
    }
    paintJackRow (g, o);
}

void OrigamiPanel::paintMeter (juce::Graphics& g, juce::Rectangle<float> r, float volts)
{
    g.setColour (juce::Colour (0xff0b0b10));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (juce::Colour (0xff2a2a33));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    // dB scale: -48 .. +6 dB re 5 V.
    const float db = volts > 1.0e-6f ? 20.0f * std::log10 (volts / 5.0f) : -100.0f;
    const float f = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 54.0f);
    if (f > 0.0f)
    {
        g.setColour ((volts > 5.5f ? kAudio : kInd).withAlpha (0.8f));
        g.fillRoundedRectangle (r.reduced (1.0f).withWidth ((r.getWidth() - 2.0f) * f), (r.getHeight() - 2.0f) * 0.5f);
    }
    const float x0 = r.getX() + r.getWidth() * (48.0f / 54.0f);   // 0 dB = 5 V mark
    g.setColour (kDim.withAlpha (0.6f));
    g.drawLine (x0, r.getY() - 2.0f, x0, r.getBottom() + 2.0f, 0.8f);
}

void OrigamiPanel::paintMain (juce::Graphics& g, juce::Point<float> o)
{
    auto R = [o] (float x, float y, float w, float h) { return juce::Rectangle<float> (o.x + x, o.y + y, w, h); };
    box (g, R (14, 16, 250, 224), "INPUT");
    box (g, R (kFaceW - 264, 16, 250, 224), "OUTPUT");
    box (g, R (278, 16, kFaceW - 556, 224), "WAVE");
    help (g, o.x + 139, o.y + 186, "pre-fold VCA: follower or CV sets the drive", 9, juce::Justification::horizontallyCentred, kDim);
    paintMeter (g, R (44, 198, 190, 6), juce::jmax (access_.inPeak (0), access_.inPeak (1)));
    juce::Rectangle<float> fr = R (44, 209, 190, 4);
    g.setColour (juce::Colour (0xff0b0b10)); g.fillRoundedRectangle (fr, 2.0f);
    g.setColour (kAcc.withAlpha (0.7f)); g.fillRoundedRectangle (fr.withWidth (190.0f * juce::jlimit (0.0f, 1.0f, access_.follower())), 2.0f);
    text (g, o.x + 139, o.y + 228, "INPUT / FOLLOWER", 7, juce::Justification::horizontallyCentred, kDim);

    help (g, o.x + 997, o.y + 186, juce::String (juce::CharPointer_UTF8 ("DC block 8 Hz \xc2\xb7 dry aligned to wet")), 9, juce::Justification::horizontallyCentred, kDim);
    paintMeter (g, R (kFaceW - 234, 198, 190, 6), access_.outPeak (0));
    paintMeter (g, R (kFaceW - 234, 208, 190, 6), access_.outPeak (1));
    text (g, o.x + 997, o.y + 228, "OUTPUT", 7, juce::Justification::horizontallyCentred, kDim);
    led (g, o.x + kFaceW - 30, o.y + 205, access_.over(), kAudio);
    text (g, o.x + kFaceW - 30, o.y + 225, "OVER", 6.5f, juce::Justification::horizontallyCentred, kDim);

    text (g, o.x + 568, o.y + 150, "WAVE", 11, juce::Justification::horizontallyCentred, kInk, true, 0.3f);
    help (g, o.x + 568, o.y + 163, juce::String (juce::CharPointer_UTF8 ("0 = true bypass \xc2\xb7 sweeps stages 1 \xe2\x86\x92 2 \xe2\x86\x92 3")), 9,
          juce::Justification::horizontallyCentred, kDim);
}

void OrigamiPanel::paintStages (juce::Graphics& g, juce::Point<float> o)
{
    for (int i = 0; i < 3; ++i)
    {
        const float bx = o.x + 14.0f + 374.0f * (float) i;
        box (g, { bx, o.y + 16, 360, 224 }, "STAGE " + juce::String (i + 1));
        const juce::Rectangle<float> v = stageGraph (i);
        g.setColour (juce::Colour (0xff0b0b14));
        g.fillRoundedRectangle (v, 2.0f);
        const bool held = drag_ >= 0 && controls_[(size_t) drag_].kind == Kind::Graph && controls_[(size_t) drag_].param == ui::stageAmountParam (i);
        g.setColour (held ? kInd.withAlpha (0.8f) : juce::Colour (0xff33354a));
        g.drawRoundedRectangle (v, 2.0f, 1.0f);
        g.setColour (juce::Colour (0xff33354a));
        g.drawLine (v.getCentreX(), v.getY(), v.getCentreX(), v.getBottom(), 0.6f);
        g.drawLine (v.getX(), v.getCentreY(), v.getRight(), v.getCentreY(), 0.6f);
        // Identity (dashed) and the live transfer curve, in shaper units (1 = 5 V): x +-1.25, y +-2.1.
        auto map = [&v] (double x, double y) {
            return juce::Point<float> (v.getCentreX() + (float) (x / 1.25) * v.getWidth() * 0.5f, v.getCentreY() - (float) (y / 2.1) * v.getHeight() * 0.5f);
        };
        juce::Path id, curve;
        id.startNewSubPath (map (-1.25, -1.25)); id.lineTo (map (1.25, 1.25));   // y spans +-2.1: an offset fold reaches 2
        const float dashes[] = { 3.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (0.7f).createDashedStroke (dashed, id, dashes, 2);
        g.setColour (kDim.withAlpha (0.5f));
        g.fillPath (dashed);
        for (int k = 0; k <= 120; ++k)
        {
            const double x = -1.25 + 2.5 * k / 120.0;
            const auto pt = map (x, juce::jlimit (-2.3, 2.3, access_.stageCurve (i, x)));
            if (k == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
        }
        g.saveState();
        g.reduceClipRegion (v.toNearestInt());
        g.setColour (kInd);
        g.strokePath (curve, juce::PathStrokeType (1.6f));
        g.restoreState();
        help (g, v.getCentreX(), v.getBottom() + 13, juce::String (juce::CharPointer_UTF8 ("drag to set STAGE ")) + juce::String (i + 1)
                  + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 double-click resets")), 9, juce::Justification::horizontallyCentred, kDim, false);
        help (g, bx + 180, o.y + 213, "right-click: the whole list", 9, juce::Justification::horizontallyCentred, kDim, false);
    }
    help (g, o.x + kFaceW * 0.5f, o.y + 250, juce::String (juce::CharPointer_UTF8 (mode_ == Mode::Plugin ? "VC source: a patched VC jack wins \xc2\xb7 INPUT = the signal itself \xc2\xb7 FOLLOW = envelope \xc2\xb7 SIDECHAIN = plugin sidechain"
                                                                 : "VC source: a patched VC jack wins \xc2\xb7 INPUT = the signal itself \xc2\xb7 FOLLOW = envelope \xc2\xb7 SIDECHAIN = SC jacks (back)")),
          9, juce::Justification::horizontallyCentred, kDim, false);
}

void OrigamiPanel::paintDynamics (juce::Graphics& g, juce::Point<float> o)
{
    box (g, { o.x + 14, o.y + 16, 547, 224 }, "FOLLOWER");
    box (g, { o.x + 575, o.y + 16, 547, 224 }, "VCA");
    const juce::Rectangle<float> m (o.x + 87, o.y + 176, 400, 8);
    g.setColour (juce::Colour (0xff0b0b10)); g.fillRoundedRectangle (m, 4.0f);
    g.setColour (juce::Colour (0xff2a2a33)); g.drawRoundedRectangle (m, 4.0f, 1.0f);
    g.setColour (kInd.withAlpha (0.8f));
    g.fillRoundedRectangle (m.reduced (1.0f).withWidth ((m.getWidth() - 2.0f) * juce::jlimit (0.0f, 1.0f, access_.follower())), 3.0f);
    help (g, o.x + 287, o.y + 204, juce::String (juce::CharPointer_UTF8 ("stereo-linked peak follower \xc2\xb7 1.0 = 5 V")), 9, juce::Justification::horizontallyCentred, kDim);
    help (g, o.x + 848, o.y + 204, juce::String (juce::CharPointer_UTF8 ("drive = 1 \xe2\x88\x92 DEPTH + DEPTH \xc2\xb7 E   (EXP: E squared)")), 9, juce::Justification::horizontallyCentred, kDim);
    help (g, o.x + 848, o.y + 218, juce::String (juce::CharPointer_UTF8 ("E = follower, or VCA CV / 5 V \xc3\x97 CV SCALE")), 9, juce::Justification::horizontallyCentred, kDim);
    text (g, o.x + 848, o.y + 140, "VCA SOURCE", 7.5f, juce::Justification::horizontallyCentred, kDim);
    text (g, o.x + 958, o.y + 125, "VCA LAW", 8.5f, juce::Justification::horizontallyCentred);
}

void OrigamiPanel::paintSetup (juce::Graphics& g, juce::Point<float> o)
{
    box (g, { o.x + 14, o.y + 16, 360, 224 }, "QUALITY");
    box (g, { o.x + 388, o.y + 16, 360, 224 }, "PANEL");
    box (g, { o.x + 762, o.y + 16, 360, 224 }, "LEVEL");
    const juce::Rectangle<float> lcd (o.x + 64, o.y + 112, 260, 30);
    g.setColour (juce::Colour (0xff0b0b14)); g.fillRoundedRectangle (lcd, 2.0f);
    g.setColour (juce::Colour (0xff33354a)); g.drawRoundedRectangle (lcd, 2.0f, 1.0f);
    const int lat = access_.latencySamples();
    const juce::String ms = juce::String (1000.0 * lat / juce::jmax (1.0, access_.sampleRate()), 2);
    g.setColour (kInd);
    g.setFont (juce::Font (juce::FontOptions {}.withName (juce::Font::getDefaultMonospacedFontName()).withPointHeight (10.0f)));
    g.drawText (statusText() + " smp  " + ms + " ms", lcd, juce::Justification::centred);
    help (g, o.x + 194, o.y + 168, juce::String (juce::CharPointer_UTF8 ("2\xc3\x97: 93-tap half-band up + down \xc2\xb7 dry delayed to match")), 9, juce::Justification::horizontallyCentred, kDim);
    help (g, o.x + 194, o.y + 182, "default OFF; the latency is reported to the host / rack", 9, juce::Justification::horizontallyCentred, kDim);

    if (mode_ == Mode::Plugin)
    {
        text (g, o.x + 568, o.y + 58, "UI SCALE", 8.5f);
        for (int i = 0; i < 5; ++i)
        {
            const juce::Rectangle<float> b (o.x + 568.0f + (float) (i - 2) * 64.0f - 28.0f, o.y + 69.0f, 56.0f, 22.0f);
            const bool on = std::abs (uiScale - kScales[i]) < 0.01f;
            g.setColour (on ? kInd : juce::Colour (0xff16161b)); g.fillRoundedRectangle (b, 3.0f);
            g.setColour (juce::Colour (0xff30324a)); g.drawRoundedRectangle (b, 3.0f, 1.0f);
            text (g, b.getCentreX(), b.getBottom() - 7.0f, juce::String (juce::roundToInt (kScales[i] * 100.0f)) + " %", 8, juce::Justification::horizontallyCentred,
                  on ? juce::Colour (0xff0b0b12) : kInk);
        }
        help (g, o.x + 568, o.y + 118, juce::String (juce::CharPointer_UTF8 ("900 \xc3\x97 270 to 2400 \xc3\x97 720 \xc2\xb7 drag the corner too")), 9, juce::Justification::horizontallyCentred, kDim);
        text (g, o.x + 568, o.y + 168, juce::String (juce::CharPointer_UTF8 ("CABLE COLOUR \xc2\xb7 BY ROLE")), 8.5f);
        help (g, o.x + 568, o.y + 182, "set in the rack (ring colours follow the cable standard)", 9, juce::Justification::horizontallyCentred, kDim);
    }
    else
    {
        help (g, o.x + 568, o.y + 100, "UI SCALE follows the rack", 9.5f, juce::Justification::horizontallyCentred, kInk);
        help (g, o.x + 568, o.y + 150, juce::String (juce::CharPointer_UTF8 ("CABLE COLOUR \xc2\xb7 rack header (BY ROLE / MANUAL)")), 9.5f, juce::Justification::horizontallyCentred, kInk);
    }
    text (g, o.x + 870, o.y + 84, "LEVEL COMP", 8.5f, juce::Justification::right);
    text (g, o.x + 870, o.y + 144, "BYPASS", 8.5f, juce::Justification::right);
    help (g, o.x + 942, o.y + 182, juce::String (juce::CharPointer_UTF8 ("LEVEL COMP: 20 ms RMS, \xc2\xb1" "12 dB, 5 ms smoothing")), 9, juce::Justification::horizontallyCentred, kDim);
    help (g, o.x + 942, o.y + 196, "BYPASS passes the dry signal bit-exact", 9, juce::Justification::horizontallyCentred, kDim);
}

void OrigamiPanel::paintJackRow (juce::Graphics& g, juce::Point<float> o)
{
    g.setColour (kAcc);
    g.drawLine (o.x + 14, o.y + 254, o.x + kFaceW - 14, o.y + 254, 0.8f);
    for (int j = 0; j < kJackCount; ++j)
    {
        const float cx = o.x + kJackX[j], cy = o.y + kJackY;
        const juce::Colour role = jackIsAudio (j) ? kAudio : kCv;
        g.setColour (role.withAlpha (0.9f));
        g.drawEllipse (cx - 11.5f, cy - 11.5f, 23.0f, 23.0f, 1.6f);
        radial (g, cx, cy, 9.5f, juce::Colour (0xffe2e2dc), juce::Colour (0xff8f8f8a), juce::Colour (0xff3a3a38));
        g.setColour (juce::Colour (0xff2a2a2c));
        g.drawEllipse (cx - 9.5f, cy - 9.5f, 19.0f, 19.0f, 0.9f);
        radial (g, cx, cy, 6.4f, juce::Colour (0xff5a5a58), juce::Colour (0xff383836), juce::Colour (0xff161616));
        g.setColour (juce::Colour (0xff030303));
        g.fillEllipse (cx - 3.8f, cy - 3.8f, 7.6f, 7.6f);
        const juce::String label (kJackLabels[j]);
        if (jackIsOutput (j))
        {
            const float w = (float) label.length() * 6.2f + 10.0f;
            g.setColour (kInk);
            g.fillRoundedRectangle (cx - w * 0.5f, cy + 15, w, 12, 1.5f);
            text (g, cx, cy + 24.5f, label, 8, juce::Justification::horizontallyCentred, juce::Colour (0xff111111));
        }
        else
            text (g, cx, cy + 25, label, 8);
        text (g, cx + 14, cy - 9, juce::String (juce::CharPointer_UTF8 (jackIsAudio (j) ? "\xe2\x88\xbf" : "\xe2\x89\x88")), 8, juce::Justification::left, role, true, 0.0f);
    }
    help (g, o.x + kFaceW * 0.5f, o.y + 290, juce::String (juce::CharPointer_UTF8 ("VC 1\xe2\x80\x93" "3: audio rate OK \xc2\xb7 ring = cable role")), 9,
          juce::Justification::horizontallyCentred, kDim);
}

void OrigamiPanel::paintClosed (juce::Graphics& g)
{
    juce::ColourGradient pl (juce::Colour (0xff1b1b20), 0, 0, juce::Colour (0xff101014), 0, kClosedH, false);
    g.setGradientFill (pl);
    g.fillRect (0.0f, 0.0f, kFaceW, kClosedH);
    g.setColour (kAcc);
    g.drawRoundedRectangle (10, 9, kFaceW - 20, kClosedH - 18, 2, 1.1f);
    text (g, 30, 30, "ORIGAMI", 12, juce::Justification::left, kInk, true, 0.3f);
    text (g, 30, 46, "TRIPLE WAVE SHAPER", 7.5f, juce::Justification::left, kDim, true, 0.15f);
    text (g, kFaceW - 30, 30, statusText(), 8, juce::Justification::right, kDim);
    help (g, kFaceW - 30, 46, juce::String (juce::CharPointer_UTF8 ("CLOSED \xc2\xb7 jacks on the back")), 9, juce::Justification::right, kDim, false);
    // IN meter | BYPASS | MIX  [WAVE]  LEVEL | OVER | OUT meter  (mirror-symmetric)
    text (g, 105, 54, "IN L / R", 7, juce::Justification::horizontallyCentred, kDim);
    text (g, kFaceW - 105, 54, "OUT L / R", 7, juce::Justification::horizontallyCentred, kDim);
    for (int ch = 0; ch < 2; ++ch)
    {
        paintMeter (g, { 30, 58.0f + 9.0f * (float) ch, 150, 6 }, access_.inPeak (ch));
        paintMeter (g, { kFaceW - 180, 58.0f + 9.0f * (float) ch, 150, 6 }, access_.outPeak (ch));
    }
    led (g, 270, 64, access_.get (kBypass) >= 0.5, juce::Colour (0xfff7b733));
    led (g, kFaceW - 270, 41, access_.over(), kAudio);
    text (g, kFaceW - 270, 64, "OVER", 8);
    text (g, kFaceW * 0.5f, 80, "WAVE", 9, juce::Justification::horizontallyCentred, kInk, true, 0.3f);
}

void OrigamiPanel::paintControl (juce::Graphics& g, const Control& c)
{
    const double v = access_.get (c.param);
    const auto& info = paramInfo (c.param);
    const float cx = c.c.x, cy = c.c.y, r = c.r;
    switch (c.kind)
    {
        case Kind::Knob:
        {
            const int ticks = c.bigTicks ? 21 : 11;
            g.setColour (juce::Colour (0xffbdbab0));
            for (int i = 0; i < ticks; ++i)
            {
                const float a = juce::degreesToRadians (-135.0f + 270.0f * (float) i / (float) (ticks - 1) - 90.0f);
                const float l = (i == 0 || i == ticks - 1 || i == (ticks - 1) / 2) ? 3.4f : 2.0f;
                g.drawLine (cx + (r + 3) * std::cos (a), cy + (r + 3) * std::sin (a), cx + (r + 3 + l) * std::cos (a), cy + (r + 3 + l) * std::sin (a), 0.9f);
            }
            // Value arc: from the minimum, or from the centre for bipolar controls.
            const float n = (float) toNormal (c.param, v);
            const float from = info.min < 0.0 ? 0.5f : 0.0f;
            if (std::abs (n - from) > 0.002f)
            {
                juce::Path arc;
                const float a0 = juce::degreesToRadians (-135.0f + 270.0f * juce::jmin (from, n));
                const float a1 = juce::degreesToRadians (-135.0f + 270.0f * juce::jmax (from, n));
                arc.addCentredArc (cx, cy, r + 1.4f, r + 1.4f, 0.0f, a0, a1, true);
                g.setColour (kInd);
                g.strokePath (arc, juce::PathStrokeType (2.6f));
            }
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (cx - r, cy - r + 1.5f, 2 * r, 2 * r);
            radial (g, cx, cy, r, juce::Colour (0xff3a3a3a), juce::Colour (0xff151515), juce::Colour (0xff080808));
            g.setColour (juce::Colour (0xff050505));
            g.drawEllipse (cx - r, cy - r, 2 * r, 2 * r, 1.0f);
            radial (g, cx, cy, r * 0.68f, juce::Colour (0xffe6e2d6), juce::Colour (0xff97938a), juce::Colour (0xff3b3a37));
            const float a = juce::degreesToRadians (-135.0f + 270.0f * n - 90.0f);
            g.setColour (juce::Colour (0xfff5f4ee));
            g.drawLine (cx + r * 0.15f * std::cos (a), cy + r * 0.15f * std::sin (a), cx + r * 0.92f * std::cos (a), cy + r * 0.92f * std::sin (a), 2.0f);
            if (c.label.isNotEmpty())
                text (g, cx, cy + r + 13, c.label, r >= 20 ? 8.5f : 8.0f);
            break;
        }
        case Kind::Toggle:
        {
            const bool right = v >= 0.5;
            g.setColour (juce::Colour (0xff050505));
            g.fillRoundedRectangle (cx - 10, cy - 5, 20, 10, 5);
            g.setColour (juce::Colour (0xff3a3d3a));
            g.drawRoundedRectangle (cx - 10, cy - 5, 20, 10, 5, 1.0f);
            radial (g, cx + (right ? 5.0f : -5.0f), cy, 3.8f, juce::Colour (0xffe6e2d6), juce::Colour (0xff97938a), juce::Colour (0xff3b3a37));
            if (c.left.isNotEmpty()) text (g, cx - 14, cy + 3, c.left, 7.5f, juce::Justification::right, right ? kDim : kInk);
            if (c.right.isNotEmpty()) text (g, cx + 14, cy + 3, c.right, 7.5f, juce::Justification::left, right ? kInk : kDim);
            break;
        }
        case Kind::Selector:
        {
            const juce::Rectangle<float> b (cx - r, cy - 9, 2 * r, 18);
            g.setColour (juce::Colour (0xff0b0b14)); g.fillRoundedRectangle (b, 2.0f);
            g.setColour (juce::Colour (0xff33354a)); g.drawRoundedRectangle (b, 2.0f, 1.0f);
            const int idx = juce::jlimit (0, info.choices - 1, (int) std::lround (v));
            g.setColour (kInd);
            g.setFont (juce::Font (juce::FontOptions {}.withName (juce::Font::getDefaultMonospacedFontName()).withPointHeight (9.0f)));
            g.drawText (c.label + ": " + info.choiceNames[idx], b, juce::Justification::centred);
            break;
        }
        case Kind::Graph:
            break;   // drawn by paintStages
        case Kind::Button:
        {
            const juce::Rectangle<float> b (cx - r, cy - 11, 2 * r, 22);
            const bool on = v >= 0.5;
            g.setColour (on ? juce::Colour (0xff2a2a33) : juce::Colour (0xff16161b)); g.fillRoundedRectangle (b, 3.0f);
            g.setColour (juce::Colour (0xff30324a)); g.drawRoundedRectangle (b, 3.0f, 1.0f);
            text (g, cx, cy + 4, c.label, 8, juce::Justification::horizontallyCentred, on ? juce::Colour (0xfff7b733) : kInk);
            break;
        }
    }
}
