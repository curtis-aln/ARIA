#pragma once
#include "tabs/i_tab.h"
#include "tabs/tagged_tab.h"
#include <vector>
#include <memory>
#include "../context/sim_snapshot.h"


class ControlPanel
{
public:
    ControlPanel();

    void draw(const SimSnapshot& snap, ImGuiContext& ctx, float dt);

    TaggedTab* get_tagged_tab() { return m_tagged_tab_; }

private:
    std::vector<std::unique_ptr<ITab>> m_tabs_;
    TaggedTab* m_tagged_tab_ = nullptr; // non-owning view into m_tabs_
};