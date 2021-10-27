/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <Core/Data/DATA_CFG.hpp>
#include <Input/ABS_CPT_IPT_Emulator.hpp>
#include <Rollback/ABS_CPT_RB_SaveStates.hpp>
#include <Rollback/ABS_CPT_RB_Simulator.hpp>

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

class CPT_IPT_Emulator final : public GGNoRe::API::ABS_CPT_IPT_Emulator
{
public:
	CPT_IPT_Emulator(const uint16_t OwningPlayerIndex)
		:ABS_CPT_IPT_Emulator(OwningPlayerIndex)
	{}

protected:
	void OnReadyToSendInputs(const std::vector<uint8_t>& BinaryPayload) const override {}
};

class IMPL_TEST_WhiteBox final : public GGNoRe::API::ABS_TEST_WhiteBox
{
protected:
	bool OnRunTest() override
	{

	}
};

int main()
{
	GGNoRe::API::DATA_CFG::Get().LogHumanReadable("INTEGRATION TEST");

	

	return 0;
}