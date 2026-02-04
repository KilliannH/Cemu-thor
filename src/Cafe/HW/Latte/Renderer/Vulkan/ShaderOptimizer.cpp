#include "ShaderOptimizer.h"
#include "VulkanCapabilities.h"
#include <regex>
#include <sstream>
#include <algorithm>

// Patterns unsafe pour FP16
const std::vector<std::string> ShaderOptimizer::s_unsafePatterns = {
    "mat4",           // Matrices 4x4 (transformations)
    "mat3",           // Matrices 3x3
    "projection",     // Variables de projection
    "worldMatrix",    // Matrices world
    "viewMatrix",     // Matrices view
    "modelMatrix",    // Matrices model
    "gl_FragDepth",   // Sortie de profondeur
    "gl_Position",    // Position (mais conversion auto OK)
    "dFd",            // Dérivées (precision critique)
};

// Variables qui doivent toujours rester en FP32
const std::vector<std::string> ShaderOptimizer::s_fp32Variables = {
    "depth",
    "zNear",
    "zFar",
    "projection",
    "view",
    "world",
    "mvp",
    "modelViewProjection"
};

std::string ShaderOptimizer::OptimizeForAdreno(const std::string& glslSource, OptimizationLevel level) {
    if (level == OptimizationLevel::None) {
        return glslSource;
    }
    
    // Vérifier que FP16 est supporté
    if (!VulkanCapabilities::SupportsFP16()) {
        return glslSource;
    }
    
    std::string result = glslSource;
    
    // Appliquer conversion FP16
    if (level == OptimizationLevel::Aggressive || IsSafeForFP16(result)) {
        result = ConvertToFP16(result, level == OptimizationLevel::Aggressive);
    }
    
    // Appliquer d'autres optimisations Adreno
    if (VulkanCapabilities::IsAdrenoGPU()) {
        result = ApplyAdrenoOptimizations(result);
    }
    
    return result;
}

std::string ShaderOptimizer::ConvertToFP16(const std::string& glslSource, bool forceConvert) {
    // Ne convertir que si FP16 est supporté
    if (!VulkanCapabilities::SupportsFP16() && !forceConvert) {
        return glslSource;
    }
    
    // Ne pas convertir si unsafe (sauf si forcé)
    if (!forceConvert && !IsSafeForFP16(glslSource)) {
        return glslSource;
    }
    
    std::string result = glslSource;
    
    // 1. Ajouter les extensions FP16
    result = AddFP16Extensions(result);
    
    // 2. Identifier les variables qui doivent rester FP32
    auto fp32Vars = IdentifyFP32Variables(result);
    
    // 3. Convertir les types
    result = ConvertTypes(result);
    
    // 4. TODO: Re-convertir en FP32 les variables identifiées
    // (nécessite une analyse plus sophistiquée)
    
    return result;
}

bool ShaderOptimizer::IsSafeForFP16(const std::string& glslSource) {
    // Vérifier la présence de patterns unsafe
    for (const auto& pattern : s_unsafePatterns) {
        if (glslSource.find(pattern) != std::string::npos) {
            // Exception: gl_Position est OK car conversion auto FP16→FP32
            if (pattern == "gl_Position") {
                continue;
            }
            return false;
        }
    }
    
    // Vérifier la présence de variables critiques
    for (const auto& varName : s_fp32Variables) {
        if (glslSource.find(varName) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

ShaderOptimizer::ShaderStats ShaderOptimizer::AnalyzeShader(const std::string& glslSource) {
    ShaderStats stats = {};
    
    // Compter les types de vecteurs
    std::regex vec2Pattern(R"(\bvec2\b)");
    std::regex vec3Pattern(R"(\bvec3\b)");
    std::regex vec4Pattern(R"(\bvec4\b)");
    std::regex mat4Pattern(R"(\bmat4\b)");
    
    auto countMatches = [&](const std::regex& pattern) {
        auto begin = std::sregex_iterator(glslSource.begin(), glslSource.end(), pattern);
        auto end = std::sregex_iterator();
        return std::distance(begin, end);
    };
    
    stats.vec2Count = countMatches(vec2Pattern);
    stats.vec3Count = countMatches(vec3Pattern);
    stats.vec4Count = countMatches(vec4Pattern);
    stats.mat4Count = countMatches(mat4Pattern);
    
    // Détecter sortie depth
    stats.hasDepthOutput = (glslSource.find("gl_FragDepth") != std::string::npos);
    
    // Détecter opérations matricielles
    stats.hasMatrixOps = (stats.mat4Count > 0) || 
                         (glslSource.find("mat3") != std::string::npos);
    
    // Compter les opérations texture
    std::regex texturePattern(R"(\b(texture|texelFetch|textureLod)\s*\()");
    stats.textureOpsCount = countMatches(texturePattern);
    
    // Estimation grossière des registres
    // vec4 = 4 registres FP32, vec3 = 3, vec2 = 2
    // Sur Adreno, FP16 divise par 2
    stats.estimatedRegisters = 
        stats.vec2Count * 2 + 
        stats.vec3Count * 3 + 
        stats.vec4Count * 4 + 
        stats.mat4Count * 16;
    
    return stats;
}

std::string ShaderOptimizer::AddFP16Extensions(const std::string& source) {
    // Trouver la ligne #version
    size_t versionPos = source.find("#version");
    if (versionPos == std::string::npos) {
        // Pas de version trouvée, ajouter au début
        return 
            "#version 450\n"
            "#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require\n"
            "#extension GL_EXT_shader_16bit_storage : require\n" + 
            source;
    }
    
    size_t endOfLine = source.find('\n', versionPos);
    if (endOfLine == std::string::npos) {
        endOfLine = source.length();
    }
    
    std::string extensions = 
        "#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require\n"
        "#extension GL_EXT_shader_16bit_storage : require\n";
    
    return source.substr(0, endOfLine + 1) + 
           extensions + 
           source.substr(endOfLine + 1);
}

std::string ShaderOptimizer::ConvertTypes(const std::string& source) {
    std::string result = source;
    
    // Important: Ne pas toucher aux déclarations de built-ins comme gl_Position
    // Ces conversions sont simplistes et nécessitent une meilleure analyse syntaxique
    
    // Convertir les déclarations de variables locales
    // Pattern: type nom; ou type nom = ...;
    
    // vec4 → f16vec4 (sauf pour gl_Position, etc.)
    std::regex vec4DeclPattern(R"(\b(vec4)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*[;=])");
    result = std::regex_replace(result, vec4DeclPattern, "f16vec4 $2$3");
    
    // vec3 → f16vec3
    std::regex vec3DeclPattern(R"(\b(vec3)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*[;=])");
    result = std::regex_replace(result, vec3DeclPattern, "f16vec3 $2$3");
    
    // vec2 → f16vec2
    std::regex vec2DeclPattern(R"(\b(vec2)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*[;=])");
    result = std::regex_replace(result, vec2DeclPattern, "f16vec2 $2$3");
    
    // float → float16_t (dans les déclarations, pas dans les casts)
    std::regex floatDeclPattern(R"(\bfloat\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*[;=])");
    result = std::regex_replace(result, floatDeclPattern, "float16_t $1$2");
    
    // Convertir aussi les types dans les layouts (uniforms, push constants)
    // layout(...) uniform ... { vec2 → f16vec2 }
    // Note: Ceci nécessite que 16bit_storage soit supporté
    
    return result;
}

std::vector<std::string> ShaderOptimizer::IdentifyFP32Variables(const std::string& source) {
    std::vector<std::string> fp32Vars;
    
    // Parser le source pour identifier les variables qui doivent rester FP32
    // Pour l'instant, utiliser la liste hardcodée
    for (const auto& varName : s_fp32Variables) {
        if (source.find(varName) != std::string::npos) {
            fp32Vars.push_back(varName);
        }
    }
    
    // Identifier les matrices
    std::regex matrixDeclPattern(R"(\bmat[234]\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    std::sregex_iterator begin(source.begin(), source.end(), matrixDeclPattern);
    std::sregex_iterator end;
    
    for (auto it = begin; it != end; ++it) {
        fp32Vars.push_back((*it)[1].str());
    }
    
    return fp32Vars;
}

std::string ShaderOptimizer::ApplyAdrenoOptimizations(const std::string& source) {
    std::string result = source;
    
    // Optimisation 1: Utiliser mediump pour les variables qui n'ont pas besoin de highp
    // Sur Adreno, mediump est souvent mappé à FP16
    
    // Optimisation 2: Optimiser les layouts
    // Sur TBDR, minimiser les écritures en mémoire
    
    // Optimisation 3: Utiliser des hints pour le compiler
    // #pragma optimize(on) / optimize(off)
    
    // Pour l'instant, ces optimisations sont basiques
    // Une implémentation complète nécessiterait une analyse AST
    
    return result;
}
