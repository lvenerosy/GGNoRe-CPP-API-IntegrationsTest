/*
 * Copyright 2022 Loic Venerosy
 */

#pragma once

#include <GGNoRe-CPP-API.hpp>

#include <Input/CPT_IPT_TogglesPacket.hpp>

#include <cassert>
#include <cstring>
#ifdef GGNORECPPAPI_LOG
#include <iostream>
#endif
#include <set>
#include <vector>

inline void TestLog(const std::string& Message)
{
#ifdef GGNORECPPAPI_LOG
	std::cout << Message << "\n" << std::endl;
#endif
}

class TEST_CPT_IPT_Emulator final : public GGNoRe::API::ABS_CPT_IPT_Emulator
{
	// The module uses pimpl for the private state but for brevity's sake, tests do not
	struct Ownership
	{
		GGNoRe::API::DATA_Player Owner;
		uint16_t LatestOwnerChangeFrameIndex = 0;
	};
	Ownership CurrentOwnership;

	std::set<uint8_t> LocalMockInputs;

	std::vector<uint8_t> Inputs;

public:
	TEST_CPT_IPT_Emulator() = default;

	inline GGNoRe::API::DATA_Player Owner() const { return CurrentOwnership.Owner; }

	inline const std::vector<uint8_t>& LatestInputs() const
	{
		assert(!Inputs.empty());
		return Inputs;
	}

	inline bool ShouldSendInputsToTarget(const uint8_t TargetSystemIndex) const
	{
		return CurrentOwnership.Owner.SystemIndex != TargetSystemIndex && CurrentOwnership.Owner.Local;
	}

	~TEST_CPT_IPT_Emulator() = default;

protected:
	void OnRegisterActivationChange(const RegisterActivationChangeEvent RegisteredActivationChange, const ActivationChangeEvent ActivationChange) override
	{
		if (RegisteredActivationChange.Success == I_RB_Rollbackable::RegisterSuccess_E::Registered)
		{
			// Both OnRegisterActivationChange and ShouldSendInputsToTarget happen outside of the simulation so they can be properly ordered so that the owner is valid
			// The change of owner happens here and not inside OnActivationChange because the transfer of inputs should happen under the real most recent ownership as opposed to the simulated one
			if (ActivationChange.FrameIndex >= CurrentOwnership.LatestOwnerChangeFrameIndex)
			{
				CurrentOwnership = { ActivationChange.Owner, ActivationChange.FrameIndex };
			}
		}
		else
		{
			throw RegisteredActivationChange.Success;
		}
	}

	void OnActivationChange(const ActivationChangeEvent ActivationChange) override
	{
		if (ActivationChange.Type == ActivationChangeEvent::ChangeType_E::Activate)
		{
			LocalMockInputs = { { (uint8_t)ActivationChange.Owner.Id }, {10}, {20} };
		}
	}

	// All the virtual methods are pure so there is no ambiguity whether to call the parent's implementation or not https://en.wikipedia.org/wiki/Liskov_substitution_principle
	// The parent's implementation is separated by using the bridge pattern https://en.wikipedia.org/wiki/Bridge_pattern
	void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const GGNoRe::API::SER_FixedPoint PreActivationConsumedDeltaDurationInSeconds) override {}

	void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange) override {}

	void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
	void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
	void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
	void OnToNextFrame(const uint16_t FrameIndex) override {}

	const std::set<uint8_t>& OnPollLocalInputs() override
	{
		return LocalMockInputs;
	}

	void OnReadyToUploadInputs(const std::vector<uint8_t>& BinaryPacket) override
	{
		Inputs = BinaryPacket;
	}

	void ResetAndCleanup() noexcept override
	{
		Inputs.clear();
		LocalMockInputs.clear();
	}
};

struct TEST_CPT_State
{
	uint16_t NonZero = 1; // Necessary otherwise the other fields initialized to 0 generate a checksum with a value of 0 which is considered as being a missing checksum
	uint8_t InputsAccumulator = 0; // To check that the de/serialization are consistent with OnSimulateFrame
	// The accumulator uses fixed point instead of regular floats because addition introduces error
	// In user code you can use the conversion back to float as long as you don't modify the value you get, or if you modify it, the particular piece of state it affects should not be part of the serialization (sound/particle playback possibly)
	GGNoRe::API::SER_FixedPoint DeltaDurationAccumulatorInSeconds = 0.f; // To check that the de/serialization are consistent with OnSimulateTick

	void LogHumanReadable(const std::string& Message) const
	{
		TestLog(Message + " " + std::to_string(NonZero) + " " + std::to_string(InputsAccumulator) + " " + std::to_string(DeltaDurationAccumulatorInSeconds));
	}
};

class TEST_CPT_RB_SaveStates final : public GGNoRe::API::ABS_CPT_RB_SaveStates
{
	TEST_CPT_State& PlayerState;
	// The id is stored here for logging purposes, unnecessary during real use
	GGNoRe::API::id_t PlayerId = 0;

public:
	TEST_CPT_RB_SaveStates(TEST_CPT_State& PlayerState)
		:PlayerState(PlayerState)
	{}

	~TEST_CPT_RB_SaveStates() = default;

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
		PlayerId = ActivationChange.Owner.Id;
	}

	// All the virtual methods are pure so there is no ambiguity whether to call the parent's implementation or not https://en.wikipedia.org/wiki/Liskov_substitution_principle
	// The parent's implementation is separated by using the bridge pattern https://en.wikipedia.org/wiki/Bridge_pattern
	void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const GGNoRe::API::SER_FixedPoint PreActivationConsumedDeltaDurationInSeconds) override {}

	void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange) override {}

	void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
	void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
	void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
	void OnToNextFrame(const uint16_t FrameIndex) override {}

	void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override
	{
		TargetBufferOut.resize(sizeof(PlayerState.NonZero) + sizeof(PlayerState.InputsAccumulator) + sizeof(PlayerState.DeltaDurationAccumulatorInSeconds));

		std::memcpy(TargetBufferOut.data(), &PlayerState.NonZero, sizeof(PlayerState.NonZero));
		size_t CopyOffset = sizeof(PlayerState.NonZero);

		std::memcpy(TargetBufferOut.data() + CopyOffset, &PlayerState.InputsAccumulator, sizeof(PlayerState.InputsAccumulator));
		CopyOffset += sizeof(PlayerState.InputsAccumulator);

		std::memcpy(TargetBufferOut.data() + CopyOffset, &PlayerState.DeltaDurationAccumulatorInSeconds, sizeof(PlayerState.DeltaDurationAccumulatorInSeconds));

		PlayerState.LogHumanReadable("{TEST SAVE STATES SERIALIZE - PLAYER " + std::to_string(PlayerId) + "}");
	}

	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override
	{
		assert(!SourceBuffer.empty());

		std::memcpy(&PlayerState.NonZero, SourceBuffer.data(), sizeof(PlayerState.NonZero));
		size_t ReadOffset = sizeof(PlayerState.NonZero);

		std::memcpy(&PlayerState.InputsAccumulator, SourceBuffer.data() + ReadOffset, sizeof(PlayerState.InputsAccumulator));
		ReadOffset += sizeof(PlayerState.InputsAccumulator);

		std::memcpy(&PlayerState.DeltaDurationAccumulatorInSeconds, SourceBuffer.data() + ReadOffset, sizeof(PlayerState.DeltaDurationAccumulatorInSeconds));

		PlayerState.LogHumanReadable("{TEST SAVE STATES DESERIALIZE - PLAYER " + std::to_string(PlayerId) + "}");
	}

	void ResetAndCleanup() noexcept override {}
};

class TEST_CPT_RB_Simulator final : public GGNoRe::API::ABS_CPT_RB_Simulator
{
	TEST_CPT_State& PlayerState;

public:
	TEST_CPT_RB_Simulator(TEST_CPT_State& PlayerState)
		:PlayerState(PlayerState)
	{}

	~TEST_CPT_RB_Simulator() = default;

protected:
	void OnRegisterActivationChange(const RegisterActivationChangeEvent RegisteredActivationChange, const ActivationChangeEvent ActivationChange) override
	{
		if (RegisteredActivationChange.Success != I_RB_Rollbackable::RegisterSuccess_E::Registered)
		{
			throw RegisteredActivationChange.Success;
		}
	}

	// All the virtual methods are pure so there is no ambiguity whether to call the parent's implementation or not https://en.wikipedia.org/wiki/Liskov_substitution_principle
	// The parent's implementation is separated by using the bridge pattern https://en.wikipedia.org/wiki/Bridge_pattern
	void OnActivationChange(const ActivationChangeEvent ActivationChange) override {}

	void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const GGNoRe::API::SER_FixedPoint PreActivationConsumedDeltaDurationInSeconds) override {}

	void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange) override {}

	void OnSimulateFrame(const uint16_t SimulatedFrameIndex, const std::set<uint8_t>& Inputs) override
	{
		for (auto Input : Inputs)
		{
			PlayerState.InputsAccumulator += Input;
		}
	}

	void OnSimulateTick(const GGNoRe::API::SER_FixedPoint DeltaDurationInSeconds) override
	{
		PlayerState.DeltaDurationAccumulatorInSeconds += DeltaDurationInSeconds;
	}

	void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
	void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
	void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
	void OnToNextFrame(const uint16_t FrameIndex) override {}

	void ResetAndCleanup() noexcept override {}
};