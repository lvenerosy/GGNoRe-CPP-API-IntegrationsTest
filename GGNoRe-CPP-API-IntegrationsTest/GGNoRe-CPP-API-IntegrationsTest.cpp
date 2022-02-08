/*
 * Copyright 2021 Loïc Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <GGNoRe-CPP-API.hpp>

#include <functional>
#include <iostream>
#include <stdlib.h>

using namespace GGNoRe::API;

class TEST_CPT_IPT_Emulator final : public ABS_CPT_IPT_Emulator
{
public:
	std::set<uint8_t> LocalMockInputs;
	std::function<void(const std::vector<uint8_t>&)> DownloadRemoteInputs;

protected:
	const std::set<uint8_t>& OnPollLocalInputs() override
	{
		return LocalMockInputs;
	}

	void OnReadyToSendInputs(const std::vector<uint8_t>& BinaryPayload) override
	{
		DownloadRemoteInputs(BinaryPayload);
	}

	void OnDestroy() override {}
};

class TEST_CPT_RB_Simulator;
class TEST_CPT_RB_SaveStates final : public ABS_CPT_RB_SaveStates
{
	friend class TEST_CPT_RB_Simulator;
	uint8_t Counter = 0;

public:
	TEST_CPT_RB_SaveStates(uint8_t Counter)
		:ABS_CPT_RB_SaveStates(), Counter(Counter)
	{}

protected:
	// This mock should not trigger trigger rollback even if the inputs don't match
	// The objective here is to always fill the buffer the same way so the checksum is consistent
	void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override
	{
		TargetBufferOut.clear();
		TargetBufferOut.push_back(Counter);
	}

	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override
	{
		if (SourceBuffer.size() == 1)
		{
			Counter = SourceBuffer[0];
		}
	}

	void OnDestroy() override {}
};

class TEST_CPT_RB_Simulator final : public ABS_CPT_RB_Simulator
{
	TEST_CPT_RB_SaveStates& SaveStates;
public:
	TEST_CPT_RB_Simulator(TEST_CPT_RB_SaveStates& SaveStates)
		:SaveStates(SaveStates)
	{}

protected:
	void OnSimulateFrame(const float FrameDurationInSeconds, const std::set<uint8_t>& Inputs) override
	{
		SaveStates.Counter++;
	}

	void OnEndOfLife() override {}
};

void LogSuccess(ABS_RB_Rollbackable::SINGLETON::TickSuccess_E Success, std::string SystemName)
{
	switch (Success)
	{
	case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StallAdvantage:
		std::cout << "^^^^^^^^^^^^" + SystemName + " STALLING^^^^^^^^^^^^" << std::endl << std::endl;
		break;
	case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StarvedForInput:
		std::cout << "^^^^^^^^^^^^" + SystemName + " STARVED^^^^^^^^^^^^" << std::endl << std::endl;
		break;
	case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StayCurrent:
		std::cout << "^^^^^^^^^^^^" + SystemName + " STAY^^^^^^^^^^^^" << std::endl << std::endl;
		break;
	case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::ToNext:
		std::cout << "^^^^^^^^^^^^" + SystemName + " NEXT^^^^^^^^^^^^" << std::endl << std::endl;
		break;
	default:
		std::cout << "^^^^^^^^^^^^" + SystemName + " DEFAULT^^^^^^^^^^^^" << std::endl << std::endl;
		break;
	}
}

bool TestRemoteMockRollback()
{
	DATA_CFG ConfigWithNoFakedDelay;
	ConfigWithNoFakedDelay.FakedMissedPredictionsFramesCount = 0; // Comment out to use the fake rollback
	DATA_CFG::Load(ConfigWithNoFakedDelay);

	const uint16_t LocalStartFrameIndex = 0;
	const uint16_t RemoteStartFrameIndex = 2;
	assert(RemoteStartFrameIndex >= LocalStartFrameIndex);

	const DATA_Player TrueLocalPlayer1{ 0, true, LocalStartFrameIndex, 0 };
	const DATA_Player LocalPlayer2{ TrueLocalPlayer1.Id + 1, false, RemoteStartFrameIndex, TrueLocalPlayer1.SystemIndex };

	const DATA_Player TrueRemotePlayer2{ LocalPlayer2.Id, true, RemoteStartFrameIndex, uint8_t(LocalPlayer2.SystemIndex + 1) };
	const DATA_Player RemotePlayer1{ TrueLocalPlayer1.Id, false, RemoteStartFrameIndex, TrueRemotePlayer2.SystemIndex };

	const uint16_t ReceiveRemoteIntervalInFrames = 3;
	const uint16_t FrameAdvantageInFrames = 4;
	const uint16_t FrameDurationDivider = 2;
	const size_t MockIterationsCount = 60 * FrameDurationDivider;

	TEST_CPT_IPT_Emulator TrueLocalPlayer1Emulator;
	TrueLocalPlayer1Emulator.LocalMockInputs = { {0}, {1}, {2} };
	TEST_CPT_IPT_Emulator LocalPlayer2Emulator;
	TEST_CPT_RB_SaveStates TrueLocalPlayer1SaveStates(LocalStartFrameIndex);
	TEST_CPT_RB_SaveStates LocalPlayer2SaveStates(RemoteStartFrameIndex);
	TEST_CPT_RB_Simulator TrueLocalPlayer1Simulator(TrueLocalPlayer1SaveStates);
	TEST_CPT_RB_Simulator LocalPlayer2Simulator(LocalPlayer2SaveStates);

	TEST_CPT_IPT_Emulator RemotePlayer1Emulator;
	TEST_CPT_IPT_Emulator TrueRemotePlayer2Emulator;
	TrueRemotePlayer2Emulator.LocalMockInputs = { {3}, {2}, {1} };
	TEST_CPT_RB_SaveStates RemotePlayer1SaveStates(RemoteStartFrameIndex);
	TEST_CPT_RB_SaveStates TrueRemotePlayer2SaveStates(RemoteStartFrameIndex);
	TEST_CPT_RB_Simulator RemotePlayer1Simulator(RemotePlayer1SaveStates);
	TEST_CPT_RB_Simulator TrueRemotePlayer2Simulator(TrueRemotePlayer2SaveStates);

	uint16_t MockIterationIndex = LocalStartFrameIndex * FrameDurationDivider;

	TrueLocalPlayer1Emulator.DownloadRemoteInputs = [&](const std::vector<uint8_t>& BinaryPayload)
	{
		if (MockIterationIndex >= RemoteStartFrameIndex * FrameDurationDivider)
		{
			SystemMultiton::GetEmulator(TrueRemotePlayer2.SystemIndex).DownloadRemotePlayerBinary(BinaryPayload.data());
		}
	};

	const uint16_t RemoteStartMockIterationIndex = (RemoteStartFrameIndex + FrameAdvantageInFrames) * FrameDurationDivider;

	TrueRemotePlayer2Emulator.DownloadRemoteInputs = [&](const std::vector<uint8_t>& BinaryPayload)
	{
		if (MockIterationIndex >= RemoteStartMockIterationIndex)
		{
			// The remainder is 0 when starting a new frame and 1 when ending
			if ((MockIterationIndex - RemoteStartMockIterationIndex) % (ReceiveRemoteIntervalInFrames * FrameDurationDivider) == 1)
			{
				SystemMultiton::GetEmulator(TrueLocalPlayer1.SystemIndex).DownloadRemotePlayerBinary(BinaryPayload.data());
			}
		}
	};

	SystemMultiton::GetRollbackable(TrueLocalPlayer1.SystemIndex).SyncWithRemoteFrameIndex(LocalStartFrameIndex);

	TrueLocalPlayer1Emulator.ActivateNow(TrueLocalPlayer1);
	TrueLocalPlayer1SaveStates.ActivateNow(TrueLocalPlayer1);
	TrueLocalPlayer1Simulator.Enable(TrueLocalPlayer1, LocalStartFrameIndex);

	srand(0);

	bool IsRemoteInitialized = false;

	for (; MockIterationIndex < LocalStartFrameIndex * FrameDurationDivider + (uint16_t)MockIterationsCount; MockIterationIndex++)
	{
		if (MockIterationIndex == RemoteStartFrameIndex * FrameDurationDivider && !IsRemoteInitialized)
		{
			IsRemoteInitialized = true;

			LocalPlayer2Emulator.ActivateNow(LocalPlayer2);
			LocalPlayer2SaveStates.ActivateNow(LocalPlayer2);
			LocalPlayer2Simulator.Enable(LocalPlayer2, RemoteStartFrameIndex);

			SystemMultiton::GetRollbackable(TrueRemotePlayer2.SystemIndex).SyncWithRemoteFrameIndex(RemoteStartFrameIndex);

			RemotePlayer1Emulator.ActivateNow(RemotePlayer1);
			TrueRemotePlayer2Emulator.ActivateNow(TrueRemotePlayer2);
			RemotePlayer1SaveStates.ActivateNow(RemotePlayer1);
			TrueRemotePlayer2SaveStates.ActivateNow(TrueRemotePlayer2);
			RemotePlayer1Simulator.Enable(RemotePlayer1, RemoteStartFrameIndex);
			TrueRemotePlayer2Simulator.Enable(TrueRemotePlayer2, RemoteStartFrameIndex);
		}

		auto LocalSuccess = SystemMultiton::GetRollbackable(TrueLocalPlayer1.SystemIndex).TryTickingToNextFrame(DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / FrameDurationDivider);
		LogSuccess(LocalSuccess, "LOCAL");

		if (MockIterationIndex >= RemoteStartMockIterationIndex)
		{
			auto RemoteSuccess = SystemMultiton::GetRollbackable(TrueRemotePlayer2.SystemIndex).TryTickingToNextFrame(DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / FrameDurationDivider);
			LogSuccess(RemoteSuccess, "REMOTE");
		}

		//TrueLocalPlayer1Emulator.LocalMockInputs = { {(uint8_t)(rand() % CPT_IPT_TogglesPayload::MaxInputToken())} };
		//TrueRemotePlayer2Emulator.LocalMockInputs = { {(uint8_t)(rand() % CPT_IPT_TogglesPayload::MaxInputToken())} };
	}

	return true;
}
