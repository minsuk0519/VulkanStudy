#pragma once

//3rd party library
#include <vulkan/vulkan.h>

//standard library

enum BUFFERTYPE
{
	BUFFER_VERTEX,
	BUFFER_INDEX,
	BUFFER_UNIFORM,
	BUFFER_MAX,
};

class Buffer;

class VulkanMemoryManager
{
public:
	static void Init(VkDevice device);

	static Buffer* CreateVertexBuffer(void* memory, size_t memorysize);
	static Buffer* CreateIndexBuffer(void* memory, size_t memorysize);
	static Buffer* CreateUniformBuffer(size_t memorysize);
private:
	static VkDevice vulkanDevice;
	static VkPhysicalDevice vulkanPhysicalDevice;
	static VkQueue vulkanQueue;
	static VkCommandPool vulkanCommandpool;

public:
	static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	static void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	static VkCommandBuffer beginSingleTimeCommands();
	static void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	static void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	static void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	static VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	static void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	static void FreeMemory(VkBuffer buf, VkDeviceMemory devicememory);

	static void MapMemory(VkDeviceMemory devicememory, size_t size, void* data);
};

class Buffer
{
public:
	void close();

	friend class VulkanMemoryManager;
public:
	VkBuffer GetBuffer() const;
	VkDeviceMemory GetMemory() const;

private:
	uint32_t id;

	BUFFERTYPE type;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory;
	VkDeviceSize size;
};