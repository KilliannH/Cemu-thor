#pragma once
#include <vulkan/vulkan.h>
#include <vector>

/**
 * Classe pour détecter et stocker les capabilities du GPU Vulkan
 * Spécialement optimisé pour détecter les GPUs Adreno et leurs features
 */
class VulkanCapabilities {
public:
    /**
     * Initialise les capabilities en interrogeant le device physique
     * Doit être appelé après la création du VkPhysicalDevice
     */
    static void Initialize(VkPhysicalDevice device);
    
    /**
     * Retourne true si le GPU supporte FP16 (shader_float16)
     * Sur Adreno 740, c'est ~2x plus rapide que FP32
     */
    static bool SupportsFP16() { return s_supportsFP16; }
    
    /**
     * Retourne true si le GPU supporte Int8 (shader_int8)
     */
    static bool SupportsInt8() { return s_supportsInt8; }
    
    /**
     * Retourne true si les extensions Qualcomm sont disponibles
     * (VK_QCOM_render_pass_transform, etc.)
     */
    static bool SupportsQcomExtensions() { return s_supportsQcom; }
    
    /**
     * Retourne true si le GPU est un Adreno (vendorID Qualcomm)
     */
    static bool IsAdrenoGPU() { return s_isAdreno; }
    
    /**
     * Retourne le modèle du GPU Adreno (ex: 740 pour Adreno 740)
     * Retourne 0 si ce n'est pas un Adreno
     */
    static uint32_t GetAdrenoModel() { return s_adrenoModel; }
    
    /**
     * Retourne true si 16bit storage est supporté
     * Nécessaire pour stocker des f16vec dans les buffers
     */
    static bool Supports16BitStorage() { return s_supports16BitStorage; }
    
    /**
     * Retourne le nombre de registres par thread (si disponible)
     * Utile pour optimiser la complexité des shaders
     */
    static uint32_t GetMaxRegistersPerThread() { return s_maxRegistersPerThread; }
    
    /**
     * Retourne true si le device supporte VK_EXT_shader_viewport_index_layer
     * Utile pour les geometry shaders
     */
    static bool SupportsShaderViewportIndexLayer() { return s_supportsViewportIndexLayer; }

private:
    static bool s_supportsFP16;
    static bool s_supportsInt8;
    static bool s_supportsQcom;
    static bool s_isAdreno;
    static uint32_t s_adrenoModel;
    static bool s_supports16BitStorage;
    static uint32_t s_maxRegistersPerThread;
    static bool s_supportsViewportIndexLayer;
    
    // Helper pour parser le device name et extraire le modèle Adreno
    static uint32_t ParseAdrenoModel(const char* deviceName);
};
