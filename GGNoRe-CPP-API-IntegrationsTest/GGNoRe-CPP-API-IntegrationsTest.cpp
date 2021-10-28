/*
 * Copyright 2021 Loïc Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <Input/IPT_VerifiableInputs_STRC.hpp>

using namespace GGNoRe::API;

bool TestBasicRollback()
{
	DATA_CFG ConfigWithNoFakedDelay;
	ConfigWithNoFakedDelay.FakedMissedPredictionsFramesCount = 0; // Comment out to use the fake rollback
	DATA_CFG::Load(ConfigWithNoFakedDelay);

	const uint16_t LocalPlayerIndex = 0;
	const uint16_t RemotePlayerIndex = LocalPlayerIndex + 1;

	TEST_CPT_IPT_Emulator LocalPlayerEmulator(LocalPlayerIndex);
	TEST_CPT_IPT_Emulator RemotePlayerEmulator(RemotePlayerIndex);

	TEST_CPT_RB_SaveStates LocalPlayerSaveStates(LocalPlayerIndex);
	TEST_CPT_RB_SaveStates RemotePlayerSaveStates(RemotePlayerIndex);

	TEST_CPT_RB_Simulator LocalPlayerSimulator(LocalPlayerIndex);
	TEST_CPT_RB_Simulator RemotePlayerSimulator(RemotePlayerIndex);

	std::map<uint16_t, std::set<uint8_t>> SingleFrameLocalPlayerIndexToMockInputs;
	SingleFrameLocalPlayerIndexToMockInputs.emplace(std::pair<uint16_t, std::set<uint8_t>>(LocalPlayerIndex, { {0}, {1}, {2} }));

	const uint16_t DelayFramesCount = (uint16_t)DATA_CFG::Get().RollbackConfiguration.DelayFramesCount;
	const uint16_t StartFrameIndex = 0;

	std::map<uint16_t, IPT_VerifiableInputs_STRC> RemoteFrameIndexToVerifiableInputs;
	// In the real use case of VerifiableBuffer, for the delay frame count + 1, the inputs are initialized to empty since the player interaction is stored for the future
	for (uint16_t InitialFrameIndex = StartFrameIndex; InitialFrameIndex < StartFrameIndex + DelayFramesCount + 1; InitialFrameIndex++)
	{
		// Add remote inputs manually as if they came from the internet
		RemoteFrameIndexToVerifiableInputs.emplace(
			std::pair<uint16_t, IPT_VerifiableInputs_STRC>(InitialFrameIndex, { {}, 0, false })
		);
	}

	const uint16_t FrameAdvantageInFrames = 2;
	const uint16_t DelayedInputsStartFrameIndex = StartFrameIndex + DelayFramesCount + 1;

	uint16_t CurrentFrameIndex = StartFrameIndex;

	for (; CurrentFrameIndex < DelayedInputsStartFrameIndex;)
	{
		auto Success = ABS_CPT_IPT_Emulator::TryTickingToNextFrame({
			DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / 2.f, SingleFrameLocalPlayerIndexToMockInputs
			});

		if (Success == ABS_CPT_IPT_Emulator::TickSuccess_E::ToNext)
		{
			CurrentFrameIndex++;
		}
	}

	std::vector<std::set<uint8_t>> RemotePlayerMockInputTogglesBuffer{ {0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9} };
	const uint16_t MockRunDurationInFrames = (uint16_t)RemotePlayerMockInputTogglesBuffer.size() + DelayedInputsStartFrameIndex;
	// TODO: make sure it works to test starvation
	const uint16_t ReceiveRemoteIntervalInFrames = 3; // If bigger than the rollback buffer, will starve

	for (; CurrentFrameIndex < StartFrameIndex + MockRunDurationInFrames;)
	{
		// Every ReceiveRemoteIntervalInFrames frames
		if (CurrentFrameIndex % ReceiveRemoteIntervalInFrames == 0 && CurrentFrameIndex - FrameAdvantageInFrames > ReceiveRemoteIntervalInFrames)
		{
			for (uint16_t RemoteFrameIndexOffset = 0; RemoteFrameIndexOffset < ReceiveRemoteIntervalInFrames; RemoteFrameIndexOffset++)
			{
				const uint16_t RemoteFrameIndexDelayedByLocalAdvantage = RemoteFrameIndexOffset + CurrentFrameIndex - ReceiveRemoteIntervalInFrames - FrameAdvantageInFrames;
				// Add remote inputs manually as if they came from the internet
				RemoteFrameIndexToVerifiableInputs.emplace(
					std::pair<uint16_t, IPT_VerifiableInputs_STRC>(
						RemoteFrameIndexDelayedByLocalAdvantage,
						{
							// TODO: fix the index so it starts from the beginning of the buffer
							RemotePlayerMockInputTogglesBuffer[RemoteFrameIndexDelayedByLocalAdvantage - StartFrameIndex],
							ABS_CPT_RB_SaveStates::ComputeChecksum(RemoteFrameIndexDelayedByLocalAdvantage),
							false
						}
					)
				);

				if (RemoteFrameIndexToVerifiableInputs.size() > DATA_CFG::Get().RollbackInputBufferMaxSize())
				{
					RemoteFrameIndexToVerifiableInputs.erase(RemoteFrameIndexToVerifiableInputs.begin());
				}
			}

			// Basically a test for the binary upload/download
			RemotePlayerEmulator.DownloadPlayerBinary(RemotePlayerEmulator.InputsToBinary(RemoteFrameIndexToVerifiableInputs).data());
		}

		auto Success = ABS_CPT_IPT_Emulator::TryTickingToNextFrame({
			DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / 2.f, SingleFrameLocalPlayerIndexToMockInputs
		});

		if (Success == ABS_CPT_IPT_Emulator::TickSuccess_E::ToNext)
		{
			CurrentFrameIndex++;
		}
	}

	return true;
}