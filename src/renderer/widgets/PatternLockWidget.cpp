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
    std::cout << "[PatternLockWidget] Constructor called!" << std::endl;
}

void PatternLockWidget::registerSelf(const ASP<PatternLockWidget>& self) {
    m_self = self;
}

void PatternLockWidget::configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) {
    std::cout << "[PatternLockWidget] configure called!" << std::endl;
    std::cout << "[PatternLockWidget] configure: props size: " << props.size() << std::endl;
    
    viewport = pOutput ? pOutput->getViewport() : Vector2D{300, 300};
    // Set position to (0,0) - bottom-left corner
    position = {0, 0};
    
    // Parse pattern from config
    if (props.contains("pattern")) {
        std::cout << "[PatternLockWidget] configure: found pattern property" << std::endl;
        try {
            std::string patternStr = std::any_cast<Hyprlang::STRING>(props.at("pattern"));
            std::cout << "[PatternLockWidget] configure: pattern string: '" << patternStr << "'" << std::endl;
            m_configuredPattern = parsePatternString(patternStr);
            std::cout << "[PatternLockWidget] configure: pattern='" << patternStr << "' parsed to ";
            for (int idx : m_configuredPattern) {
                std::cout << idx << " ";
            }
            std::cout << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[PatternLockWidget] configure: failed to parse pattern: " << e.what() << std::endl;
            m_configuredPattern.clear();
        }
    } else {
        std::cout << "[PatternLockWidget] configure: no pattern property found" << std::endl;
    }
    
    std::cout << "[PatternLockWidget] configure: position set to (0,0)" << std::endl;
    std::cout << "[PatternLockWidget] configure: final configured pattern size: " << m_configuredPattern.size() << std::endl;
}

bool PatternLockWidget::draw(const SRenderData& data) {
    Vector2D size = {300, 300};
    // Position grid at (0,0) - bottom-left corner
    Vector2D pos = {0, 0};
    double margin = 40;
    double dotRadius = 15;
    double cellW = (size.x - 2 * margin) / (GRID_SIZE - 1);
    double cellH = (size.y - 2 * margin) / (GRID_SIZE - 1);
    int rounding = dotRadius; // full rounding for circle

    // Draw dots, coloring touched ones blue
    // Grid layout: 6 7 8  (corresponds to 7 8 9)
    //              3 4 5  (corresponds to 4 5 6)
    //              0 1 2  (corresponds to 1 2 3)
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            int idx = y * GRID_SIZE + x;
            double cx = pos.x + margin + x * cellW - dotRadius;
            // Top-left coordinate system: y=0 is top row, y=GRID_SIZE-1 is bottom row
            double cy = pos.y + margin + y * cellH - dotRadius; 
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
    Vector2D gridPos = {0, 0}; // Grid positioned at (0,0) - bottom-left of screen
    double margin = 40;
    
    // Convert from screen coordinates to widget coordinates
    double widgetX = pos.x - gridPos.x;
    double widgetY = viewport.y - pos.y - gridPos.y; // Flip Y coordinate
    
    double gridX = margin - 15;
    double gridY = margin - 15;
    double gridW = size.x - 2 * margin + 30;
    double gridH = size.y - 2 * margin + 30;
    
    bool inside = (widgetX >= gridX && widgetX <= gridX + gridW && widgetY >= gridY && widgetY <= gridY + gridH);
    return inside;
}

void PatternLockWidget::onClick(uint32_t button, bool down, const Hyprutils::Math::Vector2D& pos) {
    std::cout << "[PatternLockWidget] onClick: button=" << button << ", down=" << down << ", pos=(" << pos.x << "," << pos.y << ")" << std::endl;
    
    int idx = pointToDotIndex(pos);
    std::cout << "[PatternLockWidget] onClick: pointToDotIndex returned: " << idx << std::endl;
    
    if (down) {
        std::cout << "[PatternLockWidget] onClick: touch down, isTouchActive=" << isTouchActive << std::endl;
        if (!isTouchActive) {
            isTouchActive = true;
            m_patternPath.clear();
            if (idx >= 0) {
                m_patternPath.push_back(idx);
                std::cout << "[PatternLockWidget] Added first dot: " << idx << std::endl;
            }
        } else {
            if (idx >= 0 && std::find(m_patternPath.begin(), m_patternPath.end(), idx) == m_patternPath.end()) {
                m_patternPath.push_back(idx);
                std::cout << "[PatternLockWidget] Added dot: " << idx << std::endl;
            }
        }
        
    } else {
        std::cout << "[PatternLockWidget] onClick: touch up, isTouchActive=" << isTouchActive << std::endl;
        isTouchActive = false;
        
        // Check pattern when touch is released
        if (!m_patternPath.empty()) {
            std::cout << "[PatternLockWidget] Final pattern: ";
            for (int dot : m_patternPath) {
                std::cout << dot << " ";
            }
            std::cout << std::endl;
            
            if (checkPatternMatch()) {
                std::cout << "[PatternLockWidget] Pattern correct, unlocking!" << std::endl;
                g_pAuth->enqueueUnlock();
            } else {
                std::cout << "[PatternLockWidget] Pattern incorrect!" << std::endl;
            }
        } else {
            std::cout << "[PatternLockWidget] onClick: pattern path is empty!" << std::endl;
        }
        
        m_patternPath.clear();
    }
    g_pHyprlock->renderAllOutputs();
}

int PatternLockWidget::pointToDotIndex(const Hyprutils::Math::Vector2D& pt) const {
    Vector2D size = {300, 300};
    Vector2D gridPos = {0, 0}; // Grid positioned at (0,0) - bottom-left of screen
    double margin = 40;
    double dotRadius = 15;
    double cellW = (size.x - 2 * margin) / (GRID_SIZE - 1);
    double cellH = (size.y - 2 * margin) / (GRID_SIZE - 1);
    
    // Convert from screen coordinates (top-left origin) to widget coordinates (bottom-left origin)
    // pt is in screen coordinates, we need to convert to widget coordinates
    double widgetX = pt.x - gridPos.x;
    double widgetY = viewport.y - pt.y - gridPos.y; // Flip Y coordinate
    
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            double cx = margin + x * cellW;
            double cy = margin + y * cellH;
            double dx = widgetX - cx;
            double dy = widgetY - cy;
            double dist2 = dx * dx + dy * dy;
            if (dist2 <= dotRadius * dotRadius * 2) {
                int idx = y * GRID_SIZE + x;
                return idx; // Grid: 6 7 8 / 3 4 5 / 0 1 2
            }
        }
    }
    return -1;
}

std::vector<int> PatternLockWidget::parsePatternString(const std::string& patternStr) const {
    std::vector<int> pattern;
    
    std::cout << "[PatternLockWidget] parsePatternString: parsing '" << patternStr << "'" << std::endl;
    
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
            std::cout << "[PatternLockWidget] parsePatternString: char '" << c << "' -> dot index " << dotIndex << std::endl;
        } else {
            std::cout << "[PatternLockWidget] parsePatternString: ignoring invalid char '" << c << "'" << std::endl;
        }
    }
    
    std::cout << "[PatternLockWidget] parsePatternString: final pattern: ";
    for (int dot : pattern) {
        std::cout << dot << " ";
    }
    std::cout << std::endl;
    
    return pattern;
}

bool PatternLockWidget::checkPatternMatch() const {
    std::cout << "[PatternLockWidget] checkPatternMatch: configured pattern size: " << m_configuredPattern.size() << std::endl;
    std::cout << "[PatternLockWidget] checkPatternMatch: drawn pattern size: " << m_patternPath.size() << std::endl;
    
    if (m_configuredPattern.empty()) {
        std::cout << "[PatternLockWidget] checkPatternMatch: no pattern configured, allowing any pattern" << std::endl;
        return true; // No pattern configured, allow any pattern
    }
    
    if (m_patternPath.size() != m_configuredPattern.size()) {
        std::cout << "[PatternLockWidget] checkPatternMatch: size mismatch!" << std::endl;
        return false;
    }
    
    std::cout << "[PatternLockWidget] checkPatternMatch: comparing patterns:" << std::endl;
    std::cout << "[PatternLockWidget] checkPatternMatch: configured: ";
    for (int dot : m_configuredPattern) {
        std::cout << dot << " ";
    }
    std::cout << std::endl;
    std::cout << "[PatternLockWidget] checkPatternMatch: drawn: ";
    for (int dot : m_patternPath) {
        std::cout << dot << " ";
    }
    std::cout << std::endl;
    
    for (size_t i = 0; i < m_patternPath.size(); ++i) {
        if (m_patternPath[i] != m_configuredPattern[i]) {
            std::cout << "[PatternLockWidget] checkPatternMatch: mismatch at position " << i << ": configured=" << m_configuredPattern[i] << ", drawn=" << m_patternPath[i] << std::endl;
            return false;
        }
    }
    
    std::cout << "[PatternLockWidget] checkPatternMatch: patterns match!" << std::endl;
    return true;
}