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

	const uint16_t LocalSystemIndex = 0;
	const uint16_t MockRemoteSystemIndex = LocalSystemIndex + 1;

	TEST_CPT_IPT_Emulator LocalPlayerEmulator(LocalPlayerIndex, LocalSystemIndex);
	TEST_CPT_IPT_Emulator RemotePlayerEmulator(RemotePlayerIndex, LocalSystemIndex);

	TEST_CPT_RB_SaveStates LocalPlayerSaveStates(LocalPlayerIndex, LocalSystemIndex);
	TEST_CPT_RB_SaveStates RemotePlayerSaveStates(RemotePlayerIndex, LocalSystemIndex);

	TEST_CPT_RB_Simulator LocalPlayerSimulator(LocalPlayerIndex, LocalSystemIndex);
	TEST_CPT_RB_Simulator RemotePlayerSimulator(RemotePlayerIndex, LocalSystemIndex);

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
		auto Success = SystemMultiton::GetEmulator(LocalSystemIndex).TryTickingToNextFrame({
			DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / 2.f, SingleFrameLocalPlayerIndexToMockInputs
			});

		if (Success == ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::ToNext)
		{
			CurrentFrameIndex++;
		}
	}

	std::vector<std::set<uint8_t>> RemotePlayerMockInputTogglesBuffer{ {0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9} };
	const uint16_t MockRunDurationInFrames = (uint16_t)RemotePlayerMockInputTogglesBuffer.size() + DelayedInputsStartFrameIndex;
	const uint16_t ReceiveRemoteIntervalInFrames = 3;
	// Bigger than the rollback buffer, will starve
	//const uint16_t ReceiveRemoteIntervalInFrames = (uint16_t)DATA_CFG::Get().RollbackBufferMaxSize() + DelayFramesCount + 1;

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
							SystemMultiton::GetSaveStates(LocalSystemIndex).ComputeChecksum(RemoteFrameIndexDelayedByLocalAdvantage),
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
			SystemMultiton::GetEmulator(LocalSystemIndex).DownloadPlayerBinary(RemotePlayerEmulator.InputsToBinary(RemoteFrameIndexToVerifiableInputs).data());
		}

		auto Success = SystemMultiton::GetEmulator(LocalSystemIndex).TryTickingToNextFrame({
			DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / 2.f, SingleFrameLocalPlayerIndexToMockInputs
		});

		if (Success == ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::ToNext)
		{
			CurrentFrameIndex++;
		}

		switch (Success)
		{
		case ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::StallAdvantage:
			std::cout << "###########STALLING############" << std::endl;
			break;
		case ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::StarvedForInput:
			std::cout << "###########STARVED############" << std::endl;
			break;
		case ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::StayCurrent:
			std::cout << "###########STAY############" << std::endl;
			break;
		case ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::ToNext:
			std::cout << "###########NEXT############" << std::endl;
			break;
		default:
			std::cout << "###########DEFAULT############" << std::endl;
			break;
		}
	}

	return true;
}