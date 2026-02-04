#include "VulkanCapabilities.h"
#include <cstring>
#include <algorithm>
#include <cctype>

// Définition des variables statiques
bool VulkanCapabilities::s_supportsFP16 = false;
bool VulkanCapabilities::s_supportsInt8 = false;
bool VulkanCapabilities::s_supportsQcom = false;
bool VulkanCapabilities::s_isAdreno = false;
uint32_t VulkanCapabilities::s_adrenoModel = 0;
bool VulkanCapabilities::s_supports16BitStorage = false;
uint32_t VulkanCapabilities::s_maxRegistersPerThread = 0;
bool VulkanCapabilities::s_supportsViewportIndexLayer = false;

void VulkanCapabilities::Initialize(VkPhysicalDevice device) {
    // 1. Vérifier les features FP16/Int8
    VkPhysicalDeviceShaderFloat16Int8Features float16Int8Features = {};
    float16Int8Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
    
    VkPhysicalDevice16BitStorageFeatures storage16BitFeatures = {};
    storage16BitFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
    storage16BitFeatures.pNext = &float16Int8Features;
    
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &storage16BitFeatures;
    
    vkGetPhysicalDeviceFeatures2(device, &features2);
    
    s_supportsFP16 = float16Int8Features.shaderFloat16;
    s_supportsInt8 = float16Int8Features.shaderInt8;
    s_supports16BitStorage = storage16BitFeatures.storageBuffer16BitAccess;
    
    // 2. Récupérer les propriétés du device
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    
    // Qualcomm vendor ID = 0x5143
    s_isAdreno = (props.vendorID == 0x5143);
    
    if (s_isAdreno) {
        // Parser le modèle Adreno depuis le nom du device
        s_adrenoModel = ParseAdrenoModel(props.deviceName);
        
        // Sur Adreno, estimer les registres disponibles
        // Adreno 7xx a généralement ~128 registres FP32 par thread
        // ou ~256 registres FP16 par thread
        if (s_adrenoModel >= 700) {
            s_maxRegistersPerThread = s_supportsFP16 ? 256 : 128;
        } else if (s_adrenoModel >= 600) {
            s_maxRegistersPerThread = s_supportsFP16 ? 192 : 96;
        }
    }
    
    // 3. Vérifier les extensions disponibles
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
    
    if (extCount > 0) {
        std::vector<VkExtensionProperties> extensions(extCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, extensions.data());
        
        for (const auto& ext : extensions) {
            // Extensions Qualcomm
            if (strcmp(ext.extensionName, "VK_QCOM_render_pass_transform") == 0) {
                s_supportsQcom = true;
            }
            
            // Extension viewport index layer (utile pour GS)
            if (strcmp(ext.extensionName, "VK_EXT_shader_viewport_index_layer") == 0) {
                s_supportsViewportIndexLayer = true;
            }
        }
    }
    
    // 4. Log des capabilities détectées (peut être enlevé en production)
    #ifdef _DEBUG
    const char* gpuType = s_isAdreno ? "Adreno" : "Other";
    printf("VulkanCapabilities: GPU Type: %s\n", gpuType);
    if (s_isAdreno) {
        printf("VulkanCapabilities: Adreno Model: %u\n", s_adrenoModel);
    }
    printf("VulkanCapabilities: FP16 Support: %s\n", s_supportsFP16 ? "Yes" : "No");
    printf("VulkanCapabilities: Int8 Support: %s\n", s_supportsInt8 ? "Yes" : "No");
    printf("VulkanCapabilities: 16-bit Storage: %s\n", s_supports16BitStorage ? "Yes" : "No");
    printf("VulkanCapabilities: Qualcomm Extensions: %s\n", s_supportsQcom ? "Yes" : "No");
    if (s_maxRegistersPerThread > 0) {
        printf("VulkanCapabilities: Estimated Registers/Thread: %u\n", s_maxRegistersPerThread);
    }
    #endif
}

uint32_t VulkanCapabilities::ParseAdrenoModel(const char* deviceName) {
    // Le nom du device est généralement de la forme "Adreno (TM) 740" ou "Adreno 740"
    std::string name(deviceName);
    
    // Chercher "Adreno" dans le nom
    size_t adrenoPos = name.find("Adreno");
    if (adrenoPos == std::string::npos) {
        return 0;
    }
    
    // Chercher le numéro après "Adreno"
    size_t numStart = adrenoPos + 6; // "Adreno" = 6 chars
    
    // Sauter les espaces et caractères non-numériques
    while (numStart < name.length() && !std::isdigit(name[numStart])) {
        numStart++;
    }
    
    if (numStart >= name.length()) {
        return 0;
    }
    
    // Parser le numéro
    uint32_t model = 0;
    while (numStart < name.length() && std::isdigit(name[numStart])) {
        model = model * 10 + (name[numStart] - '0');
        numStart++;
    }
    
    return model;
}
