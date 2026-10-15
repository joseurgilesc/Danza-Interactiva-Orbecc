#include "ui/ParamPanel.h"

#include "mapping/ParameterTypes.h"
#include "imgui_internal.h"
#include "ui/PanelIds.h"

namespace modular {

ParamPanel::ParamPanel(SceneEngine* sceneEngine, ParameterRegistry* registry)
    : sceneEngine_(sceneEngine)
    , registry_(registry)
    , visible_(true)
    , showGlobal_(true)
    , showScene_(true) {}

void ParamPanel::setVisible(bool visible) {
    visible_ = visible;
}

bool ParamPanel::isVisible() const {
    return visible_;
}

void ParamPanel::draw() {
    if (!visible_) {
        return;
    }

    if (!ImGui::Begin(panel_ids::kParameters, &visible_)) {
        ImGui::End();
        return;
    }

    ScenePtr activeScene = sceneEngine_ ? sceneEngine_->getActiveScene() : nullptr;

    ImGui::Checkbox("Global", &showGlobal_);
    ImGui::SameLine();
    ImGui::Checkbox("Escena", &showScene_);
    ImGui::SameLine();
    filter_.Draw("Buscar", 180.0f);

    // Rebuild rows every frame to get current values
    std::vector<ParameterRow> rows;
    rebuildRows(rows);

    if (rows.empty()) {
        ImGui::TextDisabled("No hay parámetros disponibles para mostrar.");
    } else if (ImGui::BeginChild("ParamPanel.Scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rows.size()));
        while (clipper.Step()) {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                drawRow(rows[index], activeScene);
            }
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    
    if (ImGui::Button("Save Parameters")) {
        std::string filepath = "config/parameters.json";
        if (registry_ && sceneEngine_) {
            ofLogNotice("ParamPanel") << "Saving parameters from all scenes...";
            auto loadedScenes = sceneEngine_->getLoadedScenes();
            int totalParams = 0;
            
            for (const auto& sceneId : loadedScenes) {
                auto scene = sceneEngine_->getScene(sceneId);
                if (!scene) {
                    ofLogWarning("ParamPanel") << "Scene not found: " << sceneId;
                    continue;
                }
                
                auto paramIds = scene->getParameterIds();
                totalParams += paramIds.size();
                
                for (const auto& localId : paramIds) {
                    auto param = scene->getParameter(localId);
                    if (!param) continue;
                    
                    std::string fullParamId = sceneId + "." + localId;
                    registry_->setValue(fullParamId, param->currentValue);
                }
            }
            
            if (registry_->saveToJson(filepath)) {
                ofLogNotice("ParamPanel") << "Saved " << totalParams << " parameters to: " << filepath;
            } else {
                ofLogError("ParamPanel") << "Failed to save parameters to: " << filepath;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Parameters")) {
        std::string filepath = "config/parameters.json";
        if (registry_) {
            if (registry_->loadFromJson(filepath)) {
                ofLogNotice("ParamPanel") << "Parameters loaded from: " << filepath;
                
                // Sync loaded values to ALL scenes using direct lookup (no fragile matching)
                if (sceneEngine_) {
                    auto params = registry_->getAllParams();
                    int synced = 0;
                    
                    for (const auto& param : params) {
                        if (param.scope == ParameterScope::SCENE) {
                            size_t dotPos = param.id.find('.');
                            if (dotPos != std::string::npos) {
                                std::string sceneId = param.id.substr(0, dotPos);
                                std::string localParamId = param.id.substr(dotPos + 1);
                                
                                auto scene = sceneEngine_->getScene(sceneId);
                                if (scene) {
                                    scene->setParameter(localParamId, param.currentValue);
                                    synced++;
                                } else {
                                    ofLogWarning("ParamPanel") << "Scene not found for param: " << param.id;
                                }
                            }
                        }
                    }
                    
                    ofLogNotice("ParamPanel") << "Synced " << synced << " parameters to scenes";
                }
            } else {
                ofLogWarning("ParamPanel") << "No parameters file found at: " << filepath;
            }
        }
    }

    ImGui::End();
}

void ParamPanel::rebuildRows(std::vector<ParameterRow>& rows) {
    rows.clear();
    if (showGlobal_ && registry_) {
        auto globals = registry_->getParamsByScope(ParameterScope::GLOBAL);
        for (auto& param : globals) {
            if (!filter_.PassFilter(param.label.c_str()) && !filter_.PassFilter(param.id.c_str())) {
                continue;
            }
            // Get fresh param from registry (with current value)
            auto optParam = registry_->getParam(param.id);
            if (optParam) {
                rows.push_back(ParameterRow{*optParam, true, "Global"});
            }
        }
    }

    if (showScene_ && sceneEngine_) {
        ScenePtr activeScene = sceneEngine_->getActiveScene();
        if (activeScene) {
            // Don't sync here - values come from applyChange already
            auto ids = activeScene->getParameterIds();
            for (const auto& id : ids) {
                auto parameter = activeScene->getParameter(id);
                if (!parameter) {
                    continue;
                }
                if (!filter_.PassFilter(parameter->label.c_str()) && !filter_.PassFilter(parameter->id.c_str())) {
                    continue;
                }
                // Clone the parameter so we can update the copy's value
                rows.push_back(ParameterRow{*parameter, false, activeScene->getName()});
            }
        }
    }
}

void ParamPanel::drawRow(ParameterRow& row, ScenePtr activeScene) {
    ImGui::PushID(row.parameter.id.c_str());
    ImGui::Separator();

    ImGui::TextUnformatted(row.parameter.label.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]", row.isGlobal ? row.owner.c_str() : row.owner.c_str());

    if (!row.parameter.description.empty()) {
        ImGui::TextDisabled("%s", row.parameter.description.c_str());
    }

    bool handled = false;
    switch (row.parameter.type) {
        case ParameterType::BOOL: {
            bool value = row.parameter.currentValue > 0.5f;
            if (ImGui::Checkbox("##bool", &value)) {
                applyChange(row, value ? 1.0f : 0.0f, activeScene);
            }
            handled = true;
            break;
        }
        case ParameterType::FLOAT:
        case ParameterType::INT: {
            float value = row.parameter.currentValue;
            float speed = std::max(0.001f, row.parameter.step);
            if (ImGui::DragFloat("##scalar", &value, speed, row.parameter.min, row.parameter.max, "%.3f")) {
                applyChange(row, value, activeScene);
            }
            handled = true;
            break;
        }
        default:
            break;
    }

    if (!handled) {
        ImGui::TextDisabled("Tipo no soportado en este panel.");
    }

    ImGui::PopID();
}

void ParamPanel::applyChange(const ParameterRow& row, float value, ScenePtr activeScene) {
    if (row.isGlobal) {
        if (registry_) {
            registry_->setValue(row.parameter.id, value);
        }
        // Background color needs immediate visual update via SceneEngine
        if (row.parameter.id == "global.backgroundColor" && sceneEngine_) {
            sceneEngine_->setBackgroundColor(value);
        }
    } else if (activeScene) {
        // Update scene first
        activeScene->setParameter(row.parameter.id, value);
        
        // Update registry with full ID
        if (registry_) {
            std::string sceneId = activeScene->getId();
            std::string fullParamId = sceneId + "." + row.parameter.id;
            registry_->setValue(fullParamId, value);
        }
    }
}

} // namespace modular
