#pragma once
#include "i_tab.h"
#include "context/sim_command.h"
#include "context/sim_snapshot.h"

class SimulationTab : public ITab
{
public:
    const char* label() const override { return "Simulation"; }
    void        draw(const SimSnapshot& snap, ImGuiContext& ctx)   override;

private:
    enum class FFCondition { Duration, Population, Generation };
    FFCondition m_ff_cond_ = FFCondition::Duration;
    float       m_ff_target_ = 300.f;
    bool        m_fast_fwd_ = false;
    float       m_speed_ = 1.f;
};
