#include "PatternLockWidget.hpp"
#include "../../config/ConfigDataValues.hpp"
#include "../../core/Output.hpp"
#include "../Renderer.hpp"
#include "../../auth/Auth.hpp"
#include <cmath>
#include <iostream>
#include "../../core/hyprlock.hpp"
#include <hyprlang.hpp>

PatternLockWidget::PatternLockWidget() {
}

void PatternLockWidget::registerSelf(const ASP<PatternLockWidget>& self) {
    m_self = self;
}

void PatternLockWidget::configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) {
    viewport = pOutput ? pOutput->getViewport() : Vector2D{300, 300};
    // Parse position from config using CLayoutValueData
    if (props.contains("position")) {
        try {
            position = CLayoutValueData::fromAnyPv(props.at("position"))->getAbsolute(viewport);
        } catch (...) { position = {0, 0}; }
    } else {
        position = {0, 0};
    }
    if (props.contains("halign")) {
        try { halign = std::any_cast<Hyprlang::STRING>(props.at("halign")); } catch (...) { halign = "center"; }
    } else { halign = "center"; }
    if (props.contains("valign")) {
        try { valign = std::any_cast<Hyprlang::STRING>(props.at("valign")); } catch (...) { valign = "center"; }
    } else { valign = "center"; }
    if (props.contains("zindex")) {
        try { zindex = std::any_cast<Hyprlang::INT>(props.at("zindex")); } catch (...) { zindex = 0; }
    } else { zindex = 0; }
    // Parse pattern from config
    if (props.contains("pattern")) {
        try {
            std::string patternStr = std::any_cast<Hyprlang::STRING>(props.at("pattern"));
            m_configuredPattern = parsePatternString(patternStr);
        } catch (const std::exception& e) {
            m_configuredPattern.clear();
        }
    }
}

// Helper to compute anchor point based on alignment and position
Vector2D computeAnchor(const Vector2D& viewport, const Vector2D& gridSize, const Vector2D& offset, const std::string& halign, const std::string& valign) {
    double x = 0, y = 0;
    // Horizontal alignment
    if (halign == "left") x = 0;
    else if (halign == "center") x = (viewport.x - gridSize.x) / 2.0;
    else if (halign == "right") x = viewport.x - gridSize.x;
    // Vertical alignment (remember: (0,0) is bottom-left, (0,height) is top-left)
    if (valign == "bottom") y = 0;
    else if (valign == "center") y = (viewport.y - gridSize.y) / 2.0;
    else if (valign == "top") y = viewport.y - gridSize.y;
    // Apply offset
    return {x + offset.x, y + offset.y};
}

bool PatternLockWidget::draw(const SRenderData& data) {
    Vector2D size = {300, 300};
    double margin = 40;
    double dotRadius = 15;
    double cellW = (size.x - 2 * margin) / (GRID_SIZE - 1);
    double cellH = (size.y - 2 * margin) / (GRID_SIZE - 1);
    int rounding = dotRadius; // full rounding for circle
    // Compute anchor for grid
    Vector2D anchor = computeAnchor(viewport, size, position, halign, valign);
    // Draw dots, coloring touched ones blue
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            int idx = y * GRID_SIZE + x;
            double cx = anchor.x + margin + x * cellW - dotRadius;
            double cy = anchor.y + margin + y * cellH - dotRadius;
            bool selected = std::find(m_patternPath.begin(), m_patternPath.end(), idx) != m_patternPath.end();
            CHyprColor dotColor = selected ? CHyprColor(0.2, 0.5, 1.0, data.opacity) : CHyprColor(0.7, 0.7, 0.7, data.opacity);
            g_pRenderer->renderRect(CBox{cx, cy, dotRadius * 2, dotRadius * 2}, dotColor, rounding);
        }
    }
    return false;
}

void PatternLockWidget::setPatternPath(const std::vector<int>& path) {
    m_patternPath = path;
}

void PatternLockWidget::clearPatternPath() {
    m_patternPath.clear();
}

void PatternLockWidget::drawGrid(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Unused in new draw implementation
}

void PatternLockWidget::drawPattern(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Unused in new draw implementation
}

bool PatternLockWidget::containsPoint(const Hyprutils::Math::Vector2D& pos) const {
    Vector2D size = {300, 300};
    double margin = 40;
    // Compute anchor for grid
    Vector2D anchor = computeAnchor(viewport, size, position, halign, valign);
    // Convert from screen coordinates to widget coordinates
    double widgetX = pos.x - anchor.x;
    double widgetY = viewport.y - pos.y - anchor.y; // Flip Y coordinate
    double gridX = margin - 15;
    double gridY = margin - 15;
    double gridW = size.x - 2 * margin + 30;
    double gridH = size.y - 2 * margin + 30;
    bool inside = (widgetX >= gridX && widgetX <= gridX + gridW && widgetY >= gridY && widgetY <= gridY + gridH);
    return inside;
}

void PatternLockWidget::onClick(uint32_t button, bool down, const Hyprutils::Math::Vector2D& pos) {
    int idx = pointToDotIndex(pos);
    
    if (down) {
        if (!isTouchActive) {
            isTouchActive = true;
            m_patternPath.clear();
            if (idx >= 0) {
                m_patternPath.push_back(idx);
            }
        } else {
            if (idx >= 0 && std::find(m_patternPath.begin(), m_patternPath.end(), idx) == m_patternPath.end()) {
                m_patternPath.push_back(idx);
            }
        }
        
    } else {
        isTouchActive = false;
        
        // Check pattern when touch is released
        if (!m_patternPath.empty()) {
            if (checkPatternMatch()) {
                g_pAuth->enqueueUnlock();
            }
        }
        
        // Always clear the pattern path and reset visual state on touch up
        m_patternPath.clear();
    }
    g_pHyprlock->renderAllOutputs();
}

int PatternLockWidget::pointToDotIndex(const Hyprutils::Math::Vector2D& pt) const {
    Vector2D size = {300, 300};
    double margin = 40;
    double dotRadius = 15;
    double cellW = (size.x - 2 * margin) / (GRID_SIZE - 1);
    double cellH = (size.y - 2 * margin) / (GRID_SIZE - 1);
    // Compute anchor for grid
    Vector2D anchor = computeAnchor(viewport, size, position, halign, valign);
    // Convert from screen coordinates (top-left origin) to widget coordinates (bottom-left origin)
    double widgetX = pt.x - anchor.x;
    double widgetY = viewport.y - pt.y - anchor.y; // Flip Y coordinate
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            double cx = margin + x * cellW;
            double cy = margin + y * cellH;
            double dx = widgetX - cx;
            double dy = widgetY - cy;
            double dist2 = dx * dx + dy * dy;
            if (dist2 <= dotRadius * dotRadius * 2) {
                int idx = y * GRID_SIZE + x;
                return idx;
            }
        }
    }
    return -1;
}

std::vector<int> PatternLockWidget::parsePatternString(const std::string& patternStr) const {
    std::vector<int> pattern;
    
    // Convert string like "789632145" to dot indices
    // Grid layout: 6 7 8  (corresponds to 7 8 9)
    //              3 4 5  (corresponds to 4 5 6)
    //              0 1 2  (corresponds to 1 2 3)
    for (char c : patternStr) {
        if (c >= '1' && c <= '9') {
            // Convert '1'-'9' to 0-8 (subtract 1)
            // So '1' -> 0, '2' -> 1, '3' -> 2, etc.
            int dotIndex = c - '1';
            pattern.push_back(dotIndex);
        }
    }
    
    return pattern;
}

bool PatternLockWidget::checkPatternMatch() const {
    if (m_configuredPattern.empty()) {
        return true; // No pattern configured, allow any pattern
    }
    
    if (m_patternPath.size() != m_configuredPattern.size()) {
        return false;
    }
    
    for (size_t i = 0; i < m_patternPath.size(); ++i) {
        if (m_patternPath[i] != m_configuredPattern[i]) {
            return false;
        }
    }
    
    return true;
}