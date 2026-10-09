// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// The panel's interaction rules as plain functions (no JUCE, no GUI), so the tests can check them directly:
//   - knob and graph drags: 200 px of travel (at 100 % UI scale) covers the whole range; Shift is 5x finer,
//   - list controls: a click steps forward, Shift-click steps back (both wrap), a right-click opens the whole list
//     as a menu with the current choice ticked (menu item id = choice index + 1, 0 = dismissed),
//   - the STAGES page edits the same STAGE 1-3 parameters as MAIN (one parameter, two places on the panel).

#include "origami/dsp/OrigamiParams.h"

#include <string>
#include <vector>

namespace origami::ui {

inline constexpr double kDragPixels = 200.0;   // full range, at UI scale 1
inline constexpr double kFineFactor = 5.0;     // Shift

// New normalised value after a vertical drag of deltaUp pixels (positive = up) from startNormal.
inline double dragToNormal (double startNormal, double deltaUp, double uiScale, bool fine) noexcept
{
    const double px = kDragPixels * (uiScale > 0.01 ? uiScale : 0.01) * (fine ? kFineFactor : 1.0);
    const double n = startNormal + deltaUp / px;
    return n < 0.0 ? 0.0 : (n > 1.0 ? 1.0 : n);
}

// The next choice of a list control: forward on a click, back on Shift-click; wraps both ways.
inline int stepChoice (int current, int count, bool backwards) noexcept
{
    if (count <= 0) return 0;
    current = current < 0 ? 0 : (current >= count ? count - 1 : current);
    return backwards ? (current + count - 1) % count : (current + 1) % count;
}

struct ChoiceList
{
    std::vector<std::string> names;   // in parameter order
    int current = 0;                  // the ticked item
};

// The full list a right-click shows for a choice parameter (empty for a continuous one).
inline ChoiceList choiceList (int param, double value)
{
    ChoiceList l;
    const auto& i = paramInfo (param);
    if (i.choices <= 0 || i.choiceNames == nullptr) return l;
    for (int k = 0; k < i.choices; ++k) l.names.emplace_back (i.choiceNames[k]);
    const long v = std::lround (clampParam (param, value));
    l.current = (int) v;
    return l;
}

// The choice a menu result selects, or -1 when the menu was dismissed or the id is out of range.
inline int choiceFromMenuResult (int param, int menuId) noexcept
{
    const int n = paramInfo (param).choices;
    return menuId >= 1 && menuId <= n ? menuId - 1 : -1;
}

// The STAGE n amount the STAGES page edits (its knob and its graph): the MAIN page's parameter, not a copy.
inline constexpr int stageAmountParam (int stage) noexcept { return kStage1 + stage; }

} // namespace origami::ui
