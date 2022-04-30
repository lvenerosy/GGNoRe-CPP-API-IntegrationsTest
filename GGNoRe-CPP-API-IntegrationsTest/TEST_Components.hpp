/*
 * Copyright 2022 Loic Venerosy
 */

#pragma once

#include <GGNoRe-CPP-API.hpp>

#include <Input/CPT_IPT_TogglesPacket.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#if GGNORECPPAPI_LOG
#include <iostream>
#endif
#include <set>
#include <vector>

inline void TestLog(const std::string& Message)
{
#if GGNORECPPAPI_LOG
	std::cout << Message << "\n" << std::endl;
#endif
}

class TEST_CPT_IPT_Emulator final : public GGNoRe::API::ABS_CPT_IPT_Emulator
{
	struct Ownership
	{
		GGNoRe::API::DATA_Player Owner;
		uint16_t LatestOwnerChangeFrameIndex = 0;
	};
	Ownership CurrentOwnership;

	std::set<uint8_t> LocalMockInputs;
	const bool UseRandomInputs = false;

	std::vector<uint8_t> Inputs;

public:
	explicit TEST_CPT_IPT_Emulator(const bool UseRandomInputs)
		:UseRandomInputs(UseRandomInputs)
	{
	}

	inline GGNoRe::API::DATA_Player Owner() const { return CurrentOwnership.Owner; }

	inline const std::vector<uint8_t>& LatestInputs() const
	{
		assert(!Inputs.empty());
		return Inputs;
	}

	inline bool ShouldSendInputsToTarget(const uint8_t TargetSystemIndex) const
	{
		return CurrentOwnership.Owner.SystemIndex != TargetSystemIndex && CurrentOwnership.Owner.Local && !Inputs.empty();
	}

	~TEST_CPT_IPT_Emulator() = default;

protected:
	void OnRegisterActivationChange(const RegisterActivationChangeEvent RegisteredActivationChange, const ActivationChangeEvent ActivationChange) override
	{
		if (RegisteredActivationChange.Success == I_RB_Rollbackable::RegisterSuccess_E::Registered || RegisteredActivationChange.Success == I_RB_Rollbackable::RegisterSuccess_E::PreStart)
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

	void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const float PreActivationConsumedDeltaDurationInSeconds) override {}

	void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange) override {}

	void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
	void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
	void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
	void OnToNextFrame(const uint16_t FrameIndex) override {}

	const std::set<uint8_t>& OnPollLocalInputs() override
	{
		if (UseRandomInputs)
		{
			LocalMockInputs = { {(uint8_t)(rand() % GGNoRe::API::CPT_IPT_TogglesPacket::MaxInputToken())} };
		}

		return LocalMockInputs;
	}

	void OnReadyToSendInputs(const std::vector<uint8_t>& BinaryPacket) override
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
	float DeltaDurationAccumulatorInSeconds = 0.f; // To check that the de/serialization are consistent with OnSimulateTick

	void LogHumanReadable(const std::string& Message) const
	{
		TestLog(Message + " " + std::to_string(NonZero) + " " + std::to_string(InputsAccumulator) + " " + std::to_string(DeltaDurationAccumulatorInSeconds));
	}
};

class TEST_CPT_RB_SaveStates final : public GGNoRe::API::ABS_CPT_RB_SaveStates
{
	TEST_CPT_State& PlayerState;

public:
	TEST_CPT_RB_SaveStates(TEST_CPT_State& PlayerState)
		:PlayerState(PlayerState)
	{}

	~TEST_CPT_RB_SaveStates() = default;

protected:
	void OnRegisterActivationChange(const RegisterActivationChangeEvent RegisteredActivationChange, const ActivationChangeEvent ActivationChange) override
	{
		if (RegisteredActivationChange.Success != I_RB_Rollbackable::RegisterSuccess_E::Registered && RegisteredActivationChange.Success != I_RB_Rollbackable::RegisterSuccess_E::PreStart)
		{
			throw RegisteredActivationChange.Success;
		}
	}

	void OnActivationChange(const ActivationChangeEvent ActivationChange) override
	{
		if (ActivationChange.Type == ActivationChangeEvent::ChangeType_E::Activate)
		{
			// In order to make sure that the content is what is expected when activating
			PlayerState.LogHumanReadable("{TEST SAVE STATES ACTIVATE}");
		}
	}

	void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const float PreActivationConsumedDeltaDurationInSeconds) override {}

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

		PlayerState.LogHumanReadable("{TEST SAVE STATES SERIALIZE}");
	}

	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override
	{
		if (!SourceBuffer.empty())
		{
			std::memcpy(&PlayerState.NonZero, SourceBuffer.data(), sizeof(PlayerState.NonZero));
			size_t ReadOffset = sizeof(PlayerState.NonZero);

			std::memcpy(&PlayerState.InputsAccumulator, SourceBuffer.data() + ReadOffset, sizeof(PlayerState.InputsAccumulator));
			ReadOffset += sizeof(PlayerState.InputsAccumulator);

			std::memcpy(&PlayerState.DeltaDurationAccumulatorInSeconds, SourceBuffer.data() + ReadOffset, sizeof(PlayerState.DeltaDurationAccumulatorInSeconds));

			PlayerState.LogHumanReadable("{TEST SAVE STATES DESERIALIZE SUCCESS}");
		}
		else
		{
			PlayerState.LogHumanReadable("{TEST SAVE STATES DESERIALIZE FAILURE}");
		}
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
		if (RegisteredActivationChange.Success != I_RB_Rollbackable::RegisterSuccess_E::Registered && RegisteredActivationChange.Success != I_RB_Rollbackable::RegisterSuccess_E::PreStart)
		{
			throw RegisteredActivationChange.Success;
		}
	}

	void OnActivationChange(const ActivationChangeEvent ActivationChange) override {}

	void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const float PreActivationConsumedDeltaDurationInSeconds) override {}

	void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange) override {}

	void OnSimulateFrame(const uint16_t SimulatedFrameIndex, const std::set<uint8_t>& Inputs) override
	{
		for (auto Input : Inputs)
		{
			PlayerState.InputsAccumulator += Input;
		}
	}
	void OnSimulateTick(const float DeltaDurationInSeconds) override
	{
		PlayerState.DeltaDurationAccumulatorInSeconds += DeltaDurationInSeconds;
	}

	void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
	void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
	void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
	void OnToNextFrame(const uint16_t FrameIndex) override {}

	void ResetAndCleanup() noexcept override {}
};