#pragma once

struct RoundSnapshot
{
	int number = 0;
	bool warmup = false;
	bool freeze = false;
	float startTime = 0;
	float restartTime = 0;
};

enum class RoundChange { None, Started, Ended };

// Observe the rules directly; network game events need not contain names.
class RoundTracker
{
public:
	RoundChange Observe(const RoundSnapshot& now)
	{
		RoundChange change = RoundChange::None;
		if (m_initialized)
		{
			if (now.number < m_previous.number || (now.warmup && !m_previous.warmup))
				m_lastAutomaticRound = -1;
			if (now.restartTime > 0 && m_previous.restartTime <= 0)
				change = RoundChange::Ended;
			else if (now.restartTime <= 0 &&
				((now.freeze && !m_previous.freeze) || m_previous.restartTime > 0 ||
				 now.number != m_previous.number ||
				 (!now.freeze && !m_previous.freeze && now.startTime != m_previous.startTime)))
				change = RoundChange::Started;
		}
		m_initialized = true;
		m_previous = now;
		return change;
	}

	bool ClaimAutomaticRound(int round, bool warmup)
	{
		if (warmup || round <= 0 || round % 10 || round == m_lastAutomaticRound)
			return false;
		m_lastAutomaticRound = round;
		return true;
	}

private:
	bool m_initialized = false;
	RoundSnapshot m_previous;
	int m_lastAutomaticRound = -1;
};
