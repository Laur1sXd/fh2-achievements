// The in-game Achievements page (platform API 8): what the pause menu's ACHIEVEMENTS tile opens,
// and MODS > Installed mods > Achievements. A summary, then each group (the game, Storm Island,
// the Porsche Expansion) under a heading: the unlocked ones first, then the locked ones closest
// to unlocking, then the ones that need online play, then the ones the mod can't track yet.
#include "ach.hpp"
#include "fh2mod.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ach {

namespace {

// 1234567 -> "1,234,567"; from 100,000 up, shorter: "812k", "1.41M"
std::string number(double v) {
    char b[48];
    if (v >= 1e6) {
        std::snprintf(b, sizeof b, "%.2f", v / 1e6);
        std::string s = b;
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
        return s + "M";
    }
    if (v >= 1e5) {
        std::snprintf(b, sizeof b, "%.0fk", std::floor(v / 1e3));
        return b;
    }
    std::snprintf(b, sizeof b, "%.0f", std::floor(v));
    std::string s = b, o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i && (s.size() - i) % 3 == 0) o += ',';
        o += s[i];
    }
    return o;
}

std::string gamerscore(int g) { return std::to_string(g) + "G"; }

const char* const kGroupNames[] = {"FORZA HORIZON 2", "STORM ISLAND", "PORSCHE EXPANSION"};
const char* const kGroupLong[]  = {"Forza Horizon 2", "Storm Island", "the Porsche Expansion"};

enum Order { kUnlocked, kLocked, kOnline, kPending };

struct Entry {
    const Def* d;
    Order order;
    Progress p;
    bool measured;   // has progress to show
};

}  // namespace

std::vector<PageRow> page_rows(const Facts* f, const std::map<std::string, std::string>& unlocked) {
    std::vector<PageRow> rows;
    int got = 0, all = 0, gs = 0, gsAll = 0;
    for (const Def& d : defs()) {
        ++all;
        gsAll += d.gamerscore;
        if (unlocked.count(d.id)) {
            ++got;
            gs += d.gamerscore;
        }
    }
    PageRow sum;
    sum.label       = "ALL ACHIEVEMENTS";
    sum.value       = std::to_string(got) + " / " + std::to_string(all);
    sum.style       = FH2_ROW_HEADING;
    sum.description = std::to_string(got) + " of " + std::to_string(all) + " unlocked, " + gamerscore(gs) + " of " + gamerscore(gsAll) +
                      ". They unlock as you play, with the game's own pop-up. What your save already had counted when the mod "
                      "was installed; the few that need a particular car or place count from then on, and say so.";
    sum.footer      = f ? "Unlocked ones show their gamerscore. Locked ones are grey, with your progress."
                        : "Your progress shows once the game has saved (it does every minute or so).";
    rows.push_back(sum);

    for (int g = 0; g < 3; ++g) {
        std::vector<Entry> es;
        int gGot = 0, gAll = 0, gGs = 0, gGsAll = 0;
        for (const Def& d : defs()) {
            if (static_cast<int>(d.group) != g) continue;
            Entry e{&d, kLocked, {}, false};
            if (d.eval && f) {
                e.p        = d.eval(*f);
                e.measured = d.unit != nullptr;
            }
            if (unlocked.count(d.id)) e.order = kUnlocked;
            else if (d.track == Track::Online) e.order = kOnline;
            else if (d.track == Track::Pending) e.order = kPending;
            ++gAll;
            gGsAll += d.gamerscore;
            if (e.order == kUnlocked) {
                ++gGot;
                gGs += d.gamerscore;
            }
            es.push_back(e);
        }
        // locked ones closest to unlocking first; otherwise the game's own order
        std::stable_sort(es.begin(), es.end(), [](const Entry& a, const Entry& b) {
            if (a.order != b.order) return a.order < b.order;
            if (a.order != kLocked) return false;
            const double fa = a.measured && a.p.need > 0 ? a.p.have / a.p.need : -1;
            const double fb = b.measured && b.p.need > 0 ? b.p.have / b.p.need : -1;
            return fa > fb;
        });

        PageRow head;
        head.label       = kGroupNames[g];
        head.value       = std::to_string(gGot) + " / " + std::to_string(gAll);
        head.style       = FH2_ROW_HEADING;
        head.description = std::string(g == 0 ? "The game's own achievements: " : std::string("The achievements for ") + kGroupLong[g] + ": ") +
                           std::to_string(gGot) + " of " + std::to_string(gAll) + " unlocked, " + gamerscore(gGs) + " of " +
                           gamerscore(gGsAll) + ".";
        rows.push_back(head);

        for (const Entry& e : es) {
            const Def& d = *e.d;
            PageRow r;
            r.key         = d.id;
            r.label       = d.name;
            r.description = d.desc;
            const std::string progress = e.measured ? number(e.p.have) + " / " + number(e.p.need) : "";
            const std::string has_of   = e.measured ? number(e.p.have) + " of " + number(e.p.need) + " " + d.unit : "";
            switch (e.order) {
                case kUnlocked: {
                    r.value               = gamerscore(d.gamerscore);
                    const std::string& at = unlocked.at(d.id);
                    r.footer              = gamerscore(d.gamerscore) + " - unlocked" + (at.size() >= 10 ? " " + at.substr(0, 10) : "");
                    break;
                }
                case kLocked:
                    r.style  = FH2_ROW_DIM;
                    r.value  = e.measured ? progress : "Locked";
                    r.footer = gamerscore(d.gamerscore) + " - " + (e.measured ? has_of : "locked");
                    if (d.track == Track::Events) r.description += " Counted from when the mod was installed.";
                    break;
                case kOnline:
                    r.style  = FH2_ROW_DIM;
                    r.value  = "Online only";
                    r.description += " It needs online play, which the game doesn't have on PC, so it stays locked for now.";
                    r.footer = gamerscore(d.gamerscore) + " - " + (e.measured ? has_of + " so far" : "locked");
                    break;
                case kPending:
                    r.style  = FH2_ROW_DIM;
                    r.value  = "Not tracked";
                    r.description += " The mod can't see this happen in the game yet, so it stays locked for now.";
                    r.footer = gamerscore(d.gamerscore) + " - not tracked yet";
                    break;
            }
            rows.push_back(r);
        }
    }
    return rows;
}

}  // namespace ach
