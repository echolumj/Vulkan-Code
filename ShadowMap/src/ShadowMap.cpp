#include "ShadowMap.h"
#include <iostream>
#include <fstream>
#include <set>
#include <random>
#include <ctime>
#include <chrono>
#include <array>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif // NDEBUG

uint32_t indicesNum = 0;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 1.0f);//相机位置
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);//面对的方向,以这个为尺度更改观看的，其实就是单位速度
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);//头顶的方向

glm::vec3 lightPos = cameraPos + glm::vec3(1.0f, 4.0f, 3.0f);//头顶的方向

float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;

float yaw;
float pitch;

float fov = 45.0f;//视野比例

int dynamicAlignment = 0;

//check whether the physical device support the required extensions 
const std::vector<const char*> requireExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	//去除VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT（正常的诊断信息）
	if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

//call back function
//定义为静态函数，才可以做回调函数
static void framebufferResizeCallback(GLFWwindow * window, int width, int height)
{
	auto app = reinterpret_cast<ShadowMap*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}

bool firstMouse = true;
static void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
	//
	std::cout << "xpos:" << xpos << "   ypos:" << ypos << std::endl;
	if (firstMouse) // 这个bool变量初始时是设定为true的
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
		//        return;
	}
	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;
	lastX = xpos;
	lastY = ypos;


	float sensitivity = 0.05f;//灵敏度
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	pitch = pitch > 89.0f ? 89.0f : pitch;
	pitch = pitch < -89.0f ? -89.0f : pitch;

	glm::vec3 front;
	//根据俯仰和偏航角度来算出此向量，也就是速度在三个维度的数值
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch)) - 1;
	cameraFront = glm::normalize(front);
}

//滚轮的回调
static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
	if (fov >= 1.0f && fov <= 45.0f) {
		fov -= yoffset;
	}

	fov = fov <= 1.0f ? 1.0f : fov;
	fov = fov >= 45.0f ? 45.0f : fov;
}

// read spv file code
static std::vector<char> readFile(const std::string& filename)
{
	//ate: Start reading at the end of the file
	//binary: Read the file as binary file(avoid text transformations)
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}


ShadowMap::ShadowMap()
{
	//检测窗口大小变化，改变帧缓冲大小
	this->framebufferResized = false;
	this->physicalDevice = VK_NULL_HANDLE;

	this->currentFrame = 0;
}

//public part
void ShadowMap::run(void)
{
	window_init();
	vulkan_init();
	main_loop();
	clean_up();
}

//private part
void ShadowMap::window_init(void)
{
	glfwInit(); //配置GLFW
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //显示设置，阻止自动创建Opengl上下文
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);//窗口大小不可变

	window = glfwCreateWindow(WIDTH, HEIGHT, "vulkan", nullptr, nullptr);

	//creat window fail
	if (window == NULL)
	{
		std::cout << "Failed to creat GLFW	window" << "\n";
		glfwTerminate();
		return;
	}
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	//glfwSetCursorPosCallback(window, mouse_callback);
	//glfwSetScrollCallback(window, scroll_callback);
}

void ShadowMap::vulkan_init(void)
{
	instance_create();
	debugMessenger_setUp();
	surface_create();
	physicalDevice_pick();
	logicalDevice_create();
	swapChain_create();
	imageView_create();

	commandPool_create();
	commondBuffers_create();
	createDepthResource();

    renderPass_create();
	createDescSetLayout();
	uniformBuffer_create();
	vertexAndIndiceBuffer_create();
	createDescPool();

	graphicsPiplinePass0_create();
	graphicsPiplinePass1_create();

	createDescSets();
	framebuffer_create();
	syncObjects_create();
}
float deltaTime = 0.0f;
float lastFrameTime = 0.0f;

void ShadowMap::main_loop(void)
{
	while (!glfwWindowShouldClose(window))
	{
		processInput(window);

		//记录deltaTime
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrameTime;
		lastFrameTime = currentFrame;

		glfwPollEvents();
		drawFrame();
		//★vkQueueWaitIdle(presentQueue);
	}
	//all of the operations in drawFrame are asynchronous
	vkDeviceWaitIdle(logicalDevice);
}
void ShadowMap::createDepthResource(void)
{
	//depth image
	depthImageFormat = VK_FORMAT_D32_SFLOAT;
	depthImageExtent = swapChainExtent;

	depthImages.resize(swapChainImages.size());
	depthImageMems.resize(depthImages.size());
	depthImageViews.resize(depthImages.size());
	depthImageSamplers.resize(depthImages.size());

	depthImagesPass1.resize(depthImages.size());
	depthImageMemsPass1.resize(depthImages.size());
	depthImageViewsPass1.resize(depthImages.size());

	int width, height, nrChannels;
	unsigned char* pixels = stbi_load("E:/2_GITHUB/Vulkan-Code/ShadowMap/data/room.png", &width, &height, &nrChannels, 0);

	VkDeviceSize imageSize = width * height * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	stbi_image_free(pixels);

	createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texImages, texImageMems);
	createImageView(texImages, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, texImageViews);
	createImageSampler(texImageSamplers);

	auto cb = beginSingleTimeCommands();
	transitionImageLayout(cb, texImages, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(cb, stagingBuffer, texImages, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	transitionImageLayout(cb, texImages, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	endSingleTimeCommands(cb);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	for (int i = 0; i < depthImages.size(); ++i)
	{
		createImage(depthImageExtent.width, depthImageExtent.height, depthImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT 
			| VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImages[i], depthImageMems[i]);
		createImageView(depthImages[i], depthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImageViews[i]);
		createImageSampler(depthImageSamplers[i]);

		createImage(depthImageExtent.width, depthImageExtent.height, depthImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImagesPass1[i], depthImageMemsPass1[i]);
		createImageView(depthImagesPass1[i], depthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImageViewsPass1[i]);
	}
}

void ShadowMap::updateUniformBuffer(uint32_t currentImage) {
	static auto startTime =  std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject uboPass0{};
	uboPass0.model = glm::mat4(1.0f);// glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	uboPass0.view = glm::lookAt(lightPos, glm::vec3(0,0,0), cameraUp);
	uboPass0.proj = glm::ortho(-1.5,1.5,-1.5,1.5,0.1,7.5);//glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 7.0f); //

	UniformBufferObject uboPass1{};
	uboPass1.model = translateMat * rotateMat;
	uboPass1.view = glm::lookAt(cameraPos, glm::vec3(0, 0, 0), cameraUp);
	uboPass1.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);

	uboPass0.proj[1][1] *= -1;
	uboPass1.proj[1][1] *= -1;

	uboPass1.lightProjViewModel = uboPass0.proj * uboPass0.view * uboPass0.model;
	uboPass1.lightPos = lightPos;

	glm::vec4 vec = uboPass1.lightProjViewModel * vec4(0.130, 337, 0.151, 1.0);

	memcpy(uniformBuffersMapped[currentImage], &uboPass0, sizeof(uboPass0));
	memcpy((unsigned char*)(uniformBuffersMapped[currentImage]) + dynamicAlignment, &uboPass1, sizeof(uboPass1));
}

void ShadowMap::drawFrame(void)
{
	//fence initized as true
	vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	//1.Acquiring an image from the swap chain
	uint32_t imageIndex;
	VkResult result =  vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		swapCahin_recreate();
		return;
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(logicalDevice, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}

	vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

	imagesInFlight[imageIndex] = inFlightFences[currentFrame];

	updateUniformBuffer(currentFrame);

	//2.Submitting the command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	
	std::vector<VkSemaphore> waitSemaphores{ imageAvailableSemaphores[currentFrame]};
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = static_cast<uint32_t>( waitSemaphores.size());
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages;

	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	recordCommandBuffer(commandBuffers[currentFrame], imageIndex);
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

	std::vector<VkSemaphore> signalSemaphores{ renderFinishedSemaphores[currentFrame] };
	submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	// submit the command buffer to the graphics queue 
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
	presentInfo.pWaitSemaphores = signalSemaphores.data();

	std::vector<VkSwapchainKHR> swapChains{ swapChain };
	presentInfo.swapchainCount = static_cast<uint32_t>(swapChains.size());
	presentInfo.pSwapchains = swapChains.data();
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(presentQueue, &presentInfo);

	//we will also recreate the swap chain if it is suboptimal, because we want the best possible result.
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
	{
		//此时以及执行了present，不需要return
		framebufferResized = false;//consistent
		swapCahin_recreate();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to present swap chain image!");
	}

	//为什么要把这行代码放在这而不是循环中？
	//因为会存在这个函数中未执行到这句话，而转去重建swapchain，可能存在被丢弃需要重新显示的image
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void ShadowMap::swapCahin_recreate() 
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	//part1: wait device idle 
	vkDeviceWaitIdle(logicalDevice);

	//part2:clean up old swap chain
	cleanupSwapChain();

	//part3:recreate swap chain
	swapChain_create();
	imageView_create();
	createDepthResource();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorImageInfo depthImageInfo{};
		depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		depthImageInfo.imageView = depthImageViews[i];
		depthImageInfo.sampler = depthImageSamplers[i];

		VkDescriptorImageInfo texImageInfo{};
		texImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		texImageInfo.imageView =  texImageViews;
		texImageInfo.sampler = texImageSamplers;

		std::array<VkWriteDescriptorSet,2> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSetsPass1[i];
		descriptorWrites[0].dstBinding = 1;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pImageInfo = &depthImageInfo; // Optional

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSetsPass1[i];
		descriptorWrites[1].dstBinding = 2;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &texImageInfo; // Optional

		vkUpdateDescriptorSets(logicalDevice, 2, descriptorWrites.data(), 0, nullptr);
	}

	framebuffer_create();
}

void ShadowMap::cleanupSwapChain(void)
{
	//1.FrameBuffer 
	for (auto framebuffer : framebuffers)
	{
		vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
	}

	for (auto framebuffer : offFramebuffers)
	{
		vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
	}

	//2.Swap Chain Images View
	for (int i = 0; i < swapChainImages.size(); ++i)
	{
		vkDestroyImageView(logicalDevice, swapChainImageViews[i], nullptr);
		vkDestroyImage(logicalDevice, depthImages[i], nullptr);
		vkFreeMemory(logicalDevice, depthImageMems[i], nullptr);
		vkDestroyImageView(logicalDevice, depthImageViews[i], nullptr);
		vkDestroySampler(logicalDevice, depthImageSamplers[i], nullptr);

		vkDestroyImage(logicalDevice, depthImagesPass1[i], nullptr);
		vkFreeMemory(logicalDevice, depthImageMemsPass1[i], nullptr);
		vkDestroyImageView(logicalDevice, depthImageViewsPass1[i], nullptr);
	}

	vkDestroyImage(logicalDevice, texImages, nullptr);
	vkFreeMemory(logicalDevice, texImageMems, nullptr);
	vkDestroyImageView(logicalDevice, texImageViews, nullptr);
	vkDestroySampler(logicalDevice, texImageSamplers, nullptr);

	//3.Swap Chain
	vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
}

void ShadowMap::clean_up(void)
{
	//vkFreeCommandBuffers(logicalDevice, commandPool, commandBuffers.size(), commandBuffers.data());
	cleanupSwapChain();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);

		//vkDestroyBuffer(logicalDevice, shaderStorageBuffer[i], nullptr);
		//vkFreeMemory(logicalDevice, shaderStorageBufferMem[i], nullptr);

		vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, uniformBufferMems[i], nullptr);
	}

	vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
	vkFreeMemory(logicalDevice, vertexBufferMem, nullptr);

	vkDestroyBuffer(logicalDevice, indiceBuffer, nullptr);
	vkFreeMemory(logicalDevice, indiceBufferMem, nullptr);

	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descSetLayoutPass0, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descSetLayoutPass1, nullptr);

	//vkDestroyPipeline(logicalDevice, compPipeline, nullptr);
	//vkDestroyPipelineLayout(logicalDevice, compPipelineLayout, nullptr);
		
	vkDestroyPipeline(logicalDevice, pipelinePass_0, nullptr);
	vkDestroyPipeline(logicalDevice, pipelinePass_1, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayoutPass0, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayoutPass1, nullptr);

	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
	vkDestroyRenderPass(logicalDevice, offRenderPass, nullptr);

	vkDestroyDevice(logicalDevice, nullptr);

	if (enableValidationLayers)
		DestoryDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

//要确保surface在instance之前被销毁
vkDestroySurfaceKHR(instance, surface, nullptr);
vkDestroyInstance(instance, nullptr);

glfwDestroyWindow(window);
glfwTerminate();
}


void ShadowMap::instance_create(void)
{
	if (enableValidationLayers && !CheckValidationLayerSupport()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo appInfo{};//{}初始化必不可少，结构体内存在指针变量
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	//置于if语句之外，以确保它在vkCreateInstance 调用之前不会被销毁
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		//set debug information call back
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = debugCallback;

		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create instance!");
	}
}

// create debugMessenger
VkResult ShadowMap::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func == nullptr)
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
}

//destory debugMessenger
void ShadowMap::DestoryDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debugMessenger, pAllocator);
}

void ShadowMap::transitionImageLayout(VkCommandBuffer &cb,VkImage& image, VkFormat format, VkImageLayout initImageLayout, VkImageLayout targetImageLayout)
{
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = initImageLayout;
	barrier.newLayout = targetImageLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (targetImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if (initImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL&& targetImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (initImageLayout == VK_IMAGE_LAYOUT_UNDEFINED && targetImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (initImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && targetImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	vkCmdPipelineBarrier(
		cb,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

void ShadowMap::debugMessenger_setUp(void)
{
	//如果不开启验证层，就不会又相应的验证信息反馈
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug messenger!");
	}
}


void ShadowMap::surface_create(void)
{
	//用vulkan创建surface
	//配置相关信息
	
	VkWin32SurfaceCreateInfoKHR  surfaceCreateInfo{};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hwnd = glfwGetWin32Window(window);//获取窗口handle
	surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);

	if (vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface))
	{
		throw std::runtime_error(" failed to create window surface .");
	}
	
	
	//if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
	//	throw std::runtime_error("failed to create window surface!");
	//}
	
}

QueueFamilyIndices ShadowMap::findQueueFamilies(VkPhysicalDevice devices)
{
	QueueFamilyIndices indice;

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(devices, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamily(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(devices, &queueFamilyCount, queueFamily.data());

	int index = 0;

	for (const auto qfamily : queueFamily)
	{	
		if (qfamily.queueCount <= 0) continue;
		//队列支持图形处理命令
		if ((qfamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && qfamily.queueCount > 0)
		{
			indice.graphicsFamily = index;
		}

		//判定队列族编号对应的队列族下的队列是否支持将图像呈现到窗口上
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(devices, index, surface, &presentSupport);

		if (qfamily.queueCount > 0 && presentSupport)
		{
			indice.presentFamily = index;
		}

		if (indice.isComplete())
		{
			break;
		}
		index++;
	}
	//选择physical device时需要作为判定条件，不能强行终止程序
	//if (!indice.isComplete())
	//{
	//	throw std::runtime_error("No suitable queue .");
	//}

	return indice;
}

bool ShadowMap::isDeviceSuitable(VkPhysicalDevice devices)
{
	//VkPhysicalDeviceProperties property;
	//VkPhysicalDeviceFeatures feature;

	//vkGetPhysicalDeviceProperties(devices, &property);
	//vkGetPhysicalDeviceFeatures(devices, &feature);

	// 独显 + geometryShader
	//return (property.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) && feature.geometryShader;

	QueueFamilyIndices indice = findQueueFamilies(devices);

	bool extensionSupport = CheckDeviceExtensionSupport(devices);//swapchain
	bool swapChainAdequate = false;

	if (extensionSupport)
	{
		SwapChainSupportDetails swapChainDetails = querySwapChainSupport(devices);
		swapChainAdequate = !(swapChainDetails.formats.empty() || swapChainDetails.presentModes.empty());
	}
	
	return indice.isComplete() && extensionSupport && swapChainAdequate;
}

void ShadowMap::physicalDevice_pick(void)
{
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("failed to find physical device .");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	//选择策略：选取最先满足条件的physicaldevice
	for (const auto phydevice : devices)
	{
		if (isDeviceSuitable(phydevice))
		{
			physicalDevice = phydevice;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("no suitable device.");
	}
}

void ShadowMap::logicalDevice_create(void)
{
	QueueFamilyIndices indice = findQueueFamilies(physicalDevice);

	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = indice.graphicsFamily.value();
	queueCreateInfo.queueCount = 1;
	float priorities = 1.0f;
	queueCreateInfo.pQueuePriorities = &priorities;

	//创建逻辑设备时，我们需要指明要使用物理设备所支持的特性中的哪一些特性
	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.queueCreateInfoCount = 1;

	//在choose physical device 中已经检查过extension的支持
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requireExtensions.size());//static_cast<uint32_t>(requireExtensions.size());
	createInfo.ppEnabledExtensionNames = requireExtensions.data();

	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device!");
	}

	vkGetDeviceQueue(logicalDevice, indice.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(logicalDevice, indice.presentFamily.value(), 0, &presentQueue);
}

void ShadowMap::swapChain_create(void)
{
	SwapChainSupportDetails swapChainDetails = querySwapChainSupport(physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainDetails.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainDetails.presentModes);

	VkExtent2D swapExtent = chooseSwapExtent(swapChainDetails.capabilities);

	//swapExtent.height = max(swapChainDetails.capabilities.minImageExtent.height, min(swapChainDetails.capabilities.maxImageExtent.height, HEIGHT));
	//swapExtent.width = max(swapChainDetails.capabilities.minImageExtent.width, min(swapChainDetails.capabilities.maxImageExtent.width, WIDTH));

	uint32_t imageCount = swapChainDetails.capabilities.minImageCount + 1;

	if (swapChainDetails.capabilities.maxImageCount > 0 && imageCount > swapChainDetails.capabilities.maxImageCount)
	{
		imageCount = swapChainDetails.capabilities.maxImageCount;
	}

	//create swap chain part
	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount; //why add 1
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = swapExtent;
	createInfo.imageArrayLayers = 1;//specifies the amount of layers each image consists of
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	//图形渲染和image呈现在surface的队列族不一致：并发模式
	if (indices.presentFamily != indices.graphicsFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform = swapChainDetails.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

	swapChainExtent = swapExtent;
	swapChainImageFormat = surfaceFormat.format;
}

void ShadowMap::imageView_create(void)
{
	swapChainImageViews.resize(swapChainImages.size());

	//create an image view for each image
	for (int i = 0; i < swapChainImages.size(); i++)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainImageFormat;

		//这里选择默认，也可以通过将所有的通道映射到一个通道，实现黑白纹理
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		//表示用途，这里只做颜色使用，也可以做深度使用
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.layerCount = 1;
		createInfo.subresourceRange.levelCount = 1;

		if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapChainImageViews[i])!= VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image view.");
		}
	}
}

VkShaderModule ShadowMap::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module .");
	}

	return shaderModule;
}

void ShadowMap::renderPass_create(void)
{
	//offscreen render pass
	//1. attachment description
	VkAttachmentDescription depthAttachDesc{};
	depthAttachDesc.format = depthImageFormat;
	depthAttachDesc.samples = VK_SAMPLE_COUNT_1_BIT; //multi sample disable
	depthAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//2. subpass description
	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 0; //index of attachment
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription{};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;//作为图形管线
	subpassDescription.colorAttachmentCount = 0;
	subpassDescription.pColorAttachments = nullptr;
	subpassDescription.pDepthStencilAttachment = &depthAttachmentRef;

	//3. subpass dependency
	std::array<VkSubpassDependency, 2> dependency{};
	dependency[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency[0].dstSubpass = 0; //index of subpass
	dependency[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependency[0].dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	dependency[1].srcSubpass = 0;
	dependency[1].dstSubpass = VK_SUBPASS_EXTERNAL; //index of subpass
	dependency[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &depthAttachDesc;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependency.size());
	renderPassInfo.pDependencies = dependency.data();

	if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &offRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}

	//renderpass 2
	std::array<VkAttachmentDescription, 2> attachDesc{};
	attachDesc[0].format = swapChainImageFormat;
	attachDesc[0].samples = VK_SAMPLE_COUNT_1_BIT; //multi sample disable
	attachDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachDesc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachDesc[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	attachDesc[1].format = depthImageFormat;
	attachDesc[1].samples = VK_SAMPLE_COUNT_1_BIT; //multi sample disable
	attachDesc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachDesc[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachDesc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachDesc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachDesc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachDesc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0; //index of attachment
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef2{};
	depthAttachmentRef2.attachment = 1; //index of attachment
	depthAttachmentRef2.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription2{};
	subpassDescription2.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;//作为图形管线
	subpassDescription2.colorAttachmentCount = 1;
	subpassDescription2.pColorAttachments = &colorAttachmentRef;
	subpassDescription2.inputAttachmentCount = 0;
	subpassDescription2.pInputAttachments = nullptr;
	subpassDescription2.pDepthStencilAttachment = &depthAttachmentRef2;

	std::array<VkSubpassDependency, 2> dependency2{};
	dependency2[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency2[0].dstSubpass = 0; //index of subpass
	dependency2[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency2[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependency2[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency2[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	dependency2[1].srcSubpass = 0;
	dependency2[1].dstSubpass = VK_SUBPASS_EXTERNAL; //index of subpass
	dependency2[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency2[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency2[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency2[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachDesc.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription2;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependency2.size());
	renderPassInfo.pDependencies = dependency2.data();

	if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

uint32_t ShadowMap::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) 
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw  std::runtime_error("failed to find suitable memory type!");
}

void ShadowMap::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
}

void ShadowMap::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) 
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(commandBuffer);
}

void ShadowMap::copyBufferToImage(VkCommandBuffer& cb, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(
		cb,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);
}

void ShadowMap::createDescPool(void)
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};

	//pass0 + pass1
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2); 

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2); //pass0 + pass1
 
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2);

	if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void ShadowMap::createDescSets(void)
{
	std::vector<VkDescriptorSetLayout> layoutsPass0(MAX_FRAMES_IN_FLIGHT, descSetLayoutPass0);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layoutsPass0.data();

	descriptorSetsPass0.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSetsPass0.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	std::vector<VkDescriptorSetLayout> layoutsPass1(MAX_FRAMES_IN_FLIGHT, descSetLayoutPass1);
	allocInfo.pSetLayouts = layoutsPass1.data();

	descriptorSetsPass1.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSetsPass1.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		// pass0 + pass1
		//pass0: strip = sizeof(uniformBufferObject) * 2, offset = 0
		//pass1: strip = sizeof(uniformBufferObject) * 2, offset = sizeof(uniformBufferObject)
		VkDescriptorBufferInfo bufferInfoPass0{};
		bufferInfoPass0.buffer = uniformBuffers[i];
		bufferInfoPass0.offset = dynamicAlignment * 2 * i;
		bufferInfoPass0.range = sizeof(UniformBufferObject);

		std::array<VkWriteDescriptorSet, 1> descriptorWrites0{};
		descriptorWrites0[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites0[0].dstSet = descriptorSetsPass0[i];
		descriptorWrites0[0].dstBinding = 0;
		descriptorWrites0[0].dstArrayElement = 0;
		descriptorWrites0[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites0[0].descriptorCount = 1;
		descriptorWrites0[0].pBufferInfo = &bufferInfoPass0;
		descriptorWrites0[0].pImageInfo = nullptr; // Optional
		descriptorWrites0[0].pTexelBufferView = nullptr; // Optional


		vkUpdateDescriptorSets(logicalDevice, 1, descriptorWrites0.data(), 0, nullptr);

		VkDescriptorBufferInfo bufferInfoPass1{};
		bufferInfoPass1.buffer = uniformBuffers[i];
		bufferInfoPass1.offset = dynamicAlignment * 2 * i + dynamicAlignment;
		bufferInfoPass1.range = sizeof(UniformBufferObject);

		std::array<VkWriteDescriptorSet,3>  descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSetsPass1[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfoPass1;
		descriptorWrites[0].pImageInfo = nullptr; // Optional
		descriptorWrites[0].pTexelBufferView = nullptr; // Optional

		VkDescriptorImageInfo depthImageInfo{};
		depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		depthImageInfo.imageView = depthImageViews[i];
		depthImageInfo.sampler = depthImageSamplers[i];

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSetsPass1[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &depthImageInfo; // Optional

		VkDescriptorImageInfo texImageInfo{};
		texImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		texImageInfo.imageView = texImageViews;
		texImageInfo.sampler = texImageSamplers;

		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = descriptorSetsPass1[i];
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pImageInfo = &texImageInfo; // Optional

		vkUpdateDescriptorSets(logicalDevice, 3, descriptorWrites.data(), 0, nullptr);
	}
}

void ShadowMap::createDescSetLayout(void)
{
	std::array<VkDescriptorSetLayoutBinding, 1> layoutBindings{};
	layoutBindings[0].binding = 0;
	layoutBindings[0].descriptorCount = 1;
	layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBindings[0].pImmutableSamplers = nullptr;
	layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = layoutBindings.data();

	if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descSetLayoutPass0) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphic pass0 descriptor set layout!");
	}
	std::array<VkDescriptorSetLayoutBinding, 3> layoutBindingsPass1{};
	layoutBindingsPass1[0].binding = 0;
	layoutBindingsPass1[0].descriptorCount = 1;
	layoutBindingsPass1[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBindingsPass1[0].pImmutableSamplers = nullptr;
	layoutBindingsPass1[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//depth attachment
	layoutBindingsPass1[1].binding = 1;
	layoutBindingsPass1[1].descriptorCount = 1;
	layoutBindingsPass1[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBindingsPass1[1].pImmutableSamplers = nullptr;
	layoutBindingsPass1[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	layoutBindingsPass1[2].binding = 2;
	layoutBindingsPass1[2].descriptorCount = 1;
	layoutBindingsPass1[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBindingsPass1[2].pImmutableSamplers = nullptr;
	layoutBindingsPass1[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	//layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	VkDescriptorSetLayoutCreateInfo layoutInfoPass1{};
	layoutInfoPass1.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfoPass1.bindingCount = 3;
	layoutInfoPass1.pBindings = layoutBindingsPass1.data();

	if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfoPass1, nullptr, &descSetLayoutPass1) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphic pass1 descriptor set layout!");
	}

}

void ShadowMap::graphicsPiplinePass0_create(void)
{
	auto vertexShaderCode = readFile("E:/2_GITHUB/Vulkan-Code/ShadowMap/src/spvs/pass0_vert.spv");
	auto fragShaderCode = readFile("E:/2_GITHUB/Vulkan-Code/ShadowMap/src/spvs/pass0_frag.spv");

	VkShaderModule vertShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main"; // entry points

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragmentShaderModule;
	fragShaderStageInfo.pName = "main";

	//step 1:prepare Shader Stage
	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
	
	//step 2:prepare Vertex Input State
	auto bindingDesc = Vertex::getBindingDescription();
	auto attributeDesc = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDesc;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 3;
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDesc.data();

	//step 3:input assembly 
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	//step 4:Viewports and scissors
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.extent = swapChainExtent;//show all
	scissor.offset = { 0, 0 };

	VkPipelineViewportStateCreateInfo viewportCreateInfo{};
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = &viewport;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = &scissor;

	//step 5:Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterCreateInfo{};
	rasterCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterCreateInfo.depthClampEnable = VK_FALSE;
	rasterCreateInfo.rasterizerDiscardEnable = VK_FALSE;//★
	rasterCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterCreateInfo.lineWidth = 1.0f;
	rasterCreateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterCreateInfo.depthBiasEnable = VK_FALSE;

	//step 5:Multisample 
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo{};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	//step 6:Depth and stencil testing 
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;

	//step 7:Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendState{};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_COPY;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachment;
	colorBlendState.blendConstants[0] = 0.0f;
	colorBlendState.blendConstants[1] = 0.0f;
	colorBlendState.blendConstants[2] = 0.0f;
	colorBlendState.blendConstants[3] = 0.0f;

	//step 8:Dynamic state 
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
 
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	//step 9:Pipeline layout 
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descSetLayoutPass0;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	         
	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, NULL, &pipelineLayoutPass0) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline . ");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;//可编程的stage number
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineInfo.pViewportState = &viewportCreateInfo;
	pipelineInfo.pRasterizationState = &rasterCreateInfo;
	pipelineInfo.pMultisampleState = &multisampleCreateInfo;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlendState;
	pipelineInfo.pDynamicState = &dynamicState;// &dynamicState; //optional
	pipelineInfo.layout = pipelineLayoutPass0;
	pipelineInfo.renderPass = offRenderPass;
	pipelineInfo.subpass = 0;//index of used subpass

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(logicalDevice, nullptr, 1, &pipelineInfo, nullptr, &pipelinePass_0))
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	//destroy the shader modules again as soon as pipeline creation is finished
	vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);
}

void ShadowMap::graphicsPiplinePass1_create(void)
{
	auto vertexShaderCode = readFile("E:/2_GITHUB/Vulkan-Code/ShadowMap/src/spvs/pass1_vert.spv");
	auto fragShaderCode = readFile("E:/2_GITHUB/Vulkan-Code/ShadowMap/src/spvs/pass1_frag.spv");

	VkShaderModule vertShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main"; // entry points

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragmentShaderModule;
	fragShaderStageInfo.pName = "main";

	//step 1:prepare Shader Stage
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	//step 2:prepare Vertex Input State
	auto bindingDesc = Vertex::getBindingDescription();
	auto attributeDesc = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDesc;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 3;
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDesc.data();

	//step 3:input assembly 
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	//step 4:Viewports and scissors
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.extent = swapChainExtent;//show all
	scissor.offset = { 0, 0 };

	VkPipelineViewportStateCreateInfo viewportCreateInfo{};
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = &viewport;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = &scissor;

	//step 5:Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterCreateInfo{};
	rasterCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterCreateInfo.depthClampEnable = VK_FALSE;
	rasterCreateInfo.rasterizerDiscardEnable = VK_FALSE;//★
	rasterCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterCreateInfo.lineWidth = 1.0f;
	rasterCreateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterCreateInfo.depthBiasEnable = VK_FALSE;

	//step 5:Multisample 
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo{};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	//step 6:Depth and stencil testing 
	// //step 6:Depth and stencil testing 
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;

	//step 7:Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendState{};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_COPY;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachment;
	colorBlendState.blendConstants[0] = 0.0f;
	colorBlendState.blendConstants[1] = 0.0f;
	colorBlendState.blendConstants[2] = 0.0f;
	colorBlendState.blendConstants[3] = 0.0f;

	//step 8:Dynamic state 
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	//step 9:Pipeline layout 
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descSetLayoutPass1;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, NULL, &pipelineLayoutPass1) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline . ");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;//可编程的stage number
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineInfo.pViewportState = &viewportCreateInfo;
	pipelineInfo.pRasterizationState = &rasterCreateInfo;
	pipelineInfo.pMultisampleState = &multisampleCreateInfo;
	pipelineInfo.pDepthStencilState = &depthStencil;//optional
	pipelineInfo.pColorBlendState = &colorBlendState;
	pipelineInfo.pDynamicState = &dynamicState;// &dynamicState; //optional
	pipelineInfo.layout = pipelineLayoutPass1;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;//index of used subpass

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(logicalDevice, nullptr, 1, &pipelineInfo, nullptr, &pipelinePass_1))
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	//destroy the shader modules again as soon as pipeline creation is finished
	vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);
}

void ShadowMap::uniformBuffer_create(void)
{
	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBufferMems.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	size_t minUboAlignment = properties.limits.minUniformBufferOffsetAlignment;
	dynamicAlignment = sizeof(UniformBufferObject);
	if (minUboAlignment > 0) {
		dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	VkDeviceSize bufferSize = 2 * dynamicAlignment;
	// pass0 + pass1
	//pass0: strip = sizeof(uniformBufferObject) * 2, offset = 0
	//pass1: strip = sizeof(uniformBufferObject) * 2, offset = sizeof(uniformBufferObject)
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBufferMems[i]);
		vkMapMemory(logicalDevice, uniformBufferMems[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
	}
}

void ShadowMap::vertexAndIndiceBuffer_create(void)
{
	Object* ob = new Object("E:/2_GITHUB/Vulkan-Code/ShadowMap/data/room.obj");
	auto verticesIn = ob->getVertices();
	auto indices = ob->getIndices();

	if (verticesIn.size() == 0 || indices.size() == 0)
	{
		std::runtime_error("not have vertices or indices data");
		return;
	}

	indicesNum = indices.size();
	//Vertex buffer
	auto bufferSize = sizeof(verticesIn[0]) * verticesIn.size();
	createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vertexBuffer, vertexBufferMem);

	//load data
	void* data;
	vkMapMemory(logicalDevice, vertexBufferMem, 0, bufferSize, 0, &data);
	memcpy(data, verticesIn.data(), bufferSize);
	vkUnmapMemory(logicalDevice, vertexBufferMem);

	bufferSize = sizeof(uint16_t) * indices.size();
	createBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, indiceBuffer, indiceBufferMem);

	vkMapMemory(logicalDevice, indiceBufferMem, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), bufferSize);
	vkUnmapMemory(logicalDevice, indiceBufferMem);

	delete ob;
	return;
}

void ShadowMap::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectMask, VkImageView& imageView) {
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectMask; //VK_IMAGE_ASPECT_COLOR_BIT
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(logicalDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}
}

void ShadowMap::createImageSampler(VkSampler &sampler)
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_FALSE; //need enable device feature

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void ShadowMap::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
	VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	//Vk图像对象创建
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;      //Vulkan支持多种图像格式，但无论如何我们要在缓冲区中为纹素应用与像素一致的格式，否则拷贝操作会失败。(VK_FORMAT_R8G8B8A8_SRGB)
	imageInfo.tiling = tiling;
	/*
		VK_IMAGE_TILING_LINEARpixels：纹素像我们的数组一样按行主序排列
		VK_IMAGE_TILING_OPTIMAL：纹理元素按照实现定义的顺序进行布局，以实现最佳访问
	*/
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	// 为图像分配内存
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(logicalDevice, image, imageMemory, 0);
}

void ShadowMap::framebuffer_create(void)
{
	framebuffers.resize(swapChainImageViews.size());
	offFramebuffers.resize(swapChainImageViews.size());

	for (int i = 0; i < swapChainImageViews.size(); i++)
	{
		//specify the VkImageView objects that should be bound to the respective
		//attachment descriptions in the render pass pAttachment array
		VkImageView offAttachments[] = {depthImageViews[i]};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = offRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = offAttachments;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &offFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer!");
		}

		VkImageView attachments[] = { swapChainImageViews[i], depthImageViewsPass1[i]};

		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void ShadowMap::commandPool_create(void)
{
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional  neither

	if (vkCreateCommandPool(logicalDevice, &poolCreateInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}

}

void ShadowMap::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	VkRenderPassBeginInfo renderPassInfo0{};
	renderPassInfo0.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo0.renderPass = offRenderPass;
	renderPassInfo0.framebuffer = offFramebuffers[imageIndex];

	renderPassInfo0.renderArea.offset = { 0,0 };
	renderPassInfo0.renderArea.extent = swapChainExtent;

	std::array<VkClearValue, 1> offClearValues{};
	offClearValues[0].depthStencil = { 1.0f, 0 };

	renderPassInfo0.clearValueCount = static_cast<uint32_t>(offClearValues.size());
	renderPassInfo0.pClearValues = offClearValues.data();

	//The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo0, VK_SUBPASS_CONTENTS_INLINE);

	//The second parameter specifies if the pipeline object is a graphics or compute pipeline. 
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinePass_0);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	VkBuffer vertexBuffers[] = { vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indiceBuffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutPass0, 0, 1, &descriptorSetsPass0[currentFrame], 0, nullptr);
	vkCmdDrawIndexed(commandBuffer, indicesNum, 1, 0, 0, 0);
	vkCmdEndRenderPass(commandBuffer);

	transitionImageLayout(commandBuffer, depthImages[imageIndex], depthImageFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

	VkRenderPassBeginInfo renderPassInfo1{};
	renderPassInfo1.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo1.renderPass = renderPass;
	renderPassInfo1.framebuffer = framebuffers[imageIndex];
	renderPassInfo1.renderArea.offset = { 0,0 };
	renderPassInfo1.renderArea.extent = swapChainExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = {0.0, 0.0, 0.0, 1.0};
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo1.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo1.pClearValues = clearValues.data();

	//The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo1, VK_SUBPASS_CONTENTS_INLINE);

	//The second parameter specifies if the pipeline object is a graphics or compute pipeline. 
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinePass_1);

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indiceBuffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutPass1, 0, 1, &descriptorSetsPass1[currentFrame], 0, nullptr);
	vkCmdDrawIndexed(commandBuffer, indicesNum, 1, 0, 0, 0);
	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record command buffer!");
	}
}

void ShadowMap::commondBuffers_create(void)
{
	//command buffer的数量和framebuffersframe buffer的数量一致
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}
}

void ShadowMap::syncObjects_create(void)
{
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{}; 
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;//initial state of fence = signed

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS||
			vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}

}

// get basic info of swap chain
//only query
SwapChainSupportDetails ShadowMap::querySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails swapChainDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &(swapChainDetails.capabilities));

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0)
	{
		swapChainDetails.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, swapChainDetails.presentModes.data());
	}
	return swapChainDetails;
}

VkSurfaceFormatKHR ShadowMap::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	for (const auto& format : formats)
	{
		if (format.format == VK_FORMAT_B8G8R8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return format;
	}
	return formats[0];
}

//目前有的driver不支持VK_PRESENT_MODE_FIFO_KHR，所以当VK_PRESENT_MODE_MAILBOX_KHR不可用时，
//我们倾向于VK_PRESENT_MODE_IMMEDIATE_KHR
VkPresentModeKHR ShadowMap::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
{
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto& mode : presentModes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return mode;
		else if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			bestMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}


VkExtent2D ShadowMap::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = static_cast<uint32_t>(std::fmax(capabilities.minImageExtent.width, std::fmin(capabilities.maxImageExtent.width, actualExtent.width)));
		actualExtent.height = static_cast<uint32_t>(std::fmax(capabilities.minImageExtent.height, std::fmin(capabilities.maxImageExtent.height, actualExtent.height)));

		return actualExtent;
	}
}


bool ShadowMap::CheckValidationLayerSupport(void)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> layerProperties(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

	std::cout << "all supported layer names" << "\n";
	for (const char* layerName : validationLayers)
	{
		bool layFound = false;
		for (const auto& layerProperties : layerProperties)
		{
			//★print out all names of supported layer 
			std::cout << layerProperties.layerName << "\n";
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layFound = true;
				break;
			}
		}
		if (!layFound)
			return false;
	}
	return true;
}

bool ShadowMap::CheckDeviceExtensionSupport(VkPhysicalDevice devices)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(devices, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtension(extensionCount);
	vkEnumerateDeviceExtensionProperties(devices, nullptr, &extensionCount, availableExtension.data());

	std::set<std::string> deviceExtensions(requireExtensions.begin(), requireExtensions.end());

	for(const auto& extension : availableExtension)
	{
		deviceExtensions.erase(extension.extensionName);
	}

	//作为选择physical device的判定条件之一，不需要抛出程序错误，强制中断程序
	//if (!deviceExtensions.empty()) {
	//	throw std::runtime_error("extension not fulfill");
	//}

	return deviceExtensions.empty();
}

//glfw + layers information call back
std::vector<const char*> ShadowMap::getRequiredExtensions(void)
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensionCount + glfwExtensions);

	if (enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	//★print all required extensions
	std::cout << "all required extensions" << "\n";
	for (const auto extension : extensions)
		std::cout << extension << "\n";

	return extensions;
}

void ShadowMap::processInput(GLFWwindow *window) {
	//帧间隔长，就让它移动的多一些，这样能间接保证速度
	float cameraSpeed = 4.f * deltaTime;
	//按下esc键的意思
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);//关闭窗户
	}
	else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		translateMat = glm::translate(translateMat, vec3(0, 0, -1.0) * cameraSpeed);
		//auto translate = cameraUp* cameraSpeed;
		//cameraPos += translate;
	}
	else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		translateMat = glm::translate(translateMat, vec3(0, 0, 1.0) * cameraSpeed);
		//auto translate = cameraUp * cameraSpeed;
		//cameraPos -= translate;
	}
	else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		//glm::normalize(glm::cross(cameraFront, cameraUp))这个求的是标准化的右向量
		translateMat = glm::translate(translateMat, vec3(-1, 0, 0) * cameraSpeed);
	}
	else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		translateMat = glm::translate(translateMat, vec3(1, 0, 0) * cameraSpeed);
	}
	else if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
	{
		//auto origin = cameraFront + cameraPos;
		//auto rotate = glm::rotate(glm::mat4(1.0f), -glm::radians(cameraSpeed * 5), cameraUp);
		//auto front = (rotate * glm::vec4(cameraFront, 0.0));
		//cameraFront = glm::vec3(front.x, front.y, front.z);
		//cameraPos = origin - cameraFront;
		rotateMat = glm::rotate(rotateMat, -glm::radians(cameraSpeed * 5), vec3(0, 1, 0));
	}

	else if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
	{
		//auto origin = cameraFront + cameraPos;
		//auto rotate = glm::rotate(glm::mat4(1.0f), glm::radians(cameraSpeed * 5), cameraUp);
		//auto front = (rotate * glm::vec4(cameraFront, 0.0));
		//cameraFront = glm::vec3(front.x, front.y, front.z);
		//cameraPos = origin - cameraFront;
		rotateMat = glm::rotate(rotateMat, glm::radians(cameraSpeed * 5), vec3(0, 1, 0));
	}
	else if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
	{
		//auto translate = cameraFront * cameraSpeed;
		//cameraPos += translate;
		rotateMat = glm::rotate(rotateMat, glm::radians(cameraSpeed * 5), vec3(0, 0, 1));
	}
	else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
	{
		//auto translate = cameraFront * cameraSpeed;
		//cameraPos -= translate;
		rotateMat = glm::rotate(rotateMat, -glm::radians(cameraSpeed * 5), vec3(0, 0, 1));
	}
}

VkCommandBuffer ShadowMap::beginSingleTimeCommands() {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void ShadowMap::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}