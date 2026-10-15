#pragma once

#include "scenes/BaseScene.h"
#include "engine/ISceneActivator.h"
#include "registry/ParameterRegistry.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>

namespace modular {

enum class TransitionState {
    NONE,
    FADING_OUT,
    FADING_IN
};

// ============================================================================
// SceneEngine - Gestor del ciclo de vida de las escenas
// ============================================================================

/**
 * @brief Clase responsable de gestionar el ciclo de vida de las escenas
 * 
 * SceneEngine implementa ISceneActivator para cumplir el contrato del sistema
 * y proporciona una API unificada para:
 * - Cargar/descargar escenas
 * - Activar/desactivar escenas
 * - Preparar escenas para modo hybrid
 * - Gestionar el update y render de la escena activa
 */
class SceneEngine : public ISceneActivator {
public:
    SceneEngine();
    ~SceneEngine() override;

    // ============================================
    // Scene Management - Carga/Descarga
    // ============================================
    
    /**
     * @brief Carga una escena en memoria
     * @param sceneId Identificador único de la escena
     * @param scene Puntero a la implementación de IBaseScene
     * @return true si la carga fue exitosa
     */
    bool loadScene(const std::string& sceneId, ScenePtr scene);
    
    /**
     * @brief Descarga una escena de memoria
     * @param sceneId Identificador de la escena a descargar
     * @return true si la descarga fue exitosa
     */
    bool unloadScene(const std::string& sceneId);
    
    /**
     * @brief Verifica si una escena está cargada
     * @param sceneId Identificador de la escena
     * @return true si la escena está cargada
     */
    bool isSceneLoaded(const std::string& sceneId) const;

    // ============================================
    // ISceneActivator Implementation
    // ============================================
    
    /**
     * @brief Activa una escena (desactiva la anterior)
     * @param sceneId ID de la escena a activar
     * @return true si la activación fue exitosa
     */
    bool activateScene(const std::string& sceneId) override;
    
    /**
     * @brief Desactiva una escena
     * @param sceneId ID de la escena a desactivar
     * @return true si la desactivación fue exitosa
     */
    bool deactivateScene(const std::string& sceneId) override;
    
    /**
     * @brief Prepara una escena para activación rápida (modo hybrid)
     * @param sceneId ID de la escena a preparar
     * @return true si la preparación fue exitosa
     */
    bool prepareScene(const std::string& sceneId) override;
    
    /**
     * @brief Obtiene el ID de la escena activa
     * @return ID de la escena activa, o string vacío
     */
    std::string getActiveSceneId() const override;
    
    /**
     * @brief Desactiva todas las escenas
     * @return true si todas las desactivaciones fueron exitosas
     */
    bool deactivateAll() override;

    // ============================================
    // Scene Lifecycle - Pausa/Reanudación
    // ============================================
    
    /**
     * @brief Pausa la escena activa
     */
    void pauseScene();
    
    /**
     * @brief Reanuda la escena pausada
     */
    void resumeScene();
    
    /**
     * @brief Verifica si hay una escena activa
     * @return true si hay una escena activa
     */
    bool hasActiveScene() const;
    
    /**
     * @brief Verifica si la escena activa está pausada
     * @return true si está pausada
     */
    bool isScenePaused() const;

    // ============================================
    // Update & Draw - Rendering
    // ============================================
    
    /**
     * @brief Update de la escena activa
     * @param deltaTime Tiempo transcurrido en segundos
     */
    void update(float deltaTime);
    
    /**
     * @brief Render de la escena activa
     */
    void draw();

    // ============================================
    // Scene Access - Consultas
    // ============================================
    
    /**
     * @brief Obtiene referencia a la escena activa
     * @return Puntero a la escena activa, o nullptr
     */
    ScenePtr getActiveScene();
    
    /**
     * @brief Obtiene cualquier escena cargada
     * @param sceneId ID de la escena
     * @return Puntero a la escena, o nullptr si no existe
     */
    ScenePtr getScene(const std::string& sceneId);
    
    /**
     * @brief Obtiene lista de escenas disponibles (cargadas)
     * @return Vector con los IDs de escenas cargadas
     */
    std::vector<std::string> getLoadedScenes() const;
    
    /**
     * @brief Obtiene el número de escenas cargadas
     * @return Cantidad de escenas
     */
    size_t getLoadedSceneCount() const;
    
    // ============================================
    // Input Handling - Reenvío a escena activa
    // ============================================
    
    /**
     * @brief Reenvía keyPressed a la escena activa
     * @param key Código de la tecla
     */
    void keyPressed(int key);
    
    /**
     * @brief Reenvía keyReleased a la escena activa
     * @param key Código de la tecla
     */
    void keyReleased(int key);
    
    /**
     * @brief Reenvía mousePressed a la escena activa
     * @param x Coordenada X del mouse
     * @param y Coordenada Y del mouse
     * @param button Botón presionado
     */
    void mousePressed(int x, int y, int button);
    
    /**
     * @brief Reenvía mouseReleased a la escena activa
     * @param x Coordenada X del mouse
     * @param y Coordenada Y del mouse
     * @param button Botón liberado
     */
    void mouseReleased(int x, int y, int button);
    
    /**
     * @brief Reenvía mouseDragged a la escena activa
     * @param x Coordenada X del mouse
     * @param y Coordenada Y del mouse
     * @param button Botón arrastrado
     */
    void mouseDragged(int x, int y, int button);
    
    /**
     * @brief Reenvía mouseMoved a la escena activa
     * @param x Coordenada X del mouse
     * @param y Coordenada Y del mouse
     */
    void mouseMoved(int x, int y);
    
    /**
     * @brief Reenvía windowResized a la escena activa
     * @param w Nuevo ancho
     * @param h Nuevo alto
     */
    void windowResized(int w, int h);

    // ============================================
    // Global Toggles
    // ============================================

    /**
     * @brief Habilita o deshabilita los puntos de interacción del mouse
     * @param enabled true para habilitar, false para deshabilitar
     */
    void setUseMouseInput(bool enabled);

    /**
     * @brief Consulta si la entrada de mouse está habilitada
     * @return true si los puntos de mouse están activos
     */
    bool isMouseInputEnabled() const;

    /**
     * @brief Muestra u oculta el overlay del esqueleto
     * @param visible true para mostrar, false para ocultar
     */
    void setShowSkeleton(bool visible);

    /**
     * @brief Consulta si el esqueleto está visible
     * @return true si el esqueleto debe dibujarse
     */
    bool isShowSkeleton() const;

    // ============================================
    // Global Background Color
    // ============================================

    /**
     * @brief Cambia el color de fondo global (0-255 grayscale)
     * @param value Valor de gris (0=negro, 255=blanco)
     */
    void setBackgroundColor(float value);

    /**
     * @brief Obtiene el color de fondo actual
     * @return Valor de gris (0-255)
     */
    float getBackgroundColor() const;

    // ============================================
    // Scene Transition — Fade In/Out
    // ============================================

    bool transitionTo(const std::string& sceneId, float duration = 0.6f);
    bool isTransitioning() const;
    float getTransitionAlpha() const;  // 0=scene visible, 1=fully black

private:
    /**
     * @brief Registra los parámetros globales en el registry
     */
    void registerGlobalParameters();

    /**
     * @brief Activa una escena sin adquirir el mutex (uso interno)
     */
    bool activateSceneLocked(const std::string& sceneId);
    
    std::map<std::string, ScenePtr> loadedScenes_;
    std::string activeSceneId_;
    std::string preparedSceneId_;
    bool isPaused_;
    float backgroundColor_;
    mutable std::mutex mutex_;

    // Scene transition
    TransitionState transitionState_ = TransitionState::NONE;
    std::string transitionTargetId_;
    float transitionTimer_ = 0.0f;
    float transitionDuration_ = 0.6f;
    float transitionAlpha_ = 0.0f;
};

} // namespace modular
