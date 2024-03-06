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
const int MAX_FRAMES_IN_FLIGHT = 1;

typedef glm::vec2 vec2;
typedef glm::vec3 vec3;
typedef glm::vec4 vec4;

namespace vk
{
	struct UniformBufferObject {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 lightProjViewModel;
		glm::vec3 lightPos;
	};

	struct SVertex {
		glm::vec2 pos;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(SVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 1> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(SVertex, pos);

			return attributeDescriptions;
		}
	};

	struct Vertex {
		glm::vec3 pos;
		glm::vec2 coord;
		glm::vec3 normal;

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
			attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, coord);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, normal);

			return attributeDescriptions;
		}
	};

	//index of queue family
	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;//物理设备对应的队列族
		std::optional<uint32_t> presentFamily;
		std::optional<uint32_t> graphicAndComputeFamily;

		bool isComplete() {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}

	};

	//basic info of swap chain creation
	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

}
using namespace vk;

class ShadowMap
{
public:
	ShadowMap();
	void run(void);

	//确保window size一旦发生改变，就触发相应的error
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
	void graphicsPiplinePass1_create(void);
	void framebuffer_create(void);
	void vertexAndIndiceBuffer_create(void); //pass0 + pass1
	void commandPool_create(void);
	void commondBuffers_create(void);
	void syncObjects_create(void);
	void swapCahin_recreate(void);

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void cleanupSwapChain(void);

	void createDescPool(void);
	void createDescSets(void);
	void createDescSetLayout(void);

	void uniformBuffer_create(void);
	void updateUniformBuffer(uint32_t currentImage);
	void createDepthResource(void);

	//////////////////////Auxiliary function///////////////////////////////////
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void copyBufferToImage(VkCommandBuffer& cb, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectMask, VkImageView& imageView);
	void createImageSampler(VkSampler& sampler);

	std::vector<const char*> getRequiredExtensions(void);
	bool CheckValidationLayerSupport(void);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice devices);
	bool isDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice devices);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);//获取最新的窗口大小

	VkShaderModule createShaderModule(const std::vector<char>& code);

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestoryDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	void transitionImageLayout(VkCommandBuffer& cb, VkImage &image, VkFormat format, VkImageLayout initImageLayout, VkImageLayout targetImageLayout);

	VkCommandBuffer beginSingleTimeCommands(void);
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
	//vertex input
	VkBuffer vertexBuffer;
	VkBuffer indiceBuffer;
	VkDeviceMemory vertexBufferMem;
	VkDeviceMemory indiceBufferMem;

	//pass0 without lightInfo
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

	std::vector<VkImage> depthImages;
	std::vector<VkImageView> depthImageViews;
	std::vector<VkSampler> depthImageSamplers;
	std::vector<VkDeviceMemory> depthImageMems;
	VkFormat depthImageFormat;
	VkExtent2D depthImageExtent;

	std::vector<VkImage> depthImagesPass1;
	std::vector<VkImageView> depthImageViewsPass1;
	std::vector<VkDeviceMemory> depthImageMemsPass1;

	VkImage texImages;
	VkImageView texImageViews;
	VkSampler texImageSamplers;
	VkDeviceMemory texImageMems;

	VkPipelineLayout pipelineLayoutPass0;
	VkPipelineLayout pipelineLayoutPass1;
	VkRenderPass renderPass;
	VkRenderPass offRenderPass;
	VkPipeline pipelinePass_0;
	VkPipeline pipelinePass_1;
	std::vector<VkFramebuffer> framebuffers;
	std::vector<VkFramebuffer> offFramebuffers;

	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	VkQueue graphicsQueue;//queue handle ,clean up implicitly
	VkQueue presentQueue;

	//GPU-GPU Synchronization
	//VkSemaphore imageAvailableSemaphore;
	//VkSemaphore renderFinishedSemaphore;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	//CPU-GPU Synchronization
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight; //image count
	size_t currentFrame;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descSetLayoutPass0;
	VkDescriptorSetLayout descSetLayoutPass1;
	std::vector<VkDescriptorSet> descriptorSetsPass0;
	std::vector<VkDescriptorSet> descriptorSetsPass1;

	glm::mat4 rotateMat = glm::mat4(1.0f);
	glm::mat4 translateMat = glm::mat4(1.0f);
};

