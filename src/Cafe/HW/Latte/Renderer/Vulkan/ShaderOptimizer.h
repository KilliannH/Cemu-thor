#pragma once
#include <string>
#include <vector>

/**
 * Classe pour optimiser les shaders GLSL pour l'architecture Adreno
 * Applique des transformations comme la conversion FP16, l'optimisation des layouts, etc.
 */
class ShaderOptimizer {
public:
    enum class OptimizationLevel {
        None,       // Pas d'optimisation
        Safe,       // Optimisations conservatrices uniquement
        Aggressive  // Toutes les optimisations (peut casser certains shaders)
    };
    
    /**
     * Convertit un shader GLSL pour utiliser FP16 où approprié
     * @param glslSource Le code source GLSL original
     * @param level Niveau d'optimisation à appliquer
     * @return Le shader optimisé, ou l'original si la conversion échoue
     */
    static std::string OptimizeForAdreno(const std::string& glslSource, OptimizationLevel level = OptimizationLevel::Safe);
    
    /**
     * Convertit uniquement en FP16 (sans autres optimisations)
     * @param glslSource Le code source GLSL original
     * @param forceConvert Force la conversion même si détecté comme unsafe
     * @return Le shader avec types FP16, ou l'original si pas sûr
     */
    static std::string ConvertToFP16(const std::string& glslSource, bool forceConvert = false);
    
    /**
     * Analyse si un shader est sûr pour la conversion FP16
     * Vérifie la présence de patterns qui nécessitent FP32 (matrices, depth, etc.)
     * @param glslSource Le code source GLSL à analyser
     * @return true si safe pour FP16, false sinon
     */
    static bool IsSafeForFP16(const std::string& glslSource);
    
    /**
     * Retourne des statistiques sur le shader
     * Utile pour le debugging et l'analyse de performance
     */
    struct ShaderStats {
        int vec2Count = 0;
        int vec3Count = 0;
        int vec4Count = 0;
        int mat4Count = 0;
        int textureOpsCount = 0;
        bool hasDepthOutput = false;
        bool hasMatrixOps = false;
        int estimatedRegisters = 0; // Estimation basique
    };
    
    static ShaderStats AnalyzeShader(const std::string& glslSource);

private:
    // Ajoute les extensions nécessaires pour FP16
    static std::string AddFP16Extensions(const std::string& source);
    
    // Convertit les types vec2/3/4 et float en f16vec2/3/4 et float16_t
    static std::string ConvertTypes(const std::string& source);
    
    // Identifie les variables qui doivent rester en FP32
    static std::vector<std::string> IdentifyFP32Variables(const std::string& source);
    
    // Applique des optimisations spécifiques Adreno (layouts, etc.)
    static std::string ApplyAdrenoOptimizations(const std::string& source);

	// Converts only local temporary variables to FP16 (safe subset)
	static std::string ConvertLocalTemporariesToFP16(const std::string& source);

    // Patterns à ne jamais convertir en FP16
    static const std::vector<std::string> s_unsafePatterns;
    
    // Variables qui doivent rester en FP32
    static const std::vector<std::string> s_fp32Variables;
};
