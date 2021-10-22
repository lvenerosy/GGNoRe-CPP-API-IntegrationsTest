/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <Core/Data/DATA_CFG.hpp>
#include <Rollback/ABS_CPT_RB_SaveStates.hpp>

class TEST_CPT_RB_SaveStates : public GGNoRe::API::ABS_CPT_RB_SaveStates
{
protected:
	void OnSerialize(std::vector<uint8_t>& TargetBuffer) {}
	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) {}
};

int main()
{
	GGNoRe::API::DATA_CFG::Get().LogHumanReadable("LINKER TEST");

	TEST_CPT_RB_SaveStates SaveStates;
	SaveStates.Serialize(1);
	SaveStates.Serialize(2);
	SaveStates.Deserialize(1);

	return 0;
}