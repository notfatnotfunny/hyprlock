#pragma once
#include "IWidget.hpp"
#include <vector>
#include <utility>
#include <cairomm/context.h>
#include "../../core/LockSurface.hpp"
#include <string>
#include <unordered_map>
#include <any>

class PatternLockWidget : public IWidget {
public:
    PatternLockWidget();
    ~PatternLockWidget() = default;

    void registerSelf(const ASP<PatternLockWidget>& self);
    void configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) override;
    bool draw(const SRenderData& data) override;

    void createGrid();
    void clearPatternPath();

    bool containsPoint(const Hyprutils::Math::Vector2D& pos) const override;
    void onClick(uint32_t button, bool down, const Hyprutils::Math::Vector2D& pos);

private:
    bool isTouchActive = false;
    std::vector<int> m_patternPath;
    std::vector<int> m_configuredPattern;
    static constexpr int GRID_SIZE = 3;
    Vector2D viewport = {1920, 1080};
    std::vector<std::vector<Vector2D>> GRID;
    std::vector<std::vector<bool>> grid;
    Vector2D size = {300,300};
    double dotRadius = 15;
    Vector2D position = {0, 0}; 
    std::string halign = "center";
    std::string valign = "center";
    int zindex = 0;
    AWP<PatternLockWidget> m_self;
    Vector2D pointToDotIndex(const Hyprutils::Math::Vector2D& pt) const;
    std::vector<int> parsePatternString(const std::string& patternStr) const;
    bool checkPatternMatch() const;
}; 
