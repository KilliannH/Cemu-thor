#pragma once
#include <vulkan/vulkan.h>
#include "VulkanCapabilities.h"

namespace VulkanBarrierHelpers {

	/**
     * Barrière mémoire optimisée pour Adreno TBDR
     * Sur Adreno, les barrières mémoire génériques sont TRÈS coûteuses
	 */
	inline void SmartMemoryBarrier(
		VkCommandBuffer commandBuffer,
		VkPipelineStageFlags srcStage,
		VkPipelineStageFlags dstStage,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask)
	{
#ifdef __ANDROID__
		if (VulkanCapabilities::IsAdrenoGPU()) {
			// Sur Adreno TBDR, réduire la portée des barrières

			// Si c'est juste SHADER_READ après COLOR_WRITE, peut être skip
			bool isSimpleReadAfterWrite =
				(srcAccessMask == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) &&
				(dstAccessMask == VK_ACCESS_SHADER_READ_BIT);

			if (isSimpleReadAfterWrite) {
				// Sur Adreno, les reads après writes dans le même tile
				// ne nécessitent pas forcément de barrière explicite
				// MAIS seulement si on est dans le même renderpass
				// Pour l'instant, on garde la barrière mais on la rend plus légère

				VkMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
				barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				// Utiliser des stages plus précis (moins coûteux)
				vkCmdPipelineBarrier(
					commandBuffer,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_DEPENDENCY_BY_REGION_BIT, // IMPORTANT sur TBDR !
					1, &barrier,
					0, nullptr,
					0, nullptr
				);
				return;
			}
		}
#endif

		// Fallback : barrière normale
		VkMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;

		vkCmdPipelineBarrier(
			commandBuffer,
			srcStage,
			dstStage,
			0,
			1, &barrier,
			0, nullptr,
			0, nullptr
		);
	}

	/**
     * Version encore plus optimisée : utilise VK_DEPENDENCY_BY_REGION_BIT
     * sur Adreno pour éviter les flush de tiles complets
	 */
	inline void SmartMemoryBarrierByRegion(
		VkCommandBuffer commandBuffer,
		VkPipelineStageFlags srcStage,
		VkPipelineStageFlags dstStage,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask)
	{
		VkMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;

		VkDependencyFlags dependencyFlags = 0;
#ifdef __ANDROID__
		if (VulkanCapabilities::IsAdrenoGPU()) {
			// BY_REGION évite les flush complets sur TBDR
			dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}
#endif

		vkCmdPipelineBarrier(
			commandBuffer,
			srcStage,
			dstStage,
			dependencyFlags,
			1, &barrier,
			0, nullptr,
			0, nullptr
		);
	}
}