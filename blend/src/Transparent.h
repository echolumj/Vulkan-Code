#pragma once

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
//#include <Windows.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <iostream>
#include <vector>
#include <array>
#include <optional>

#include "object.h"


//the size of window
#define   WIDTH   800
#define   HEIGHT   600 

const int PARTICLE_NUM = 2000;
const int MAX_FRAMES_IN_FLIGHT = 2;

typedef glm::vec2 vec2;
typedef glm::vec3 vec3;
typedef glm::vec4 vec4;

namespace compute
{
	struct Particle{
		vec2 position;
		vec2 velocity;
		vec4 color;
	};

}

namespace transparent
{
	struct UniformBufferObject {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec3 color;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, normal);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, color);

			return attributeDescriptions;
		}
	};

	//index of queue family
	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;//�����豸��Ӧ�Ķ�����
		std::optional<uint32_t> presentFamily;
		std::optional<uint32_t> graphicAndComputeFamily;

		bool isComplete() {
			return graphicAndComputeFamily.has_value() && presentFamily.has_value();
		}

	};

	//basic info of swap chain creation
	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

}
using namespace transparent;

class Transparent
{
public:
	Transparent();
	void run(void);

	//ȷ��window sizeһ�������ı䣬�ʹ�����Ӧ��error
	bool framebufferResized;

private:
	void window_init(void);
	void vulkan_init(void);
	void main_loop(void);
	void clean_up(void);
	void processInput(GLFWwindow *window);
	void drawFrame(void);
	
	void instance_create(void);
	void debugMessenger_setUp(void);
	void surface_create(void);
	void physicalDevice_pick(void);
	void logicalDevice_create(void);
	void swapChain_create(void);
	void imageView_create(void);

	void renderPass_create(void);
	void graphicsPiplinePass0_create(void);
	void framebuffer_create(void);
	void vertexAndIndiceBuffer_create(void); //pass0
	void commandPool_create(void);
	void commondBuffers_create(void);
	void syncObjects_create(void);
	void swapCahin_recreate(void);

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void cleanupSwapChain(void);

	void createSSBO(void);
	void createCompDescSetLayout(void);
	void createDescPool(void);
	void createDescSets(void);
	void createDescSetLayout(void);
	void computePipeline_create(void);

	void uniformBuffer_create(void);
	void updateUniformBuffer(uint32_t currentImage);

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectMask, VkImageView& imageView);

	//////////////////////Auxiliary function///////////////////////////////////
	std::vector<const char*> getRequiredExtensions(void);
	bool CheckValidationLayerSupport(void);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice devices);
	bool isDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice devices);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);//��ȡ���µĴ��ڴ�С

	VkShaderModule createShaderModule(const std::vector<char>& code);

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestoryDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	//vertex input
	VkBuffer vertexBuffer;
	VkBuffer vertexSquareBuffer;
	VkBuffer indiceBuffer;
	VkDeviceMemory vertexBufferMem;
	VkDeviceMemory indiceBufferMem;
	VkDeviceMemory vertexSquareBufferMem;

	std::vector <VkBuffer> uniformBuffers;
	std::vector <VkDeviceMemory> uniformBufferMems;
	std::vector<void*> uniformBuffersMapped;

	//handle
	GLFWwindow* window;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;

	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	VkPipelineLayout pipelineLayoutPass0;
	VkRenderPass renderPass;
	VkPipeline pipelinePass_0;
	std::vector<VkFramebuffer> framebuffers;

	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	VkQueue graphicsQueue;//queue handle ,clean up implicitly
	VkQueue presentQueue;
	VkQueue computeQueue;

	//GPU-GPU Synchronization
	//VkSemaphore imageAvailableSemaphore;
	//VkSemaphore renderFinishedSemaphore;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	//CPU-GPU Synchronization
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight; //image count
	size_t currentFrame;

	//compute shader
	std::vector<VkBuffer> shaderStorageBuffer;
	std::vector<VkDeviceMemory> shaderStorageBufferMem;
	VkDeviceSize shaderStorageBufferSize;
	VkDescriptorSetLayout computeDescSetLayout;

	VkDescriptorSetLayout descSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

};

