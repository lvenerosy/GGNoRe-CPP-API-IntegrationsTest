/*
 * Copyright 2022 Loic Venerosy
 */

#pragma once

#include <Input/CPT_IPT_TogglesPacket.hpp>
#include <TEST_Fireball.hpp>

#include <array>
#include <cstring>
#include <map>
#include <set>

namespace TEST_NSPC_Systems
{
	const GGNoRe::API::id_t Player1Id = 0;
	const GGNoRe::API::id_t Player2Id = 1;

	constexpr std::array<uint8_t, 2> FireballCombo{ 20, 10 };

	const std::map<GGNoRe::API::id_t, std::vector<std::set<uint8_t>>> PlayerIdToInputPattern
	{
		std::make_pair(Player1Id, std::vector<std::set<uint8_t>>{ { (uint8_t)Player1Id }, {}, {}, { 10 }, { 10, FireballCombo[0] }, { FireballCombo[1] }, {} }),
		std::make_pair(Player2Id, std::vector<std::set<uint8_t>>{ { (uint8_t)Player2Id }, {}, {}, { 10 }, { 10, FireballCombo[0] }, { FireballCombo[1] }, {} })
	};
}

// This is a player class grouping the 3 components for simplicity's sake
// In your game it could be a fireball with only save states and a simulator, or a player input controller with only an emulator
class TEST_Player final
{
public:
	struct TEST_CPT_State
	{
		uint16_t NonZero = 1; // Necessary otherwise the other fields initialized to 0 generate a checksum with a value of 0 which is considered as being a missing checksum
		uint8_t InputsAccumulator = 0; // To check that the de/serialization are consistent with OnSimulateFrame
		// The accumulator uses fixed point instead of regular floats because addition introduces error
		// In user code you can use the conversion back to float as long as you don't modify the value you get, or if you modify it, the particular piece of state it affects should not be part of the serialization (sound/particle playback possibly)
		GGNoRe::API::SER_FixedPoint DeltaDurationAccumulatorInSeconds = 0.f; // To check that the de/serialization are consistent with OnSimulateTick
		bool PrimedForFireball = false; // True when detecting the first input of the combo

		void LogHumanReadable(const std::string& Message) const
		{
			TestLog(Message + " " + std::to_string(NonZero) + " " + std::to_string(InputsAccumulator) + " " + std::to_string(DeltaDurationAccumulatorInSeconds) + " " + std::to_string(PrimedForFireball));
		}
	};

private:
	class TEST_CPT_IPT_Emulator final : public GGNoRe::API::ABS_CPT_IPT_Emulator
	{
		// The module uses pimpl for the private state but for brevity's sake, tests do not
		struct Ownership
		{
			GGNoRe::API::DATA_Player Owner;
			uint16_t LatestOwnerChangeFrameIndex = 0;
		};
		Ownership CurrentOwnership;

		std::vector<std::set<uint8_t>>::const_iterator InputsIterator;

		std::vector<uint8_t> Inputs;

	public:
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
					if (ActivationChange.Owner.Local)
					{
						InputsIterator = TEST_NSPC_Systems::PlayerIdToInputPattern.find(ActivationChange.Owner.Id)->second.cbegin();

						assert(InputsIterator != TEST_NSPC_Systems::PlayerIdToInputPattern.find(ActivationChange.Owner.Id)->second.cend());
					}

					CurrentOwnership = { ActivationChange.Owner, ActivationChange.FrameIndex };
				}
			}
			else
			{
				throw RegisteredActivationChange.Success;
			}
		}

		// All the virtual methods are pure so there is no ambiguity whether to call the parent's implementation or not https://en.wikipedia.org/wiki/Liskov_substitution_principle
		// The parent's implementation is separated by using the bridge pattern https://en.wikipedia.org/wiki/Bridge_pattern
		void OnActivationChange(const ActivationChangeEvent ActivationChange, const SimulationStage_E) override {}

		void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const GGNoRe::API::SER_FixedPoint PreActivationConsumedDeltaDurationInSeconds) override {}

		void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange, const SimulationStage_E) override {}

		void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
		void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
		void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
		void OnToNextFrame(const uint16_t FrameIndex) override
		{
			if (CurrentOwnership.Owner.Local && ++InputsIterator == TEST_NSPC_Systems::PlayerIdToInputPattern.find(CurrentOwnership.Owner.Id)->second.cend())
			{
				InputsIterator = TEST_NSPC_Systems::PlayerIdToInputPattern.find(CurrentOwnership.Owner.Id)->second.cbegin();
			}
		}

		const std::set<uint8_t>& OnPollLocalInputs() override
		{
			return *InputsIterator;
		}

		void OnReadyToUploadInputs(const std::vector<uint8_t>& BinaryPacket) override
		{
			Inputs = BinaryPacket;
		}

		void ResetAndCleanup() noexcept override
		{
			Inputs.clear();
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

		void OnActivationChange(const ActivationChangeEvent ActivationChange, const SimulationStage_E) override
		{
			PlayerId = ActivationChange.Owner.Id;
		}

		// All the virtual methods are pure so there is no ambiguity whether to call the parent's implementation or not https://en.wikipedia.org/wiki/Liskov_substitution_principle
		// The parent's implementation is separated by using the bridge pattern https://en.wikipedia.org/wiki/Bridge_pattern
		void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const GGNoRe::API::SER_FixedPoint PreActivationConsumedDeltaDurationInSeconds) override {}

		void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange, const SimulationStage_E) override {}

		void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
		void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
		void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
		void OnToNextFrame(const uint16_t FrameIndex) override {}

		void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override
		{
			TargetBufferOut.resize(sizeof(PlayerState.NonZero) + sizeof(PlayerState.InputsAccumulator) + sizeof(PlayerState.DeltaDurationAccumulatorInSeconds) + sizeof(PlayerState.PrimedForFireball));

			std::memcpy(TargetBufferOut.data(), &PlayerState.NonZero, sizeof(PlayerState.NonZero));
			size_t CopyOffset = sizeof(PlayerState.NonZero);

			std::memcpy(TargetBufferOut.data() + CopyOffset, &PlayerState.InputsAccumulator, sizeof(PlayerState.InputsAccumulator));
			CopyOffset += sizeof(PlayerState.InputsAccumulator);

			std::memcpy(TargetBufferOut.data() + CopyOffset, &PlayerState.DeltaDurationAccumulatorInSeconds, sizeof(PlayerState.DeltaDurationAccumulatorInSeconds));
			CopyOffset += sizeof(PlayerState.DeltaDurationAccumulatorInSeconds);

			std::memcpy(TargetBufferOut.data() + CopyOffset, &PlayerState.PrimedForFireball, sizeof(PlayerState.PrimedForFireball));

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
			ReadOffset += sizeof(PlayerState.DeltaDurationAccumulatorInSeconds);

			std::memcpy(&PlayerState.PrimedForFireball, SourceBuffer.data() + ReadOffset, sizeof(PlayerState.PrimedForFireball));

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
		void OnActivationChange(const ActivationChangeEvent ActivationChange, const SimulationStage_E) override {}

		void OnActivationChangeStartingFrame(const ActivationChangeEvent ActivationChange, const GGNoRe::API::SER_FixedPoint PreActivationConsumedDeltaDurationInSeconds) override {}

		void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange, const SimulationStage_E) override {}

		void OnSimulateFrame(const uint16_t SimulatedFrameIndex, const std::set<uint8_t>& Inputs) override
		{
			for (auto Input : Inputs)
			{
				PlayerState.InputsAccumulator += Input;
			}

			if (PlayerState.PrimedForFireball && Inputs.find(TEST_NSPC_Systems::FireballCombo[1]) != Inputs.cend())
			{
				TEST_Fireball::CastFireball(OwnerAtFrame(SimulatedFrameIndex));
			}

			PlayerState.PrimedForFireball = false;

			if (Inputs.find(TEST_NSPC_Systems::FireballCombo[0]) != Inputs.cend())
			{
				PlayerState.PrimedForFireball = true;
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

	// In tests global state is used as a shortcut
	// The module has no global state beside configuration and active components trackers so the user does not have to manually instantiate them, but it could easily be made free of any global state
	static std::set<TEST_Player*> PlayersInternal;
	static uint32_t DebugIdCounter;

	// To identify more quickly which is which when debugging
	uint32_t DebugId = 0;

	TEST_CPT_State StateInternal;

	TEST_CPT_IPT_Emulator EmulatorInternal;
	TEST_CPT_RB_SaveStates SaveStatesInternal;
	TEST_CPT_RB_Simulator SimulatorInternal;

	void OnActivateNow(const GGNoRe::API::DATA_Player Owner)
	{
		assert(PlayersInternal.find(this) == PlayersInternal.cend());

		DebugId = DebugIdCounter++;

		PlayersInternal.insert(this);

		EmulatorInternal.ChangeActivationNow(Owner, GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
		SaveStatesInternal.ChangeActivationNow(Owner, GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
		SimulatorInternal.ChangeActivationNow(Owner, GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
	}

	void OnActivateInPast(const GGNoRe::API::DATA_Player Owner, const uint16_t StartFrameIndex)
	{
		assert(PlayersInternal.find(this) == PlayersInternal.cend());

		DebugId = DebugIdCounter++;

		PlayersInternal.insert(this);

		EmulatorInternal.ChangeActivationInPast({ GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
		SaveStatesInternal.ChangeActivationInPast({ GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
		SimulatorInternal.ChangeActivationInPast({ GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
	}

public:
	TEST_Player()
		:EmulatorInternal(), SaveStatesInternal(StateInternal), SimulatorInternal(StateInternal)
	{
	}

	~TEST_Player()
	{
		PlayersInternal.erase(this);
	}

	static inline const std::set<TEST_Player*>& Players()
	{
		return PlayersInternal;
	}

	inline const TEST_CPT_State& State() const
	{
		return StateInternal;
	}

	inline const TEST_CPT_IPT_Emulator& Emulator() const
	{
		return EmulatorInternal;
	}

	void ActivateNow(const GGNoRe::API::DATA_Player Owner)
	{
		assert(Owner.Local);

		OnActivateNow(Owner);
	}

	void ActivateNow(const GGNoRe::API::DATA_Player Owner, const TEST_CPT_State InitialState)
	{
		assert(!Owner.Local);

		StateInternal = InitialState;

		OnActivateNow(Owner);
	}

	void ActivateInPast(const GGNoRe::API::DATA_Player Owner, const uint16_t StartFrameIndex)
	{
		assert(Owner.Local);

		OnActivateInPast(Owner, StartFrameIndex);
	}

	void ActivateInPast(const GGNoRe::API::DATA_Player Owner, const uint16_t StartFrameIndex, const TEST_CPT_State InitialState)
	{
		assert(!Owner.Local);

		StateInternal = InitialState;

		OnActivateInPast(Owner, StartFrameIndex);
	}
};

std::set<TEST_Player*> TEST_Player::PlayersInternal;
uint32_t TEST_Player::DebugIdCounter = 0;