#include "IDemo.h"
#include "imgui/imgui.h"
#include <iterator>

namespace raphael
{
    namespace
    {
        struct DemoEntry
        {
            DemoType type;
            const char* name;
        };

        // Demos that own an ImGui pass and can therefore host this dropdown.
        constexpr DemoEntry kSwitchableDemos[] = {
            { DemoType::Box,         "Box" },
            { DemoType::Quad,        "Quad" },
            { DemoType::TexturedBox, "Textured Box" },
            { DemoType::Gltf,        "glTF" },
            { DemoType::GBuffer,     "GBuffer (Deferred)" },
            { DemoType::MultiObject, "Multi Object" },
        };
    }

    std::optional<DemoType> IDemo::TakeSwitchRequest()
    {
        std::optional<DemoType> request = m_switchRequest;
        m_switchRequest.reset();
        return request;
    }

    void IDemo::DrawDemoSwitcher()
    {
        ImGui::Begin("Demos");

        int currentIndex = -1;
        for (int i = 0; i < std::size(kSwitchableDemos); ++i)
        {
            if (kSwitchableDemos[i].type == m_demoType)
            {
                currentIndex = i;
                break;
            }
        }

        const char* preview = (currentIndex >= 0) ? kSwitchableDemos[currentIndex].name : "(current)";
        if (ImGui::BeginCombo("Active demo", preview))
        {
            for (int i = 0; i < std::size(kSwitchableDemos); ++i)
            {
                const bool isSelected = (i == currentIndex);
                if (ImGui::Selectable(kSwitchableDemos[i].name, isSelected) &&
                    kSwitchableDemos[i].type != m_demoType)
                {
                    m_switchRequest = kSwitchableDemos[i].type;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::End();
    }
}
