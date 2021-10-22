/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <Core/Data/DATA_CFG.hpp>
#include <Rollback/ABS_CPT_RB_SaveStates.hpp>
#include <Rollback/ABS_CPT_RB_Simulator.hpp>

class TEST_CPT_RB_SaveStates : public GGNoRe::API::ABS_CPT_RB_SaveStates
{
protected:
	void OnSerialize(std::vector<uint8_t>& TargetBuffer) override {}
	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override {}
};

class TEST_CPT_RB_Simulator : public GGNoRe::API::ABS_CPT_RB_Simulator
{
public:
	TEST_CPT_RB_Simulator(const uint32_t SpawnFrameIndex)
		:ABS_CPT_RB_Simulator(SpawnFrameIndex)
	{
	}

protected:
	virtual void OnSimulateFrame(const float FrameDurationInSeconds) override {}
	virtual void OnEndOfLife() override {}
};

int main()
{
	GGNoRe::API::DATA_CFG::Get().LogHumanReadable("LINKER TEST");

	TEST_CPT_RB_SaveStates SaveStates;
	TEST_CPT_RB_Simulator Simulator(0);

	Simulator.SimulateFrame(0);
	SaveStates.Serialize(0);
	Simulator.SimulateFrame(1);
	SaveStates.Serialize(1);
	SaveStates.Deserialize(0);
	Simulator.SimulateFrame(0);

	return 0;
}