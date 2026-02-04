#include "ShaderCache.h"
#include "config/ActiveSettings.h"
#include <fstream>
#include <sstream>

ShaderCache& ShaderCache::GetInstance() {
	static ShaderCache instance;
	return instance;
}

bool ShaderCache::GetOptimizedShader(uint64_t originalHash, std::string& outOptimizedCode) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_cache.find(originalHash);
	if (it != m_cache.end()) {
		outOptimizedCode = it->second;
		m_stats.hitCount++;
		return true;
	}

	m_stats.missCount++;
	return false;
}

void ShaderCache::StoreOptimizedShader(uint64_t originalHash, const std::string& optimizedCode) {
	std::lock_guard<std::mutex> lock(m_mutex);

	m_cache[originalHash] = optimizedCode;
	m_stats.totalEntries = m_cache.size();

	// Sauvegarder périodiquement (tous les 10 nouveaux shaders)
	if (m_stats.totalEntries % 10 == 0) {
		SaveToDisk();
	}
}

std::filesystem::path ShaderCache::GetCacheFilePath() const {
	// Utiliser le même dossier que le cache SPIR-V
	return ActiveSettings::GetCachePath("shaderCache/optimized_shaders.cache");
}

void ShaderCache::LoadFromDisk() {
	auto cachePath = GetCacheFilePath();

	if (!std::filesystem::exists(cachePath)) {
		return; // Pas de cache existant
	}

	std::ifstream file(cachePath, std::ios::binary);
	if (!file.is_open()) {
		return;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	try {
		// Format simple :
		// [version:uint32][count:uint64]
		// Pour chaque entrée: [hash:uint64][codeLength:uint32][code:bytes]

		uint32_t version;
		file.read(reinterpret_cast<char*>(&version), sizeof(version));

		if (version != 1) {
			// Version incompatible, ignorer le cache
			return;
		}

		uint64_t count;
		file.read(reinterpret_cast<char*>(&count), sizeof(count));

		for (uint64_t i = 0; i < count; ++i) {
			uint64_t hash;
			file.read(reinterpret_cast<char*>(&hash), sizeof(hash));

			uint32_t codeLength;
			file.read(reinterpret_cast<char*>(&codeLength), sizeof(codeLength));

			std::string code(codeLength, '\0');
			file.read(&code[0], codeLength);

			m_cache[hash] = std::move(code);
		}

		m_stats.totalEntries = m_cache.size();

#ifdef _DEBUG
		printf("ShaderCache: Loaded %zu optimized shaders from cache\n", m_cache.size());
#endif

	} catch (...) {
		// Erreur de lecture, ignorer le cache
		m_cache.clear();
	}
}

void ShaderCache::SaveToDisk() {
	auto cachePath = GetCacheFilePath();

	// Créer le dossier parent si nécessaire
	std::filesystem::create_directories(cachePath.parent_path());

	std::ofstream file(cachePath, std::ios::binary);
	if (!file.is_open()) {
		return;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	try {
		// Version du format
		uint32_t version = 1;
		file.write(reinterpret_cast<const char*>(&version), sizeof(version));

		// Nombre d'entrées
		uint64_t count = m_cache.size();
		file.write(reinterpret_cast<const char*>(&count), sizeof(count));

		// Écrire chaque entrée
		for (const auto& [hash, code] : m_cache) {
			file.write(reinterpret_cast<const char*>(&hash), sizeof(hash));

			uint32_t codeLength = static_cast<uint32_t>(code.size());
			file.write(reinterpret_cast<const char*>(&codeLength), sizeof(codeLength));

			file.write(code.data(), codeLength);
		}

#ifdef _DEBUG
		printf("ShaderCache: Saved %zu optimized shaders to cache\n", m_cache.size());
#endif

	} catch (...) {
		// Erreur d'écriture, ignorer
	}
}

ShaderCache::Stats ShaderCache::GetStats() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_stats;
}