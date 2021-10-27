/*
 * Copyright 2021 Lo�c Venerosy
 */

#pragma once

#include <Core/Data/DATA_CFG.hpp>
#include <Input/ABS_CPT_IPT_Emulator.hpp>
#include <Rollback/ABS_CPT_RB_SaveStates.hpp>
#include <Rollback/ABS_CPT_RB_Simulator.hpp>

#include <assert.h>

class TEST_CPT_IPT_Emulator final : public GGNoRe::API::ABS_CPT_IPT_Emulator
{
public:
	TEST_CPT_IPT_Emulator(const uint16_t OwningPlayerIndex)
		:ABS_CPT_IPT_Emulator(OwningPlayerIndex)
	{}

protected:
	void OnReadyToSendInputs(const std::vector<uint8_t>& BinaryPayload) const override {}
};

class TEST_CPT_RB_SaveStates final : public GGNoRe::API::ABS_CPT_RB_SaveStates
{
public:
	TEST_CPT_RB_SaveStates(const uint16_t OwningPlayerIndex)
		:GGNoRe::API::ABS_CPT_RB_SaveStates(OwningPlayerIndex)
	{}

protected:
	void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override {}
	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override {}
};

class TEST_CPT_RB_Simulator final : public GGNoRe::API::ABS_CPT_RB_Simulator
{
public:
	TEST_CPT_RB_Simulator(const uint16_t OwningPlayerIndex)
		:GGNoRe::API::ABS_CPT_RB_Simulator(OwningPlayerIndex)
	{}

protected:
	void OnSimulateFrame(const float FrameDurationInSeconds, const std::set<uint8_t>& Inputs) override {}
	void OnEndOfLife() override {}
};

int main()
{
	GGNoRe::API::DATA_CFG ConfigWithNoFakedDelay;
	ConfigWithNoFakedDelay.FakedMissedPredictionsFramesCount = 0;
	GGNoRe::API::DATA_CFG::Load(ConfigWithNoFakedDelay);

	const uint16_t LocalPlayerIndex = 0;
	const uint16_t RemotePlayerIndex = LocalPlayerIndex + 1;

	TEST_CPT_IPT_Emulator LocalPlayerEmulator(LocalPlayerIndex);
	TEST_CPT_IPT_Emulator RemotePlayerEmulator(RemotePlayerIndex);

	TEST_CPT_RB_SaveStates LocalPlayerSaveStates(LocalPlayerIndex);
	TEST_CPT_RB_SaveStates RemotePlayerSaveStates(RemotePlayerIndex);

	TEST_CPT_RB_Simulator LocalPlayerSimulator(LocalPlayerIndex);
	TEST_CPT_RB_Simulator RemotePlayerSimulator(RemotePlayerIndex);

	std::vector<uint8_t> LocalPlayerMockInputs{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

	return 0;
}