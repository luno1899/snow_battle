#include "round_tracker.h"
#include <cstdio>

#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #value); return 1; } } while (0)

int main()
{
	RoundTracker tracker;
	RoundSnapshot rules{9, false, false, 100, 0};
	CHECK(tracker.Observe(rules) == RoundChange::None);
	CHECK(!tracker.ClaimAutomaticRound(9, false));
	rules.restartTime = 150;
	CHECK(tracker.Observe(rules) == RoundChange::Ended);
	rules.number = 10; // Counter can update while the previous round is ending.
	CHECK(tracker.Observe(rules) == RoundChange::None);
	rules.restartTime = 0;
	rules.freeze = true;
	rules.startTime = 150;
	CHECK(tracker.Observe(rules) == RoundChange::Started);
	CHECK(tracker.ClaimAutomaticRound(10, false));
	CHECK(!tracker.ClaimAutomaticRound(10, false));
	CHECK(tracker.Observe(rules) == RoundChange::None);
	rules.freeze = false;
	rules.startTime = 165; // Freeze end updates the timestamp; NOT a new round.
	CHECK(tracker.Observe(rules) == RoundChange::None);
	rules.restartTime = 200;
	CHECK(tracker.Observe(rules) == RoundChange::Ended);
	CHECK(!tracker.ClaimAutomaticRound(10, false)); // Late broadcast cannot re-enable it.
	rules = {11, false, true, 200, 0};
	CHECK(tracker.Observe(rules) == RoundChange::Started);
	CHECK(!tracker.ClaimAutomaticRound(11, false));
	rules = {12, false, false, 250, 0}; // mp_freezetime 0, skipped network events.
	CHECK(tracker.Observe(rules) == RoundChange::Started);
	CHECK(!tracker.ClaimAutomaticRound(12, false));
	CHECK(!tracker.ClaimAutomaticRound(20, true));
	CHECK(tracker.ClaimAutomaticRound(20, false));
	CHECK(!tracker.ClaimAutomaticRound(20, false));
	rules = {1, false, true, 300, 0}; // A restarted match has its own tenth round.
	CHECK(tracker.Observe(rules) == RoundChange::Started);
	CHECK(tracker.ClaimAutomaticRound(10, false));
	return 0;
}
