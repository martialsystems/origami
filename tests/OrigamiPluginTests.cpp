// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// ORIGAMI plugin tests (no host, no audio device): testFxStandaloneNormals, latency reporting, state format and
// round trip, the factory presets (load, round trip, level on a test signal), the editor's sizes and controls. Writes PNGs of the editor and the rack panel modes when given an
// output directory:  OrigamiPluginTests [out-dir]

#include "origami/plugin/OrigamiEditor.h"
#include "origami/plugin/OrigamiPresets.h"
#include "origami/plugin/OrigamiProcessor.h"
#include "origami/plugin/OrigamiState.h"
#include "origami/plugin/OrigamiUiLogic.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <map>
#include <set>

// The checks below compare saved, restored and rendered values exactly on purpose (a round trip must be bit-exact),
// so -Wfloat-equal is off for this file only; ORIGAMI's product code keeps it.
#if defined (__GNUC__) || defined (__clang__)
 #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

using namespace origami;

namespace {

int checks = 0, failures = 0;
void check (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok) { ++failures; std::printf ("FAIL %s\n", what.toRawUTF8()); }
}

void snapshot (juce::Component& c, const juce::File& file, float scale = 1.0f)
{
    const auto image = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
    std::printf ("wrote %s (%d x %d)\n", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
}

juce::MouseEvent mouse (juce::Component& c, juce::Point<float> p, juce::Point<float> down, bool dragged)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    return juce::MouseEvent (source, p, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &c, &c, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
}

juce::MouseEvent mouseWith (juce::Component& c, juce::Point<float> p, juce::Point<float> down, bool dragged, juce::ModifierKeys mods)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    return juce::MouseEvent (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
}

void run (OrigamiProcessor& p, juce::AudioBuffer<float>& b)
{
    juce::MidiBuffer m;
    p.processBlock (b, m);
}

void testStandaloneNormals()
{
    OrigamiProcessor p;
    p.prepareToPlay (48000.0, 512);
    check (p.getLatencySamples() == 0, "INIT reports latency 0");
    check (p.value (kWave) == 0.0 && p.value (kLevelComp) == 1.0 && p.value (kQuality) == 0.0, "INIT: WAVE 0, LEVEL COMP ON, 2x OFF");
    juce::Random r (7);
    juce::AudioBuffer<float> b (2, 512), ref (2, 512);
    bool exact = true;
    for (int block = 0; block < 8; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                b.setSample (c, i, (r.nextFloat() * 2.0f - 1.0f) * (c == 0 ? 1.0f : 0.3f));
        ref.makeCopyOf (b);
        run (p, b);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                exact = exact && b.getSample (c, i) == ref.getSample (c, i);
    }
    check (exact, "testFxStandaloneNormals: host in -> IN L/R -> OUT L/R -> host out, bit-exact at INIT (x5 / /5 exact)");

    // Folding something: stays finite, and L == R for identical input (a fresh instance: no L/R history).
    OrigamiProcessor q;
    q.prepareToPlay (48000.0, 512);
    q.setValue (kWave, 0.6);
    for (int i = 0; i < 512; ++i) b.setSample (0, i, 0.8f * std::sin ((float) i * 0.05f)), b.setSample (1, i, b.getSample (0, i));
    run (q, b); run (q, b);
    bool finite = true, linked = true;
    for (int i = 0; i < 512; ++i) { finite = finite && std::isfinite (b.getSample (0, i)); linked = linked && b.getSample (0, i) == b.getSample (1, i); }
    check (finite && linked, "WAVE 0.6: finite, L == R for identical input");

    p.setValue (kQuality, 1.0);
    run (p, b);
    check (p.getLatencySamples() == kLatency2x, "QUALITY 2x reports 46 samples to the host");
    p.setValue (kQuality, 0.0);
    run (p, b);
    check (p.getLatencySamples() == 0, "back to 1x reports 0");
    check (p.getBypassParameter() != nullptr && p.getBypassParameter()->getName (32) == "BYPASS", "host bypass maps to the BYPASS parameter");
}

void testSidechain()
{
    OrigamiProcessor p;
    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    check (p.setBusesLayout (layout), "sidechain bus can be enabled");
    p.prepareToPlay (48000.0, 256);
    p.setValue (kWave, 0.5);
    p.setValue (kVc1Src, 3.0);    // SIDECHAIN
    p.setValue (kVc1Amt, 1.0);
    p.setValue (kLevelComp, 0.0);
    juce::AudioBuffer<float> b (4, 256);
    auto fill = [&b] (float sc) {
        for (int i = 0; i < 256; ++i)
        {
            b.setSample (0, i, 0.5f * std::sin ((float) i * 0.07f)); b.setSample (1, i, b.getSample (0, i));
            b.setSample (2, i, sc); b.setSample (3, i, sc);
        }
    };
    fill (0.0f); run (p, b); fill (0.0f); run (p, b);
    const float quiet = b.getSample (0, 100);
    fill (0.8f); run (p, b); fill (0.8f); run (p, b);
    check (std::abs (b.getSample (0, 100) - quiet) > 1e-3f, "VC 1 SOURCE = SIDECHAIN: the sidechain moves the fold");
}

void testState()
{
    OrigamiProcessor a;
    a.setValue (kWave, 0.42);
    a.setValue (kSym2, -0.3);
    a.setValue (kVc3Src, 2.0);
    a.setValue (kRelease, 250.0);
    a.uiScale = 1.25f;
    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    OrigamiProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    bool same = true;
    for (int p = 0; p < kParamCount; ++p)
        same = same && std::abs (a.value (p) - b.value (p)) < 1e-4 * (paramInfo (p).max - paramInfo (p).min);
    check (same && b.uiScale == 1.25f, "state round trip (every param, UI scale)");

    auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
    check (xml != nullptr && xml->hasTagName ("ORIGAMI") && xml->getIntAttribute ("format") == 1 && xml->getStringAttribute ("unit") == "ORIGAMI",
           "state is format 1, unit ORIGAMI (the rack device's child format)");
    check (xml != nullptr && xml->getNumChildElements() == kParamCount, "one PARAM per id");
    double v[kParamCount];
    check (xml != nullptr && stateFromXml (*xml, v).ok && std::abs (v[kWave] - 0.42) < 1e-6 && v[kVc3Src] == 2.0, "the shared reader (used by the rack) loads the plugin state");

    juce::XmlElement newer ("ORIGAMI");
    newer.setAttribute ("format", 2);
    newer.setAttribute ("unit", "ORIGAMI");
    for (int p = 0; p < kParamCount; ++p) v[p] = -99.0;
    check (! stateFromXml (newer, v).ok && v[0] == -99.0, "a newer format is refused and leaves the values alone");
    juce::XmlElement partial ("ORIGAMI");
    partial.setAttribute ("format", 1);
    auto* e = partial.createNewChildElement ("PARAM"); e->setAttribute ("id", "wave"); e->setAttribute ("value", 7.0);
    auto* u = partial.createNewChildElement ("PARAM"); u->setAttribute ("id", "future_knob"); u->setAttribute ("value", 1.0);
    check (stateFromXml (partial, v).ok && v[kWave] == 1.0 && v[kMix] == 1.0 && v[kLevelComp] == 1.0, "unknown ids ignored, missing ids default, values clamped");
}

void testEditor (const juce::File& outDir)
{
    OrigamiProcessor p;
    p.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* e = dynamic_cast<OrigamiEditor*> (ed.get());
    check (e != nullptr && e->getWidth() == 1200 && e->getHeight() == 360, "editor opens at 1200 x 360");
    if (e == nullptr) return;
    auto& panel = e->panel();

    // Drag WAVE up by 100 px: half the range.
    const auto c = panel.controlCentre (kWave);
    panel.mouseDown (mouse (panel, c, c, false));
    for (int i = 1; i <= 10; ++i) panel.mouseDrag (mouse (panel, c - juce::Point<float> (0, 10.0f * (float) i), c, true));
    panel.mouseUp (mouse (panel, c - juce::Point<float> (0, 100), c, true));
    check (std::abs (p.value (kWave) - 0.5) < 0.01, "dragging WAVE 100 px sets it to 0.5, got " + juce::String (p.value (kWave)));
    const auto t = panel.controlCentre (kLevelComp);
    panel.mouseDown (mouse (panel, t, t, false)); panel.mouseUp (mouse (panel, t, t, false));
    check (p.value (kLevelComp) == 0.0, "clicking LEVEL COMP turns it off");
    panel.mouseDown (mouse (panel, t, t, false)); panel.mouseUp (mouse (panel, t, t, false));
    panel.mouseDoubleClick (mouse (panel, c, c, false));
    check (p.value (kWave) == 0.0, "double-click resets WAVE to 0 (true bypass)");

    // Preset box: the menu has INIT and one submenu per bank; > and < step through the bank and wrap.
    {
        const auto menu = panel.presetMenu();
        juce::StringArray tops;
        int subItems = 0;
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        {
            tops.add (it.getItem().text);
            if (auto* sub = it.getItem().subMenu.get())
                for (juce::PopupMenu::MenuItemIterator si (*sub); si.next();) ++subItems;
        }
        check (tops == juce::StringArray ({ "INIT", "RONIN", "SHOGUN", "BUSHIDO", "GENERIC" }) && subItems + 1 == p.getNumPrograms(),
               "preset menu: INIT, then RONIN, SHOGUN, BUSHIDO, GENERIC submenus holding every preset (" + tops.joinIntoString (", ") + ")");
        const auto next = panel.presetPartCentre (OrigamiPanel::PresetPart::Next);
        const auto prev = panel.presetPartCentre (OrigamiPanel::PresetPart::Prev);
        panel.mouseDown (mouse (panel, next, next, false)); panel.mouseUp (mouse (panel, next, next, false));
        const auto& first = factoryPresets()[1];
        check (p.getCurrentProgram() == 1 && std::abs (p.value (kWave) - first.values[kWave]) < 1e-5,
               "preset > loads the next preset (" + p.getProgramName (1) + ")");
        if (outDir != juce::File())
            snapshot (panel, outDir.getChildFile ("origami_standalone_preset.png"));
        panel.mouseDown (mouse (panel, prev, prev, false)); panel.mouseUp (mouse (panel, prev, prev, false));
        panel.mouseDown (mouse (panel, prev, prev, false)); panel.mouseUp (mouse (panel, prev, prev, false));
        check (p.getCurrentProgram() == p.getNumPrograms() - 1, "preset < wraps from INIT to the last preset");
        panel.mouseDown (mouse (panel, next, next, false)); panel.mouseUp (mouse (panel, next, next, false));
        check (p.getCurrentProgram() == 0 && p.value (kWave) == 0.0, "preset > wraps back to INIT");
    }

    // Jacks: 8, symmetric about the centre line, >= 16 px targets at the smallest scale.
    bool sym = true;
    for (int j = 0; j < 4; ++j)
        sym = sym && std::abs ((panel.jackCentre (j).x + panel.jackCentre (7 - j).x) - 2.0f * (32.0f + 568.0f)) < 0.5f;
    check (sym, "front jack row is mirror-symmetric (4 | 4)");

    // A demo setting for the pictures.
    p.setValue (kWave, 0.5); p.setValue (kGain, 3.0); p.setValue (kVcaDepth, 0.4); p.setValue (kMix, 0.75); p.setValue (kSym, 0.2);
    p.setValue (kStage3, -0.2); p.setValue (kSym2, 0.4); p.setValue (kVc1Amt, 0.5); p.setValue (kVc2Src, 2.0);
    juce::AudioBuffer<float> b (2, 2048);
    for (int i = 0; i < 2048; ++i) { b.setSample (0, i, 0.7f * std::sin ((float) i * 0.03f)); b.setSample (1, i, 0.6f * std::sin ((float) i * 0.031f)); }
    run (p, b);
    if (outDir != juce::File())
    {
        snapshot (panel, outDir.getChildFile ("origami_standalone_main.png"));
        snapshot (panel, outDir.getChildFile ("origami_standalone_main@2x.png"), 2.0f);
    }
    for (auto pg : { Page::Stages, Page::Dynamics, Page::Setup })
    {
        const auto tc = panel.tabCentre (pg);
        panel.mouseDown (mouse (panel, tc, tc, false)); panel.mouseUp (mouse (panel, tc, tc, false));
        check (panel.page() == pg, juce::String ("tab ") + OrigamiPanel::pageName (pg));
        if (outDir != juce::File())
            snapshot (panel, outDir.getChildFile (juce::String ("origami_standalone_") + juce::String (OrigamiPanel::pageName (pg)).toLowerCase() + ".png"));
    }
    // SETUP: UI SCALE 75 % and 200 % resize the editor.
    const auto s75 = panel.toLocal ({ 32.0f + 568.0f - 128.0f, 28.0f + 80.0f });
    panel.mouseDown (mouse (panel, s75, s75, false)); panel.mouseUp (mouse (panel, s75, s75, false));
    check (e->getWidth() == 900 && e->getHeight() == 270, "UI SCALE 75 % -> 900 x 270");
    const auto s200 = panel.toLocal ({ 32.0f + 568.0f + 128.0f, 28.0f + 80.0f });
    panel.mouseDown (mouse (panel, s200, s200, false)); panel.mouseUp (mouse (panel, s200, s200, false));
    check (e->getWidth() == 2400 && e->getHeight() == 720 && std::abs (p.uiScale - 2.0f) < 0.01f, "UI SCALE 200 % -> 2400 x 720");
    e->setSize (900, 270);
    check (panel.jackRadius() * 2.0f >= 16.0f, "jack targets >= 16 px at 75 %");

    // The rack modes of the same panel (the rack device binds it to OrigamiDevice; here to the plugin).
    struct Bind : OrigamiPanel::Access
    {
        OrigamiProcessor& p;
        explicit Bind (OrigamiProcessor& q) : p (q) {}
        double get (int i) const override { return p.value (i); }
        void set (int i, double v) override { p.setValue (i, v); }
        float inPeak (int ch) const override { return p.core().inPeak (ch); }
        float outPeak (int ch) const override { return p.core().outPeak (ch); }
        float follower() const override { return p.core().follower(); }
        bool over() const override { return p.core().overLit(); }
        int latencySamples() const override { return p.value (kQuality) >= 0.5 ? kLatency2x : 0; }
        double stageCurve (int s, double x) const override
        {
            double v[kParamCount];
            for (int i = 0; i < kParamCount; ++i) v[i] = p.value (i);
            return OrigamiCore::stageCurve (v, s, x);
        }
        // The factory bank, as the rack device offers it.
        int presetCount() const override { return (int) factoryPresets().size(); }
        juce::String presetName (int i) const override { return factoryPresets()[(size_t) i].name; }
        juce::String presetBank (int i) const override { return factoryPresets()[(size_t) i].bank; }
        int currentPreset() const override { return p.getCurrentProgram(); }
        void loadPreset (int i) override { p.setCurrentProgram (i); }
    } bind (p);
    OrigamiPanel open (bind, OrigamiPanel::Mode::RackOpen), closed (bind, OrigamiPanel::Mode::RackClosed);
    open.setSize (1136, 332);
    closed.setSize (1136, 87);
    check (closed.jackAt (closed.getLocalBounds().getCentre().toFloat()) == -1 && closed.jackCentre (0) == juce::Point<float>(), "CLOSED shows no jacks");
    check (open.jackCentre (0) == juce::Point<float> (80.0f, 284.0f), "OPEN face jack IN L at (80, 284)");
    const auto bc = closed.controlCentre (kBypass);
    closed.mouseDown (mouse (closed, bc, bc, false)); closed.mouseUp (mouse (closed, bc, bc, false));
    check (p.value (kBypass) == 1.0, "CLOSED strip BYPASS button");
    closed.mouseDown (mouse (closed, bc, bc, false)); closed.mouseUp (mouse (closed, bc, bc, false));
    // The rack face carries the preset box in the jack row, between VC 1 and VC 2 (the rack strip has no room).
    {
        const auto prev = open.presetPartCentre (OrigamiPanel::PresetPart::Prev), next = open.presetPartCentre (OrigamiPanel::PresetPart::Next);
        check (prev.x > open.jackCentre (OrigamiPanel::Vc1).x + 12.0f && next.x < open.jackCentre (OrigamiPanel::Vc2).x - 12.0f
                   && std::abs (prev.y - next.y) < 0.01f && prev.y < open.jackCentre (OrigamiPanel::Vc1).y,
               "RACK OPEN preset box sits in the jack row between VC 1 and VC 2");
        const int before = p.getCurrentProgram();
        double saved[kParamCount];
        for (int i = 0; i < kParamCount; ++i) saved[i] = p.value (i);
        open.mouseDown (mouse (open, next, next, false)); open.mouseUp (mouse (open, next, next, false));
        const int stepped = p.getCurrentProgram();
        open.mouseDown (mouse (open, prev, prev, false)); open.mouseUp (mouse (open, prev, prev, false));
        check (stepped == (before + 1) % (int) factoryPresets().size() && p.getCurrentProgram() == before,
               "RACK OPEN preset box arrows step the factory bank");
        for (int i = 0; i < kParamCount; ++i) p.setValue (i, saved[i]);     // the snapshots below show the state as before
    }
    if (outDir != juce::File())
    {
        snapshot (open, outDir.getChildFile ("origami_rack_open_face.png"));
        snapshot (closed, outDir.getChildFile ("origami_rack_closed_strip.png"));
    }
}

// The interaction rules as plain functions (OrigamiUiLogic.h).
void testUiLogic()
{
    using namespace origami::ui;
    check (dragToNormal (0.5, 100.0, 1.0, false) == 1.0 && dragToNormal (0.5, 50.0, 1.0, false) == 0.75 && dragToNormal (0.5, -300.0, 1.0, false) == 0.0,
           "drag: 200 px is the whole range, clamped at both ends");
    check (std::abs (dragToNormal (0.5, 100.0, 1.0, true) - 0.6) < 1e-12 && std::abs (dragToNormal (0.5, 100.0, 2.0, false) - 0.75) < 1e-12,
           "drag: Shift is 5x finer, and the travel scales with the UI");
    check (stepChoice (0, 4, false) == 1 && stepChoice (3, 4, false) == 0 && stepChoice (0, 4, true) == 3 && stepChoice (2, 4, true) == 1,
           "list step: click forward, Shift-click back, wrapping both ways");
    check (stepChoice (9, 4, false) == 0 && stepChoice (-3, 4, true) == 3 && stepChoice (0, 0, false) == 0, "list step: out-of-range input is clamped first");
    const auto l = choiceList (kVc2Src, 2.0);
    check (l.names == std::vector<std::string> ({ "JACK", "INPUT", "FOLLOW", "SIDECHAIN" }) && l.current == 2, "VC SOURCE list: every source in order, current ticked");
    check (choiceList (kWave, 0.5).names.empty(), "a continuous parameter has no list");
    check (choiceFromMenuResult (kVc1Src, 0) == -1 && choiceFromMenuResult (kVc1Src, 1) == 0 && choiceFromMenuResult (kVc1Src, 4) == 3
               && choiceFromMenuResult (kVc1Src, 5) == -1, "menu result: id - 1, dismissed or out of range = no change");
    check (stageAmountParam (0) == kStage1 && stageAmountParam (2) == kStage3 && kParamCount == 30,
           "STAGES edits the MAIN STAGE 1-3 parameters (no new parameter: still 30)");
}

// STAGES page editing and the right-click list, on the plugin face and the rack's open face.
void testStagesAndLists()
{
    struct Plain : OrigamiPanel::Access
    {
        double v[kParamCount] {};
        int begins = 0, ends = 0, lastGesture = -1;
        Plain() { for (int i = 0; i < kParamCount; ++i) v[i] = paramInfo (i).def; }
        double get (int p) const override { return v[p]; }
        void set (int p, double x) override { v[p] = clampParam (p, x); }
        void gesture (int p, bool begin) override { (begin ? begins : ends)++; lastGesture = p; }
        float inPeak (int) const override { return 0.0f; }
        float outPeak (int) const override { return 0.0f; }
        float follower() const override { return 0.0f; }
        bool over() const override { return false; }
        int latencySamples() const override { return 0; }
        double stageCurve (int s, double x) const override { return OrigamiCore::stageCurve (v, s, x); }
    };
    const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier), shift (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier),
                            right (juce::ModifierKeys::rightButtonModifier);
    for (auto mode : { OrigamiPanel::Mode::Plugin, OrigamiPanel::Mode::RackOpen })
    {
        const juce::String tag = mode == OrigamiPanel::Mode::Plugin ? "plugin: " : "rack: ";
        Plain a;
        auto panel = std::make_unique<OrigamiPanel> (a, mode);
        auto& pn = *panel;
        const auto ds = pn.designSize();
        pn.setSize ((int) ds.x, (int) ds.y);
        pn.setPage (Page::Stages);
        // Menus are captured, not opened (no window system needed); the callback is answered by the test.
        int menusShown = 0;
        std::function<void (int)> pendingMenu;
        pn.showMenu = [&menusShown, &pendingMenu] (const juce::PopupMenu&, const juce::PopupMenu::Options&, std::function<void (int)> done)
        {
            ++menusShown;
            pendingMenu = std::move (done);
        };

        // Every box carries its STAGE knob and a graph; the layout of the three boxes is identical and mirror-symmetric.
        bool shown = true, mirrored = true, same = true;
        for (int i = 0; i < 3; ++i)
        {
            const auto g = pn.stageGraphCentre (i), st = pn.controlCentre (kStage1 + i), sy = pn.controlCentre (kSym1 + i);
            const auto am = pn.controlCentre (kVc1Amt + i), vs = pn.controlCentre (kVc1Sym + i), src = pn.controlCentre (kVc1Src + i);
            shown = shown && g.x >= 0.0f && st.x >= 0.0f && sy.x >= 0.0f && am.x >= 0.0f && vs.x >= 0.0f && src.x >= 0.0f;
            mirrored = mirrored && std::abs ((st.x + sy.x) - 2.0f * g.x) < 0.01f && std::abs (st.y - sy.y) < 0.01f && std::abs (st.y - g.y) < 0.01f
                       && std::abs ((am.x + vs.x) - 2.0f * src.x) < 0.01f && std::abs (src.x - g.x) < 0.01f && std::abs (am.y - src.y) < 0.01f;
            const float dx = 374.0f * (float) i;
            same = same && std::abs (g.x - pn.stageGraphCentre (0).x - dx) < 0.01f && std::abs (st.x - pn.controlCentre (kStage1).x - dx) < 0.01f
                   && std::abs (am.x - pn.controlCentre (kVc1Amt).x - dx) < 0.01f && std::abs (src.y - pn.controlCentre (kVc1Src).y) < 0.01f;
        }
        check (shown, tag + "STAGES shows STAGE n, SYM n, VC > AMT, VC > SYM, the VC source and an editable graph in every box");
        check (mirrored && same, tag + "each box is mirror-symmetric (STAGE | graph | SYM over AMT | source | SYM) and all three are identical");

        // The STAGE knob on STAGES is the MAIN parameter: drag it, and MAIN shows the same value.
        const auto k2 = pn.controlCentre (kStage2);
        pn.mouseDown (mouseWith (pn, k2, k2, false, left));
        pn.mouseDrag (mouseWith (pn, k2 - juce::Point<float> (0, 50), k2, true, left));
        pn.mouseUp (mouseWith (pn, k2 - juce::Point<float> (0, 50), k2, true, left));
        check (std::abs (a.v[kStage2] - 0.5) < 1e-9 && a.begins == 1 && a.ends == 1 && a.lastGesture == kStage2,
               tag + "STAGE 2 knob on STAGES: 50 px up = +0.5, one gesture, got " + juce::String (a.v[kStage2]));
        pn.setPage (Page::Main);
        check (pn.controlCentre (kStage2).x >= 0.0f && pn.stageGraphCentre (1).x < 0.0f, tag + "MAIN still has STAGE 2 (the same parameter); graphs only on STAGES");
        pn.setPage (Page::Stages);

        // The graph is a control: a vertical drag sets STAGE n, Shift is fine, double-click resets, and the curve follows.
        const auto g1 = pn.stageGraphCentre (0);
        const double before = a.stageCurve (0, 0.9);
        a.v[kWave] = 0.3;
        const double atWave = a.stageCurve (0, 0.9);
        pn.mouseDown (mouseWith (pn, g1, g1, false, left));
        for (int s = 1; s <= 4; ++s) pn.mouseDrag (mouseWith (pn, g1 - juce::Point<float> (0, 10.0f * (float) s), g1, true, left));
        pn.mouseUp (mouseWith (pn, g1 - juce::Point<float> (0, 40), g1, true, left));
        check (std::abs (a.v[kStage1] - 0.4) < 1e-9 && a.begins == 2 && a.ends == 2 && a.lastGesture == kStage1,
               tag + "dragging graph 1 up 40 px sets STAGE 1 to +0.4 as one gesture, got " + juce::String (a.v[kStage1]));
        check (std::abs (a.stageCurve (0, 0.9) - atWave) > 1e-6 && before != atWave, tag + "the drawn curve follows the graph drag");
        pn.mouseDown (mouseWith (pn, g1, g1, false, shift));
        pn.mouseDrag (mouseWith (pn, g1 + juce::Point<float> (0, 100), g1, true, shift));
        pn.mouseUp (mouseWith (pn, g1 + juce::Point<float> (0, 100), g1, true, shift));
        check (std::abs (a.v[kStage1] - 0.2) < 1e-9, tag + "Shift-drag on the graph is 5x finer (100 px down = -0.2), got " + juce::String (a.v[kStage1]));
        pn.mouseDoubleClick (mouseWith (pn, g1, g1, false, left));
        check (a.v[kStage1] == 0.0 && a.begins == a.ends, tag + "double-click on graph 1 resets STAGE 1 to 0, gestures balanced");
        const auto g3 = pn.stageGraphCentre (2);
        pn.mouseWheelMove (mouseWith (pn, g3, g3, false, juce::ModifierKeys()), juce::MouseWheelDetails { 0.0f, 1.0f, false, false, false });
        check (a.v[kStage3] > 0.0 && a.v[kStage2] == 0.5 && a.v[kStage1] == 0.0, tag + "the wheel on graph 3 moves STAGE 3 only");

        // VC SOURCE: click steps forward, Shift-click back (wraps), right-click opens the whole list and changes nothing.
        const auto s1 = pn.controlCentre (kVc1Src);
        pn.mouseDown (mouseWith (pn, s1, s1, false, left)); pn.mouseUp (mouseWith (pn, s1, s1, false, left));
        const bool fwd = a.v[kVc1Src] == 1.0;
        pn.mouseDown (mouseWith (pn, s1, s1, false, shift)); pn.mouseUp (mouseWith (pn, s1, s1, false, shift));
        pn.mouseDown (mouseWith (pn, s1, s1, false, shift)); pn.mouseUp (mouseWith (pn, s1, s1, false, shift));
        check (fwd && a.v[kVc1Src] == 3.0, tag + "VC 1 SOURCE: click JACK -> INPUT, Shift-click back to JACK, then wraps to SIDECHAIN");
        const int gb = a.begins;
        pn.mouseDown (mouseWith (pn, s1, s1, false, right)); pn.mouseUp (mouseWith (pn, s1, s1, false, right));
        check (a.v[kVc1Src] == 3.0 && a.begins == gb, tag + "right-click on VC 1 SOURCE does not step");
        check (menusShown == 1 && pendingMenu != nullptr, tag + "right-click on VC 1 SOURCE opens the list menu");
        pendingMenu (2);
        check (a.v[kVc1Src] == 1.0 && a.begins == gb + 1 && a.ends == a.begins, tag + "picking INPUT from that menu sets VC 1 SOURCE as one gesture");
        a.v[kVc2Src] = 2.0;
        juce::StringArray items;
        int ticked = -1;
        const auto menu = pn.choiceMenu (kVc2Src);
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        {
            if (it.getItem().isTicked) ticked = it.getItem().itemID;
            items.add (it.getItem().text);
        }
        check (items == juce::StringArray ({ "JACK", "INPUT", "FOLLOW", "SIDECHAIN" }) && ticked == 3, tag + "the list holds every source, the current one (FOLLOW) ticked");
        const int b0 = a.begins, e0 = a.ends;
        pn.applyChoice (kVc2Src, 2);
        check (a.v[kVc2Src] == 1.0 && a.begins == b0 + 1 && a.ends == e0 + 1 && a.lastGesture == kVc2Src, tag + "choosing INPUT sets it as one gesture");
        pn.applyChoice (kVc2Src, 0);
        check (a.v[kVc2Src] == 1.0 && a.begins == b0 + 1, tag + "a dismissed menu changes nothing");

        // A menu still open when the panel goes away (editor closed): no crash, no write.
        const auto s3 = pn.controlCentre (kVc3Src);
        pn.mouseDown (mouseWith (pn, s3, s3, false, right));
        const double vc3 = a.v[kVc3Src];
        const int menusBefore = menusShown;
        panel.reset();
        if (pendingMenu != nullptr) pendingMenu (4);     // the menu answers after the panel has gone
        check (menusBefore == 2 && a.v[kVc3Src] == vc3, tag + "closing the panel with the list open is safe and writes nothing");
    }
}

// Text rules: help text (captions, hints, footnotes) never below 9 pt at 100 % on any page or mode, and hovering it
// shows it in an enlarged tooltip; menus use a plain sans font.
void testTextRules (const juce::File& outDir)
{
    OrigamiProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* e = dynamic_cast<OrigamiEditor*> (ed.get());
    if (e == nullptr) { check (false, "editor"); return; }
    struct Bind : OrigamiPanel::Access
    {
        OrigamiProcessor& q;
        explicit Bind (OrigamiProcessor& x) : q (x) {}
        double get (int i) const override { return q.value (i); }
        void set (int i, double v) override { q.setValue (i, v); }
        float inPeak (int) const override { return 0.0f; }
        float outPeak (int) const override { return 0.0f; }
        float follower() const override { return 0.0f; }
        bool over() const override { return false; }
        int latencySamples() const override { return 0; }
        double stageCurve (int s, double x) const override { double v[kParamCount]; for (int i = 0; i < kParamCount; ++i) v[i] = q.value (i); return OrigamiCore::stageCurve (v, s, x); }
        int presetCount() const override { return (int) factoryPresets().size(); }
        juce::String presetName (int i) const override { return factoryPresets()[(size_t) i].name; }
        juce::String presetBank (int i) const override { return factoryPresets()[(size_t) i].bank; }
        int currentPreset() const override { return q.getCurrentProgram(); }
    } bind (p);
    OrigamiPanel open (bind, OrigamiPanel::Mode::RackOpen), closed (bind, OrigamiPanel::Mode::RackClosed);
    open.setSize (1136, 332);
    closed.setSize (1136, 87);
    auto& plug = e->panel();
    float smallest = 100.0f;
    juce::String where;
    for (auto* panel : { &plug, &open })
        for (auto pg : { Page::Main, Page::Stages, Page::Dynamics, Page::Setup })
        {
            panel->setPage (pg);
            (void) panel->createComponentSnapshot (panel->getLocalBounds(), true, 1.0f);
            if (panel->smallestTextSize() < smallest)
            {
                smallest = panel->smallestTextSize();
                where = juce::String (panel->mode() == OrigamiPanel::Mode::Plugin ? "plugin " : "rack ") + OrigamiPanel::pageName (pg);
            }
            check (panel->labels().size() >= 2, juce::String ("help text recorded on ") + OrigamiPanel::pageName (pg));
        }
    (void) closed.createComponentSnapshot (closed.getLocalBounds(), true, 1.0f);
    smallest = juce::jmin (smallest, closed.smallestTextSize());
    check (smallest >= OrigamiPanel::kMinTextSize, "no help text below 9 pt at 100 % (smallest " + juce::String (smallest, 1) + " on " + where + ")");

    // Hover: help text shows itself (enlarged by the tooltip look); a knob still shows its value.
    plug.setPage (Page::Stages);
    (void) plug.createComponentSnapshot (plug.getLocalBounds(), true, 1.0f);
    bool caption = false, footnote = false;
    for (auto& l : plug.labels())
    {
        const auto at = plug.toLocal (l.area.getCentre());
        if (l.text.startsWith ("drag to set STAGE 2")) caption = plug.tooltipAt (at) == l.text;
        if (l.text.startsWith ("VC source:")) footnote = plug.tooltipAt (at) == l.text;
    }
    check (caption && footnote, "hovering a graph caption or the VC source footnote shows it");
    check (plug.tooltipAt (plug.controlCentre (kSym1)).startsWith ("SYM 1  "), "hovering a knob still shows its value");
    OrigamiLookAndFeel laf;
    const auto mf = laf.getPopupMenuFont();
    check (mf.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() && mf.getHeight() >= 14.0f, "menus use the plain sans font at >= 14 px");
    check (OrigamiLookAndFeel::kTooltipTextHeight >= 2.0f * OrigamiPanel::kMinTextSize, "tooltips show the hovered text at least twice the smallest panel size");
    check (&plug.getLookAndFeel() == &e->getLookAndFeel() && dynamic_cast<OrigamiLookAndFeel*> (&e->getLookAndFeel()) != nullptr, "the plugin window uses ORIGAMI's look (menus, tooltips)");
    if (outDir != juce::File())
    {
        p.setValue (kWave, 0.5); p.setValue (kStage1, 0.3); p.setValue (kStage2, -0.2); p.setValue (kStage3, 0.15); p.setValue (kSym2, 0.35); p.setValue (kVc2Src, 2.0);
        for (auto pg : { Page::Main, Page::Stages, Page::Dynamics, Page::Setup })
        {
            plug.setPage (pg);
            snapshot (plug, outDir.getChildFile (juce::String ("v2_plugin_") + juce::String (OrigamiPanel::pageName (pg)).toLowerCase() + ".png"));
            open.setPage (pg);
            snapshot (open, outDir.getChildFile (juce::String ("v2_rack_") + juce::String (OrigamiPanel::pageName (pg)).toLowerCase() + ".png"));
        }
        snapshot (closed, outDir.getChildFile ("v2_rack_closed.png"));
    }
}

// The preset test signal, 48 kHz stereo, 3.5 s: a log sine sweep 30 Hz -> 16 kHz at -6 dBFS (R at 0.8 of L),
// then four drum-like hits (a pitch-dropping body plus a noise snap) peaking at -3 dBFS.
juce::AudioBuffer<float> presetTestSignal()
{
    const double fs = 48000.0;
    const int nSweep = 96000, nHit = 18000, n = nSweep + 4 * nHit;
    juce::AudioBuffer<float> x (2, n);
    const double f0 = 30.0, f1 = 16000.0, T = nSweep / fs, k = std::log (f1 / f0);
    for (int i = 0; i < nSweep; ++i)
    {
        const double t = i / fs;
        const double ph = 2.0 * juce::MathConstants<double>::pi * f0 * T / k * (std::exp (t / T * k) - 1.0);
        const double fade = std::min (1.0, std::min (i, nSweep - 1 - i) / 480.0);
        const double v = 0.5 * fade * std::sin (ph);
        x.setSample (0, i, (float) v);
        x.setSample (1, i, (float) (0.8 * v));
    }
    juce::Random r (2026);
    for (int h = 0; h < 4; ++h)
    {
        double ph = 0.0;
        for (int i = 0; i < nHit; ++i)
        {
            const double t = i / fs;
            ph += 2.0 * juce::MathConstants<double>::pi * (45.0 + 105.0 * std::exp (-t / 0.03)) / fs;
            const double body = 0.5 * std::exp (-t / 0.12) * std::sin (ph);
            const double snap = 0.2 * std::exp (-t / 0.02) * (r.nextDouble() * 2.0 - 1.0);
            const double v = juce::jlimit (-0.7079, 0.7079, body + snap);
            x.setSample (0, nSweep + h * nHit + i, (float) v);
            x.setSample (1, nSweep + h * nHit + i, (float) v);
        }
    }
    return x;
}

// Bank signals at their sources' levels, 48 kHz stereo, peaking at -1 dBFS.
// RONIN: a bright saw bass line (55-220 Hz), 2 s. SHOGUN: a drum loop (kick, snare, hats), 2 s.
juce::AudioBuffer<float> bankTestSignal (const juce::String& bank)
{
    const double fs = 48000.0;
    const int n = 96000;
    juce::AudioBuffer<float> x (2, n);
    x.clear();
    if (bank == "RONIN")
    {
        const double notes[8] { 55.0, 110.0, 82.41, 55.0, 130.81, 110.0, 73.42, 220.0 };
        double ph = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const int step = i / 12000, pos = i % 12000;
            ph += notes[step % 8] / fs;
            ph -= std::floor (ph);
            const double env = std::min (1.0, pos / 48.0) * std::exp (-pos / 9000.0);
            const double v = 0.891 * env * (2.0 * ph - 1.0);
            x.setSample (0, i, (float) v);
            x.setSample (1, i, (float) v);
        }
    }
    else if (bank == "SHOGUN")
    {
        juce::Random r (99);
        for (int i = 0; i < n; ++i)
        {
            const int pos = i % 12000, beat = (i / 12000) % 4;
            const double t = pos / fs;
            double v = 0.0;
            if (beat == 0 || beat == 2) v += 0.8 * std::exp (-t / 0.15) * std::sin (2.0 * juce::MathConstants<double>::pi * (50.0 * t + 1.8 * (1.0 - std::exp (-t / 0.02))));
            if (beat == 1 || beat == 3) v += std::exp (-t / 0.08) * (0.35 * std::sin (2.0 * juce::MathConstants<double>::pi * 190.0 * t) + 0.45 * (r.nextDouble() * 2.0 - 1.0));
            const int hp = i % 6000;
            v += 0.25 * std::exp (-hp / fs / 0.015) * (r.nextDouble() * 2.0 - 1.0);
            v = juce::jlimit (-0.891, 0.891, v);
            x.setSample (0, i, (float) v);
            x.setSample (1, i, (float) v);
        }
    }
    else
        return {};
    return x;
}

struct SignalResult { bool finite = true; double peakDb = -240.0, rmsDb = -240.0; };

SignalResult runSignal (OrigamiProcessor& p, const juce::AudioBuffer<float>& signal)
{
    SignalResult res;
    p.prepareToPlay (48000.0, 512);
    double peak = 0.0, sum = 0.0;
    juce::AudioBuffer<float> blk (2, 512);
    for (int start = 0; start < signal.getNumSamples(); start += 512)
    {
        const int len = std::min (512, signal.getNumSamples() - start);
        blk.clear();
        for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, signal, c, start, len);
        run (p, blk);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < len; ++i)
            {
                const double v = (double) blk.getSample (c, i);
                res.finite = res.finite && std::isfinite (v);
                peak = std::max (peak, std::abs (v));
                sum += v * v;
            }
    }
    const double rms = std::sqrt (sum / (2.0 * signal.getNumSamples()));
    res.peakDb = 20.0 * std::log10 (std::max (peak, 1e-12));
    res.rmsDb = 20.0 * std::log10 (std::max (rms, 1e-12));
    return res;
}

bool paramsMatch (OrigamiProcessor& p, const double (&v)[kParamCount], bool exact, juce::String& why)
{
    for (int i = 0; i < kParamCount; ++i)
    {
        if (i == kBypass) continue;
        const auto& info = paramInfo (i);
        const double tol = exact ? 0.0 : 1e-5 * (info.max - info.min);
        if (std::abs (p.value (i) - v[i]) > tol)
        {
            why = juce::String (info.id) + " " + juce::String (p.value (i), 7) + " vs " + juce::String (v[i], 7);
            return false;
        }
    }
    return true;
}

void testFactoryPresets()
{
    const auto& bank = factoryPresets();
    const int count = (int) bank.size();
    check (count >= 16 && count <= 40, "factory bank has 16..40 presets, got " + juce::String (count));
    {
        std::map<juce::String, int> perBank;
        juce::StringArray order;
        for (const auto& pr : bank) { ++perBank[pr.bank]; order.addIfNotAlreadyThere (pr.bank); }
        bool sizes = perBank["INIT"] == 1;
        for (auto b : { "RONIN", "SHOGUN", "BUSHIDO", "GENERIC" }) sizes = sizes && perBank[b] >= 6 && perBank[b] <= 9;
        check (order == juce::StringArray ({ "INIT", "RONIN", "SHOGUN", "BUSHIDO", "GENERIC" }) && sizes,
               "banks in order INIT, RONIN, SHOGUN, BUSHIDO, GENERIC with 6..9 presets each (" + order.joinIntoString (", ") + ")");
        check (count > 1 && bank[1].displayName() == bank[1].bank + ": " + bank[1].name && bank[0].displayName() == "INIT",
               "program names are BANK: Name (INIT alone)");
    }
    check (count > 0 && bank[0].name == "INIT", "the first factory preset is INIT");
    bool initIsDefault = count > 0;
    for (int i = 0; i < kParamCount && count > 0; ++i)
        initIsDefault = initIsDefault && bank[0].values[i] == paramInfo (i).def;
    check (initIsDefault, "INIT is exactly the parameter defaults");

    // The embedded XML: every PRESET parses, holds every param id once, and no name carries a digit.
    juce::StringArray errors;
    auto xml = juce::parseXML (factoryBankXmlText());
    check (xml != nullptr && (int) parseFactoryBank (*xml, &errors).size() == count && errors.isEmpty(),
           "the embedded bank parses with no skipped presets " + errors.joinIntoString ("; "));
    std::set<juce::String> names;
    if (xml != nullptr)
        for (auto* e : xml->getChildWithTagNameIterator ("PRESET"))
        {
            const auto name = e->getStringAttribute ("name");
            names.insert (name);
            const auto* st = e->getChildByName (kStateUnit);
            int seen[kParamCount] {};
            bool known = st != nullptr && st->getIntAttribute ("format") == kStateFormat;
            if (st != nullptr)
                for (auto* q : st->getChildWithTagNameIterator ("PARAM"))
                {
                    const int idx = paramIndex (q->getStringAttribute ("id").toStdString());
                    if (idx < 0) known = false; else ++seen[idx];
                }
            bool complete = known;
            for (int i = 0; i < kParamCount; ++i) complete = complete && seen[i] == 1;
            check (complete, "preset " + name + ": a complete format 1 state, every param id once");
            check (! name.containsAnyOf ("0123456789") && e->getStringAttribute ("bank").isNotEmpty(),
                   "preset " + name + ": descriptive name (no digits) and a bank");
        }
    check ((int) names.size() == count, "preset names are unique");

    // The host program list.
    {
        OrigamiProcessor p;
        check (p.getNumPrograms() == count && p.getProgramName (0) == "INIT" && p.getCurrentProgram() == 0,
               "the host sees the factory bank as programs, INIT current");
        check (p.getProgramName (count) == juce::String() && p.getProgramName (-1) == juce::String(), "no program names outside the bank");
    }

    const auto signal = presetTestSignal();
    for (int k = 0; k < count; ++k)
    {
        const auto& pr = bank[(size_t) k];
        const juce::String tag = "preset " + juce::String (k) + " \"" + pr.name + "\": ";
        bool inRange = true;
        for (int i = 0; i < kParamCount; ++i) inRange = inRange && clampParam (i, pr.values[i]) == pr.values[i];
        check (inRange && pr.values[kLevelComp] == 1.0 && pr.values[kBypass] == 0.0, tag + "values in range, LEVEL COMP ON, not bypassed");

        // The state XML of the preset values round-trips byte- and value-identical.
        {
            auto a = stateToXml (pr.values);
            double back[kParamCount];
            const bool ok = stateFromXml (*a, back).ok;
            bool same = ok;
            for (int i = 0; i < kParamCount; ++i) same = same && back[i] == pr.values[i];
            check (same && stateToXml (back)->toString() == a->toString(), tag + "state XML round trip is byte- and value-identical");
        }

        // Choose it as a program, save, load into a fresh instance, save again.
        OrigamiProcessor p;
        p.setCurrentProgram (k);
        juce::String why;
        check (p.getCurrentProgram() == k && paramsMatch (p, pr.values, false, why), tag + "loads as a program " + why);
        juce::MemoryBlock a, b;
        p.getStateInformation (a);
        OrigamiProcessor q;
        q.setStateInformation (a.getData(), (int) a.getSize());
        q.getStateInformation (b);
        double pv[kParamCount];
        for (int i = 0; i < kParamCount; ++i) pv[i] = p.value (i);
        check (a == b && q.getCurrentProgram() == k && paramsMatch (q, pv, true, why), tag + "plugin state round trip is byte- and param-identical " + why);

        // The test signal: finite, not silent, peaks below 0 dBFS (LEVEL COMP on, as shipped). RONIN and SHOGUN
        // presets also run their bank's signal at -1 dBFS.
        const auto res = runSignal (p, signal);
        std::printf ("  preset %-8s %-22s peak %6.2f dBFS  rms %6.2f dBFS", pr.bank.toRawUTF8(), pr.name.toRawUTF8(), res.peakDb, res.rmsDb);
        check (res.finite && res.rmsDb > -40.0 && res.peakDb < 0.0, tag + "test signal: finite, not silent, peak below 0 dBFS (peak "
               + juce::String (res.peakDb, 2) + " dBFS, rms " + juce::String (res.rmsDb, 2) + " dBFS)");
        const auto own = bankTestSignal (pr.bank);
        if (own.getNumSamples() > 0)
        {
            const auto rb = runSignal (p, own);
            std::printf ("  | %s signal peak %6.2f rms %6.2f", pr.bank.toRawUTF8(), rb.peakDb, rb.rmsDb);
            check (rb.finite && rb.rmsDb > -40.0 && rb.peakDb < 0.0, tag + pr.bank + " signal at -1 dBFS: finite, not silent, peak below 0 dBFS (peak "
                   + juce::String (rb.peakDb, 2) + " dBFS)");
        }
        std::printf ("\n");
    }
}

}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File();
    if (outDir != juce::File()) outDir.createDirectory();
    testStandaloneNormals();
    testSidechain();
    testState();
    testFactoryPresets();
    testEditor (outDir);
    testUiLogic();
    testStagesAndLists();
    testTextRules (outDir);
    std::printf ("%d checks, %d failed\n%s\n", checks, failures, failures == 0 ? "ORIGAMI PLUGIN TESTS PASS" : "ORIGAMI PLUGIN TESTS FAIL");
    return failures == 0 ? 0 : 1;
}
