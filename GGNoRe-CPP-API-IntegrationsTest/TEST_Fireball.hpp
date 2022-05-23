/*
 * Copyright 2022 Loic Venerosy
 */

#pragma once

#include <GGNoRe-CPP-API.hpp>

#include <cassert>
#ifdef GGNORECPPAPI_LOG
#include <iostream>
#endif
#include <memory>
#include <vector>

inline void TestLog(const std::string& Message)
{
#ifdef GGNORECPPAPI_LOG
	std::cout << Message << "\n" << std::endl;
#endif
}

// Only there to test that the life time is properly managed when rollbacking
// Could be expanded upon in order to make the overall test suite even more robust
class TEST_Fireball final
{
	// In tests global state is used as a shortcut
	// The module has no global state beside configuration and active components trackers so the user does not have to manually instantiate them, but it could easily be made free of any global state
	static std::vector<std::unique_ptr<TEST_Fireball>> Tracker;

	class TEST_CPT_RB_Simulator final : public GGNoRe::API::ABS_CPT_RB_Simulator
	{
		const TEST_Fireball* PublicSelf;

		uint16_t StartFrameIndex = 0;

	public:
		TEST_CPT_RB_Simulator(const TEST_Fireball* PublicSelf, const GGNoRe::API::DATA_Player Owner)
			:PublicSelf(PublicSelf)
		{
			assert(PublicSelf != nullptr);

			ChangeActivationNow(Owner, ActivationChangeEvent::ChangeType_E::Activate);
		}

		~TEST_CPT_RB_Simulator() = default;

	protected:
		void OnRegisterActivationChange(const RegisterActivationChangeEvent RegisteredActivationChange, const ActivationChangeEvent ActivationChange) override
		{
			if (RegisteredActivationChange.Success != I_RB_Rollbackable::RegisterSuccess_E::Registered)
			{
				throw RegisteredActivationChange.Success;
			}
		}
		
		void OnActivationChange(const ActivationChangeEvent ActivationChange) override
		{
			if (ActivationChange.Type == ActivationChangeEvent::ChangeType_E::Activate)
			{
				StartFrameIndex = ActivationChange.FrameIndex;
			}
		}

		// All the virtual methods are pure so there is no ambiguity whether to call the parent's implementation or not https://en.wikipedia.org/wiki/Liskov_substitution_principle
		// The parent's implementation is separated by using the bridge pattern https://en.wikipedia.org/wiki/Bridge_pattern
		void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const GGNoRe::API::SER_FixedPoint PreActivationConsumedDeltaDurationInSeconds) override {}

		void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange) override {}

		void OnSimulateFrame(const uint16_t SimulatedFrameIndex, const std::set<uint8_t>& Inputs) override
		{
			assert(SimulatedFrameIndex >= StartFrameIndex);

			const uint16_t LifetimeInFrames = 2;
			assert(SimulatedFrameIndex <= LifetimeInFrames + StartFrameIndex);
			if (SimulatedFrameIndex == LifetimeInFrames + StartFrameIndex)
			{
				ChangeActivationNow(OwnerAtFrame(SimulatedFrameIndex), ActivationChangeEvent::ChangeType_E::Deactivate);
			}
		}

		void OnSimulateTick(const GGNoRe::API::SER_FixedPoint DeltaDurationInSeconds) override {}

		void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
		void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
		void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
		void OnToNextFrame(const uint16_t FrameIndex) override {}

		void ResetAndCleanup() noexcept override
		{
			Tracker.erase(
				std::find_if(Tracker.cbegin(), Tracker.cend(),
					[&](const std::unique_ptr<TEST_Fireball>& Fireball) -> bool
					{
						return Fireball.get() == PublicSelf;
					}
				)
			);
		}
	};

	friend TEST_CPT_RB_Simulator;

	TEST_CPT_RB_Simulator Simulator;

	explicit TEST_Fireball(const GGNoRe::API::DATA_Player Owner)
		:Simulator(this, Owner)
	{}

public:
	static void CastFireball(const GGNoRe::API::DATA_Player Owner)
	{
		// Uses new instead of make_unique but it looks cleaner compared to passkey idiom
		Tracker.emplace_back(std::unique_ptr<TEST_Fireball>(new TEST_Fireball(Owner)));
	}
};

std::vector<std::unique_ptr<TEST_Fireball>> TEST_Fireball::Tracker;