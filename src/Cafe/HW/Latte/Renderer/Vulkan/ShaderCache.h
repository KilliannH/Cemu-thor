#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <filesystem>

/**
 * Cache pour les shaders optimisés
 * Évite de re-optimiser les mêmes shaders à chaque lancement
 */
class ShaderCache {
  public:
	static ShaderCache& GetInstance();

	/**
     * Tente de récupérer un shader optimisé depuis le cache
     * @param originalHash Hash du shader original
     * @param outOptimizedCode Code optimisé si trouvé
     * @return true si trouvé dans le cache
	 */
	bool GetOptimizedShader(uint64_t originalHash, std::string& outOptimizedCode);

	/**
     * Stocke un shader optimisé dans le cache
     * @param originalHash Hash du shader original
     * @param optimizedCode Code optimisé
	 */
	void StoreOptimizedShader(uint64_t originalHash, const std::string& optimizedCode);

	/**
     * Charge le cache depuis le disque au démarrage
	 */
	void LoadFromDisk();

	/**
     * Sauvegarde le cache sur disque
	 */
	void SaveToDisk();

	/**
     * Statistiques du cache
	 */
	struct Stats {
		size_t hitCount = 0;
		size_t missCount = 0;
		size_t totalEntries = 0;

		float GetHitRate() const {
			if (hitCount + missCount == 0) return 0.0f;
			return (float)hitCount / (float)(hitCount + missCount);
		}
	};

	Stats GetStats() const;

  private:
	ShaderCache() = default;

	std::unordered_map<uint64_t, std::string> m_cache;
	mutable std::mutex m_mutex;

	Stats m_stats{};

	std::filesystem::path GetCacheFilePath() const;

	// Internal version that assumes m_mutex is already held
	void SaveToDiskInternal();
};