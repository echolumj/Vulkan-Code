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
#include <optional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

namespace triangle
{
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
using namespace triangle;

class Triangle
{
public:
	Triangle();
	void run(void);

	//ȷ��window sizeһ�������ı䣬�ʹ�����Ӧ��error
	bool framebufferResized;

private:
	void window_init(void);
	void vulkan_init(void);
	void main_loop(void);
	void clean_up(void);

	void drawFrame(void);
	
	void instance_create(void);
	void debugMessenger_setUp(void);
	void surface_create(void);
	void physicalDevice_pick(void);
	void logicalDevice_create(void);
	void swapChain_create(void);
	void imageView_create(void);

	void renderPass_create(void);
	void graphicsPipline_create(void);
	void framebuffer_create(void);
	void commandPool_create(void);
	void commondBuffers_create(void);
	void syncObjects_create(void);
	void swapCahin_recreate(void);

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void cleanupSwapChain(void);

	void createSSBO(void);
	void createCompDescSetLayout(void);
	void computePipeline_create(void);

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

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

	VkPipelineLayout pipelineLayout;;
	VkRenderPass renderPass;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;

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

	VkPipelineLayout compPipelineLayout;
	VkPipeline compPipeline;
};

