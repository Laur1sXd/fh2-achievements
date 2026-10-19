// The 88 achievements and how each is worked out. See ach.hpp for where the facts come from.
//
// Numbers checked against the game's data (24 Nov 2015 build):
//  - Wristbands follow total XP (Main/XP), from media\GameTunableSettings\PlayerXPLevels.xml:
//    Yellow 14 levels x 15,000, then 15 levels x 20,000 each for Green, Blue, Pink, Orange,
//    Purple, Red, Turquoise, Bronze, Silver and Gold. So Green starts at 210,000, Pink at
//    810,000, Purple at 1,410,000 and Gold at 2,910,000. (The game's WristbandGained event fired
//    exactly when a test save crossed 510,000, Blue.)
//  - 168 mainland championships and 30 on Storm Island (Campaigns.CareerLocationId 0 and 1); six
//    of the island ones are Gauntlets (race mode IronMan).
//  - 175 mainland reward boards and 25 Storm Island bonus boards, 10 mainland barn finds and one
//    on Storm Island (pdlc_barn_find_01), all listed in SaveState whether found or not.
//  - Bucket lists 1-3 are the mainland's (45 challenges), 4-5 Storm Island's (8), 6 the Porsche
//    one (10).
//  - Showcase "Train in Vain" is event 92; the Horizon Finale is 201, Storm Island's 937.
//  - Car ids: Fiesta '81 2158, Galant VR-4 1381, Metro 6R4 2147, Ranger T6 2145, #7 SST 2143;
//    Porsche 911 Turbo 3.3 265, 914/6 266, 944 Turbo 268, 959 269, Carrera GT 292, 911 Turbo S
//    2289, 918 Spyder 2290, Cayman GTS 2293, Macan Turbo 2295, 911 GT3 RS 4.0 2297.
//  - Skills: "Clean Racing" is the FastAndClean skill (RaceFeats strings IDS_FastAndClean1-4).
#include "ach.hpp"

#include <algorithm>

namespace ach {

double Facts::stat(const std::string& name) const {
    auto l = live.stats.find(name);
    if (l != live.stats.end()) return l->second;
    auto s = save.stats.find(name);
    return s == save.stats.end() ? 0 : s->second;
}

std::string Facts::prop(const std::string& obj, const std::string& name) const {
    auto o = save.objects.find(obj);
    if (o == save.objects.end()) return {};
    auto p = o->second.find(name);
    return p == o->second.end() ? std::string{} : p->second;
}

namespace {

Progress P(double have, double need) { return {std::min(have, need), need}; }
Progress Flag(bool b) { return {b ? 1.0 : 0.0, 1}; }

constexpr double kGreenXP = 210000, kPinkXP = 810000, kPurpleXP = 1410000, kGoldXP = 2910000;
constexpr int kTrainInVain = 92, kHorizonFinale = 201, kIslandFinale = 937;
constexpr int kMainlandRoads = 315;

bool won_event(const Facts& f, int event) {
    auto it = f.save.eventPlace.find(event);
    return (it != f.save.eventPlace.end() && it->second == 1) || f.ctr.get("won." + std::to_string(event)) > 0;
}

// Reward boards: SaveState's DiscountBoardSaveState objects, plus any smashed this session.
struct Boards {
    int have = 0, total = 0;
};
Boards boards(const Facts& f, bool island) {
    Boards b;
    for (const auto& [id, type] : f.save.objectType) {
        if (type != "DiscountBoardSaveState") continue;
        if ((id.rfind("island_", 0) == 0) != island) continue;
        ++b.total;
        if (f.prop(id, "State") == "Collected" || f.live.boardsSmashed.count(id)) ++b.have;
    }
    return b;
}

int barns_restored(const Facts& f) {
    int n = 0;
    for (int i = 1; i <= 10; ++i) {
        char id[32];
        std::snprintf(id, sizeof id, "barn_find_%02d", i);
        if (f.prop(id, "State") == "Restored") ++n;
    }
    return n;
}

// Bucket list challenges done, solo or co-op, in the given lists.
struct Bucket {
    int solo = 0, coop = 0, total = 0;
};
Bucket bucket(const Facts& f, std::initializer_list<int> lists) {
    Bucket b;
    for (const auto& [ch, list] : f.gd.bucketList) {
        if (std::find(lists.begin(), lists.end(), list) == lists.end()) continue;
        ++b.total;
        auto it = f.save.bucket.find(ch);
        if (it == f.save.bucket.end()) continue;
        b.solo += it->second.first;
        b.coop += it->second.second;
    }
    return b;
}

int car_types_won(const Facts& f) {
    std::set<int> types;
    for (const auto& [event, bucket] : f.gd.champEventBucket)
        if (won_event(f, event)) types.insert(bucket);
    return static_cast<int>(types.size());
}

Progress wristband(const Facts& f, double xp) { return P(f.stat("Main/XP"), xp); }

// ---- the rules, one per achievement -------------------------------------------------------

#define RULE(name) Progress name(const Facts& f)

RULE(r_festival) { return Flag(f.flag("iestate", "IntroducedFestival")); }
RULE(r_sisteron) { return Flag(f.flag("iestate", "IntroducedSisteron")); }
RULE(r_nice) { return Flag(f.flag("iestate", "IntroducedNice")); }
RULE(r_saintmartin) { return Flag(f.flag("iestate", "IntroducedSaintMartin")); }
RULE(r_castelletto) { return Flag(f.flag("iestate", "IntroducedCastelletto")); }
RULE(r_montellino) { return Flag(f.flag("iestate", "IntroducedMontellino")); }
RULE(r_sangiovanni) { return Flag(f.flag("iestate", "IntroducedSanGimignano")); }   // San Giovanni's working name
RULE(r_wanderlust) { return P(f.stat("CompareStats/RoadTripEventsCompleted"), 1); }
RULE(r_yellow) { return Flag(f.flag("iestate", "IntroducedFestival") || f.stat("Main/XP") > 0); }
RULE(r_green) { return wristband(f, kGreenXP); }
RULE(r_pink) { return wristband(f, kPinkXP); }
RULE(r_purple) { return wristband(f, kPurpleXP); }
RULE(r_gold) { return wristband(f, kGoldXP); }
RULE(r_first_race) { return P(f.stat("CompareStats/CarChampionshipEventsWon"), 1); }
RULE(r_number_one) {
    return P(std::max<double>(f.save.champsWon.size(), f.stat("CompareStats/CarChampionshipsWon")), 1);
}
RULE(r_champs25) { return P(f.save.champsCompleted.size(), 25); }
RULE(r_champs100) { return P(f.save.champsCompleted.size(), 100); }
RULE(r_champs_all) { return P(f.save.champsCompleted.size(), f.gd.mainlandCampaigns ? f.gd.mainlandCampaigns : 168); }
RULE(r_champion) { return Flag(f.flag("CareerState", "WonFirstFinal") || won_event(f, kHorizonFinale)); }
RULE(r_new_car) {
    // Bought a car since the mod was installed, or the garage holds more than the starting car
    // and the restored barn finds (every other way into the garage is a purchase).
    const int restored = barns_restored(f) + (f.prop("pdlc_barn_find_01", "State") == "Restored");
    return Flag(f.ctr.get("bought") > 0 || static_cast<int>(f.save.garage.size()) > restored + 1);
}
RULE(r_cars50) { return P(std::max<double>(f.save.garage.size(), f.stat("CompareStats/CarsInGarage")), 50); }
RULE(r_cars175) { return P(std::max<double>(f.save.garage.size(), f.stat("CompareStats/CarsInGarage")), 175); }
RULE(r_skill5) { return P(f.stat("Main/NumSkillTokens"), 5); }
RULE(r_skill65) { return P(f.stat("Main/NumSkillTokens"), 65); }
RULE(r_clean100) { return P(f.ctr.get("skill.FastAndClean"), 100); }
RULE(r_boards25) { return P(boards(f, false).have, 25); }
RULE(r_boards_all) {
    auto b = boards(f, false);
    return P(b.have, b.total ? b.total : 175);
}
RULE(r_roads) { return P(f.stat("CompareStats/RoadsDiscovered"), kMainlandRoads); }
RULE(r_barns10) { return P(barns_restored(f), 10); }
RULE(r_photos) {
    return P(std::max<double>(f.save.photos, f.stat("CompareStats/PhotoChallengesCompleted")), 100);
}
RULE(r_bucket1) { return P(bucket(f, {1, 2, 3}).solo, 1); }
RULE(r_bucket15) { return P(bucket(f, {1, 2, 3}).solo, 15); }
RULE(r_bucket_all) {
    auto b = bucket(f, {1, 2, 3});
    return P(b.solo, b.total ? b.total : 45);
}
RULE(r_train) { return Flag(won_event(f, kTrainInVain)); }
RULE(r_car_types) {
    return P(car_types_won(f), f.gd.mainlandBuckets.empty() ? 28 : f.gd.mainlandBuckets.size());
}
// Online play: still counted from the profile's stats, in case online play comes back.
RULE(r_rivals_credits) { return P(f.stat("CompareStats/CreditsEarnedFromRivals"), 1000000); }
RULE(r_mp20) { return P(f.stat("MiscStats/OnlineEventsFinished"), 20); }
RULE(r_mp200) { return P(f.stat("MiscStats/OnlineEventsFinished"), 200); }
RULE(r_team50) { return P(f.stat("MiscStats/OnlineTeamRacesWon"), 50); }
RULE(r_online_trip1) { return P(f.stat("CompareStats/OnlineRoadTripDestsCompleted"), 1); }
RULE(r_online_trip25) { return P(f.stat("CompareStats/OnlineRoadTripDestsCompleted"), 25); }
RULE(r_coop25) { return P(bucket(f, {1, 2, 3, 4, 5}).coop, 25); }
RULE(r_pg100) {
    return P(f.stat("MiscStats/OnlineInfectedGamesEntered") + f.stat("MiscStats/OnlineKingGamesEntered"), 100);
}

// Storm Island
RULE(r_island_arrive) { return Flag(f.prop("islandiestate", "IEState") == "Complete" || f.flag("islandiestate", "HasAwardedInitialCar")); }
RULE(r_island_won1) { return P(f.save.islandChampsWon.size(), 1); }
RULE(r_island_won5) { return P(f.save.islandChampsWon.size(), 5); }
RULE(r_island_all) { return P(f.save.islandChampsCompleted.size(), f.gd.islandCampaigns ? f.gd.islandCampaigns : 30); }
RULE(r_island_champion) { return Flag(f.flag("CareerState", "WonFirstIslandFinal") || won_event(f, kIslandFinale)); }
RULE(r_gauntlet1) {
    int n = 0;
    for (int c : f.gd.gauntletCampaigns) n += f.save.islandChampsCompleted.count(c);
    return P(n, 1);
}
RULE(r_gauntlet_all) {
    int n = 0;
    for (int c : f.gd.gauntletCampaigns) n += f.save.islandChampsWon.count(c);
    return P(n, f.gd.gauntletCampaigns.empty() ? 6 : f.gd.gauntletCampaigns.size());
}
RULE(r_island_boards) {
    auto b = boards(f, true);
    return P(b.have, b.total ? b.total : 25);
}
RULE(r_island_bucket1) { return P(bucket(f, {4, 5}).solo, 1); }
RULE(r_island_bucket_all) {
    auto b = bucket(f, {4, 5});
    return P(b.solo, b.total ? b.total : 8);
}
RULE(r_island_coop_all) {
    auto b = bucket(f, {4, 5});
    return P(b.coop, b.total ? b.total : 8);
}
RULE(r_island_barn) { return Flag(f.prop("pdlc_barn_find_01", "State") == "Restored"); }
RULE(r_island_chain) { return P(f.ctr.get("chain.island.best"), 100000); }
RULE(r_fiesta) { return P(f.ctr.get("win.car.2158"), 1); }
RULE(r_galant) { return P(f.ctr.get("win.car.1381"), 1); }
RULE(r_metro) { return P(f.ctr.get("win.car.2147"), 1); }
RULE(r_ranger) { return P(f.ctr.get("win.car.2145"), 1); }
RULE(r_sst) { return P(f.ctr.get("win.car.2143"), 1); }

// Porsche
RULE(r_porsche_owned) {
    for (int car : f.save.garage)
        if (f.gd.porsches.count(car)) return Flag(true);
    return Flag(false);
}
RULE(r_porsche_bucket5) { return P(bucket(f, {6}).solo, 5); }
RULE(r_porsche_bucket_all) {
    auto b = bucket(f, {6});
    return P(b.solo, b.total ? b.total : 10);
}
RULE(r_porsche40) { return P(f.ctr.get("done.porsche"), 40); }
RULE(r_914) { return P(f.ctr.get("done.car.266"), 4); }
RULE(r_macan) { return P(f.ctr.get("done.car.2295"), 4); }
RULE(r_carrera) { return P(f.ctr.get("done.car.292"), 4); }
RULE(r_918) { return P(f.ctr.get("done.car.2290"), 4); }
RULE(r_959) { return P(f.ctr.get("done.car.269"), 4); }
RULE(r_same_porsche) {
    int64_t best = 0;
    for (int car : f.gd.porsches) best = std::max(best, f.ctr.get("done.car." + std::to_string(car)));
    return P(best, 10);
}
RULE(r_cayman_drifts) { return P(f.ctr.get("skill.Drift.2293"), 20); }
RULE(r_turbo_s_near_miss) { return P(f.ctr.get("skill.NearMiss.2289"), 30); }
RULE(r_944_clean) { return P(f.ctr.get("skill.FastAndClean.268"), 20); }

#undef RULE

using G = Group;
using T = Track;

const std::vector<Def> kDefs = {
    // ---- Forza Horizon 2: 50, 1000G ----
    {"welcome_festival", "Welcome to Horizon Europe", "Arrive at the Horizon Festival.", 10, G::Base, T::Save, nullptr, r_festival},
    {"welcome_sisteron", "Welcome to Sisteron", "Arrive at Sisteron for the first time.", 25, G::Base, T::Save, nullptr, r_sisteron},
    {"welcome_nice", "Welcome to Nice", "Arrive at Nice for the first time.", 25, G::Base, T::Save, nullptr, r_nice},
    {"welcome_saintmartin", "Welcome to Saint-Martin", "Arrive at Saint-Martin for the first time.", 25, G::Base, T::Save, nullptr, r_saintmartin},
    {"welcome_castelletto", "Welcome to Castelletto", "Arrive at Castelletto for the first time.", 25, G::Base, T::Save, nullptr, r_castelletto},
    {"welcome_montellino", "Welcome to Montellino", "Arrive at Montellino for the first time.", 25, G::Base, T::Save, nullptr, r_montellino},
    {"welcome_sangiovanni", "Welcome to San Giovanni", "Arrive at San Giovanni for the first time.", 25, G::Base, T::Save, nullptr, r_sangiovanni},
    {"horizon_leader", "Horizon Leader", "Choose where the Road Trip goes for the first time.", 20, G::Base, T::Pending, nullptr, nullptr},
    {"wanderlust", "Wanderlust", "Complete a Road Trip.", 10, G::Base, T::Save, "road trips", r_wanderlust},
    {"wristband_yellow", "Yellow Wristband", "Obtain the Yellow Wristband.", 10, G::Base, T::Save, nullptr, r_yellow},
    {"wristband_green", "Green Wristband", "Obtain the Green Wristband.", 10, G::Base, T::Save, "XP", r_green},
    {"wristband_pink", "Pink Wristband", "Obtain the Pink Wristband.", 20, G::Base, T::Save, "XP", r_pink},
    {"wristband_purple", "Purple Wristband", "Obtain the Purple Wristband.", 30, G::Base, T::Save, "XP", r_purple},
    {"wristband_gold", "Gold Wristband", "Obtain the Gold Wristband.", 30, G::Base, T::Save, "XP", r_gold},
    {"first_of_many", "First of Many", "Win your first Championship race.", 10, G::Base, T::Save, "races", r_first_race},
    {"number_one", "Number One", "Win a Championship.", 10, G::Base, T::Save, "championships", r_number_one},
    {"horizon_fan", "Horizon Fan", "Complete 25 Championships.", 20, G::Base, T::Save, "championships", r_champs25},
    {"horizon_enthusiast", "Horizon Enthusiast", "Complete 100 Championships.", 25, G::Base, T::Save, "championships", r_champs100},
    {"all_your_race", "All Your Race Are Belong To Us 2.0", "Complete all 168 Championships.", 50, G::Base, T::Save, "championships", r_champs_all},
    {"horizon_champion", "Horizon Champion", "Become the Horizon Champion.", 50, G::Base, T::Save, nullptr, r_champion},
    {"new_car_smell", "That New Car Smell", "Buy your first car.", 10, G::Base, T::Save, nullptr, r_new_car},
    {"car_lover", "Car Lover", "Have 50 cars in your Garage.", 20, G::Base, T::Save, "cars", r_cars50},
    {"worthy_sultan", "Worthy of a Sultan", "Have 175 cars in your Garage.", 40, G::Base, T::Save, "cars", r_cars175},
    {"license_to_skill", "License to Skill", "Earn 5 Skill Points.", 10, G::Base, T::Save, "skill points", r_skill5},
    {"stone_cold_skiller", "Stone Cold Skiller", "Earn 65 Skill Points.", 20, G::Base, T::Save, "skill points", r_skill65},
    {"fast_and_clean", "Fast and Clean", "Earn the Clean Racing skill 100 times.", 10, G::Base, T::Events, "times", r_clean100},
    {"summer_sale", "Summer Sale", "Smash 25 Reward Boards.", 10, G::Base, T::Save, "boards", r_boards25},
    {"smash_happy", "Smash Happy", "Collect all Reward Boards.", 20, G::Base, T::Save, "boards", r_boards_all},
    {"human_gps", "Human GPS", "Drive down every road in the game.", 10, G::Base, T::Save, "roads", r_roads},
    {"storage_hunter", "Storage Hunter", "Restore 10 Barn Finds.", 20, G::Base, T::Save, "barn finds", r_barns10},
    {"photographer", "Photographer", "Photograph 100 different cars for Horizon Promo.", 20, G::Base, T::Save, "cars", r_photos},
    {"one_down", "One Down!", "Complete a Solo Bucket List challenge.", 10, G::Base, T::Save, "challenges", r_bucket1},
    {"kick_the_bucket", "Kick the Bucket", "Scratch 15 items off your Solo Bucket List.", 10, G::Base, T::Save, "challenges", r_bucket15},
    {"rest_in_peace", "Rest in Peace", "Complete every Solo Bucket List challenge.", 20, G::Base, T::Save, "challenges", r_bucket_all},
    {"rain_master", "Rain Master", "Complete your first race in the rain.", 10, G::Base, T::Pending, nullptr, nullptr},
    {"train_in_vain", "The Train's in Vain", "Beat the train in a Horizon Showcase event.", 10, G::Base, T::Save, nullptr, r_train},
    {"multi_disciplined", "Multi-Disciplined", "Win an event in every type of car.", 10, G::Base, T::Save, "car types", r_car_types},
    {"wild_drivatar", "A Wild Drivatar Appears!", "Win 3 Head-to-Head races against Drivatars with a bounty.", 10, G::Base, T::Pending, nullptr, nullptr},
    {"we_meet_again", "We Meet Again!", "Win a Showdown at every Car Meet.", 20, G::Base, T::Pending, nullptr, nullptr},
    {"friends_with_benefits", "Friends with Benefits", "Earn 1,000,000 Cr from Rivals.", 10, G::Base, T::Online, "Cr", r_rivals_credits},
    {"super_meet_boy", "Super Meet Boy", "Get a livery and a tuning setup, buy a car and enter a Showdown at a Car Meet.", 10, G::Base, T::Online, nullptr, nullptr},
    {"bragging_rights", "Bragging Rights", "Beat friends or club members on 20 Speed Cameras.", 10, G::Base, T::Online, nullptr, nullptr},
    {"social_caterpillar", "Social Caterpillar", "Compete in 20 multiplayer events.", 10, G::Base, T::Online, "events", r_mp20},
    {"social_butterfly", "Social Butterfly", "Compete in 200 multiplayer events.", 50, G::Base, T::Online, "events", r_mp200},
    {"team_player", "Team Player", "Win 50 team events.", 30, G::Base, T::Online, "wins", r_team50},
    {"loyal", "Loyal", "Earn 500,000 XP for your club.", 25, G::Base, T::Online, nullptr, nullptr},
    {"on_the_road_again", "On the Road Again", "Complete an Online Road Trip location.", 10, G::Base, T::Online, "locations", r_online_trip1},
    {"well_travelled", "Well Travelled", "Complete 25 Online Road Trip locations.", 30, G::Base, T::Online, "locations", r_online_trip25},
    {"yes_we_can", "Yes We Can", "Complete 25 Bucket List items co-operatively.", 30, G::Base, T::Online, "challenges", r_coop25},
    {"you_are_pg", "You Are Playground Games", "Complete 100 Playground Games events.", 40, G::Base, T::Online, "events", r_pg100},

    // ---- Storm Island: 23, 500G ----
    {"welcome_storm_island", "Welcome to Storm Island", "Arrive on Storm Island.", 20, G::StormIsland, T::Save, nullptr, r_island_arrive},
    {"into_the_jungle", "Into the Jungle", "Win your first Storm Island Championship.", 20, G::StormIsland, T::Save, "championships", r_island_won1},
    {"high_five", "High Five", "Win 5 Storm Island Championships.", 20, G::StormIsland, T::Save, "championships", r_island_won5},
    {"go_back", "We Have to Go Back!", "Complete every Storm Island Championship.", 50, G::StormIsland, T::Save, "championships", r_island_all},
    {"storm_island_champion", "Storm Island Champion", "Win the Storm Island Finale.", 50, G::StormIsland, T::Save, nullptr, r_island_champion},
    {"survive_gauntlet", "Survive the Gauntlet", "Complete your first Gauntlet event.", 20, G::StormIsland, T::Save, "Gauntlets", r_gauntlet1},
    {"gauntlet_runner", "Gauntlet Runner", "Win every Gauntlet event.", 30, G::StormIsland, T::Save, "Gauntlets", r_gauntlet_all},
    {"island_explorer", "Island Explorer", "Drive every road on Storm Island.", 25, G::StormIsland, T::Pending, nullptr, nullptr},
    {"smash_em_all", "Gotta Smash Em All!", "Collect every Storm Island Bonus Board.", 25, G::StormIsland, T::Save, "boards", r_island_boards},
    {"tick", "Tick!", "Complete your first Storm Island Bucket List challenge.", 10, G::StormIsland, T::Save, "challenges", r_island_bucket1},
    {"another_life", "See You in Another Life!", "Complete every Storm Island Bucket List challenge.", 25, G::StormIsland, T::Save, "challenges", r_island_bucket_all},
    {"castaways", "Castaways", "Complete every co-op Storm Island Bucket List challenge.", 30, G::StormIsland, T::Online, "challenges", r_island_coop_all},
    {"tropical_treasure", "Tropical Treasure", "Find and restore the Barn Find on Storm Island.", 25, G::StormIsland, T::Save, nullptr, r_island_barn},
    {"skills_for_thrills", "Skills for Thrills", "Bank a 100,000-point skill chain on Storm Island.", 20, G::StormIsland, T::Events, "points", r_island_chain},
    {"storm_breaker", "Storm Breaker", "Win 5 events in stormy conditions.", 20, G::StormIsland, T::Pending, nullptr, nullptr},
    {"ready_to_bounce", "Ready to Bounce", "Start a race on Storm Island with off-road tyres, suspension and transmission.", 10, G::StormIsland, T::Pending, nullptr, nullptr},
    {"fiesta_fun", "Fiesta Fun", "Win an event in the 1981 Ford Fiesta.", 10, G::StormIsland, T::Events, nullptr, r_fiesta},
    {"galant_great", "Galant Great", "Win an event in the Mitsubishi Galant VR-4.", 10, G::StormIsland, T::Events, nullptr, r_galant},
    {"metro_master", "Metro Master", "Win an event in the MG Metro 6R4.", 10, G::StormIsland, T::Events, nullptr, r_metro},
    {"ranger_rock_star", "Ranger Rock Star", "Win an event in the Ford Ranger T6.", 10, G::StormIsland, T::Events, nullptr, r_ranger},
    {"stadium_star", "Stadium Star", "Win an event in the Robby Gordon #7 SST.", 10, G::StormIsland, T::Events, nullptr, r_sst},
    {"meet_and_beat", "Meet and Beat", "Visit the Storm Island Car Meet and win 3 Showdowns.", 20, G::StormIsland, T::Pending, nullptr, nullptr},
    {"horizon_5_0", "Horizon 5-0", "Complete 50 online events on Storm Island.", 30, G::StormIsland, T::Online, nullptr, nullptr},

    // ---- Porsche Expansion: 15, 500G ----
    {"porsche_owners_club", "Porsche Owners Club", "Own your first Porsche.", 25, G::Porsche, T::Save, nullptr, r_porsche_owned},
    {"porsche_rookie", "Porsche Rookie", "Tick off 5 items on the Porsche Bucket List.", 25, G::Porsche, T::Save, "challenges", r_porsche_bucket5},
    {"porsche_pro", "Porsche Pro", "Complete the Porsche Bucket List.", 50, G::Porsche, T::Save, "challenges", r_porsche_bucket_all},
    {"horse_power", "Horse Power", "Complete 40 events in Porsches.", 100, G::Porsche, T::Events, "events", r_porsche40},
    {"air_cooled", "Air Cooled Competitor", "Complete 4 events in the 1970 Porsche 914/6.", 30, G::Porsche, T::Events, "events", r_914},
    {"cajun_kickin", "Cajun Kickin'", "Complete 4 events in the 2015 Porsche Macan Turbo.", 30, G::Porsche, T::Events, "events", r_macan},
    {"listen_to_it_roar", "Listen to it Roar!", "Complete 4 events in the 2003 Porsche Carrera GT.", 30, G::Porsche, T::Events, "events", r_carrera},
    {"ring_wraith", "Ring Wraith", "Complete 4 events in the 2014 Porsche 918 Spyder.", 30, G::Porsche, T::Events, "events", r_918},
    {"show_and_display", "Show and Display", "Complete 4 events in the 1987 Porsche 959.", 30, G::Porsche, T::Events, "events", r_959},
    {"this_is_my_porsche", "This Is My Porsche", "Complete 10 events in the same Porsche.", 50, G::Porsche, T::Events, "events", r_same_porsche},
    {"animal", "Animal!", "Pull off 20 Drift skills in the 2015 Porsche Cayman GTS.", 20, G::Porsche, T::Events, "drifts", r_cayman_drifts},
    {"call_911", "Call 911", "Pull off 30 Near Miss skills in the Porsche 911 Turbo S.", 20, G::Porsche, T::Events, "near misses", r_turbo_s_near_miss},
    {"cup_car", "Cup Car", "Earn 25,000 Cr in Rivals with the Porsche 911 GT3 RS 4.0.", 20, G::Porsche, T::Pending, nullptr, nullptr},
    {"no_airbag", "No Airbag Required", "Earn 20 Clean Racing skills in the 1989 Porsche 944 Turbo.", 20, G::Porsche, T::Events, "skills", r_944_clean},
    {"widow_maker", "Widow Maker", "Win a race in the wet in the 1982 Porsche 911 Turbo 3.3.", 20, G::Porsche, T::Pending, nullptr, nullptr},
};

}  // namespace

const std::vector<Def>& defs() { return kDefs; }

// ---- events -----------------------------------------------------------------------------------

bool count_event(const Event& e, const GameData& gd, Counters& c, LiveFacts& live) {
    switch (e.id) {
        case ev::CarPurchased:
            c.raise("bought", 1);
            return true;
        case ev::RewardBoardSmashed:      // [3] board id ("discount_board_xp_a_047"), [4] kind, [5] boards so far
        case ev::IslandRewardBoardSmashed:
            if (e.s(3).empty()) return false;
            return live.boardsSmashed.insert(e.s(3)).second;
        case ev::SkillAchieved: {         // [3] skill ("Drift"), [4] grade, [5] score, [6] car
            const std::string skill = e.s(3);
            if (skill.empty()) return false;
            const int64_t car = e.i(6);
            if (skill == "FastAndClean") c.add("skill.FastAndClean", 1);
            if ((skill == "Drift" && car == 2293) || (skill == "NearMiss" && car == 2289) ||
                (skill == "FastAndClean" && car == 268))
                c.add("skill." + skill + "." + std::to_string(car), 1);
            return skill == "FastAndClean" || car == 2293 || car == 2289 || car == 268;
        }
        case ev::SkillChainCompleted:     // [3] score, [4] skills in the chain, [5] multiplier
            if (e.world() != kWorldIsland) return false;
            c.raise("chain.island.best", e.i(3));
            return true;
        case ev::IslandSkillChainCompleted:
            c.raise("chain.island.best", e.i(3));
            return true;
        case ev::AnthemRaceEnd: {
            // [8] car, [9] exit status (0 = finished), [10] time, [11] place (1 = won), [12] event,
            // [14] championship.
            if (e.f.size() < 13 || e.i(9) != 0) return false;
            const int64_t car = e.i(8), place = e.i(11), event = e.i(12);
            const std::string cs = std::to_string(car);
            if (place == 1) {
                c.raise("won." + std::to_string(event), 1);
                c.add("win.car." + cs, 1);
            }
            if (gd.porsches.count(static_cast<int>(car))) {
                c.add("done.car." + cs, 1);
                c.add("done.porsche", 1);
            }
            return true;
        }
        default:
            return false;
    }
}

}  // namespace ach
