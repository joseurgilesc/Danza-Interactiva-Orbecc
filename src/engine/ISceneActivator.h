#pragma once

#include <string>

namespace modular {

// ============================================================================
// ISceneActivator - Interfaz para activación de escenas
// ============================================================================

/**
 * @brief Interfaz pura para gestión de activación de escenas
 * 
 * Esta interfaz permite inyectar un objeto que gestione la activación
 * y desactivación de escenas en el Scheduler u otros componentes del sistema.
 * 
 * El Scheduler usa esta interfaz para:
 * - Activar/desactivar escenas según la secuencia
 * - Preparar escenas en modo hybrid (pre-loading)
 * - Consultar cuál escena está actualmente activa
 */
class ISceneActivator {
public:
    virtual ~ISceneActivator() = default;
    
    /**
     * @brief Activa una escena por su ID
     * @param sceneId ID de la escena a activar
     * @return true si la activación fue exitosa
     */
    virtual bool activateScene(const std::string& sceneId) = 0;
    
    /**
     * @brief Desactiva una escena por su ID
     * @param sceneId ID de la escena a desactivar
     * @return true si la desactivación fue exitosa
     */
    virtual bool deactivateScene(const std::string& sceneId) = 0;
    
    /**
     * @brief Prepara una escena para activación rápida (modo hybrid)
     * @param sceneId ID de la escena a preparar
     * @return true si la preparación fue exitosa
     */
    virtual bool prepareScene(const std::string& sceneId) = 0;
    
    /**
     * @brief Obtiene el ID de la escena actualmente activa
     * @return ID de la escena activa, o string vacío si no hay ninguna
     */
    virtual std::string getActiveSceneId() const = 0;
    
    /**
     * @brief Desactiva todas las escenas activas
     * @return true si todas las desactivaciones fueron exitosas
     */
    virtual bool deactivateAll() = 0;
};

} // namespace modular