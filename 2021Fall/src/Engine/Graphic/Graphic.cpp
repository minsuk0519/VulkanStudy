#include "Graphic.hpp"
#include "Engine/settings.hpp"
#include "Engine/Application.hpp"
#include "VertexInfo.hpp"

//standard library
#include <stdexcept>

//3rd party library
#define GLM_FORCE_RADIANS
#include <vulkan/vulkan.h>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

Graphic::Graphic(VkDevice device, Application* app) : System(device, app) {}

void Graphic::init()
{
    SetupSwapChain();

    //create vertex buffer
    {
        std::vector<PosColorTexVertex> vert = {
            {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}
        };

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        VkDeviceSize bufferSize = sizeof(vert[0]) * vert.size();
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(vulkanDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vert.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(vulkanDevice, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanVertexBuffer, vulkanVertexBufferMemory);

        copyBuffer(stagingBuffer, vulkanVertexBuffer, bufferSize);

        vkDestroyBuffer(vulkanDevice, stagingBuffer, nullptr);
        vkFreeMemory(vulkanDevice, stagingBufferMemory, nullptr);
    }

    //create index buffer
    {
        const std::vector<uint16_t> indices = {
            0, 1, 2, 2, 3, 0
        };

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(vulkanDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(vulkanDevice, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanIndexBuffer, vulkanIndexBufferMemory);

        copyBuffer(stagingBuffer, vulkanIndexBuffer, bufferSize);

        vkDestroyBuffer(vulkanDevice, stagingBuffer, nullptr);
        vkFreeMemory(vulkanDevice, stagingBufferMemory, nullptr);
    }

    //create uniform buffer
    {
        VkDeviceSize bufferSize = sizeof(transform);

        vulkanUniformBuffers.resize(vulkanSwapChainImages.size());
        vulkanUniformBuffersMemory.resize(vulkanSwapChainImages.size());

        for (size_t i = 0; i < vulkanSwapChainImages.size(); ++i)
        {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vulkanUniformBuffers[i], vulkanUniformBuffersMemory[i]);
        }
    }

    //create texture image
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("data/textures/texture.png", &texWidth,
            &texHeight, &texChannels, STBI_rgb_alpha);

        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels)
        {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(vulkanDevice, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(vulkanDevice, stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanTextureImage, vulkanTextureImageMemory);

        transitionImageLayout(vulkanTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, vulkanTextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        transitionImageLayout(vulkanTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(vulkanDevice, stagingBuffer, nullptr);
        vkFreeMemory(vulkanDevice, stagingBufferMemory, nullptr);

        vulkanTextureImageView = createImageView(vulkanTextureImage, VK_FORMAT_R8G8B8A8_SRGB);
    }

    //descriptorpool
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(vulkanSwapChainImages.size());
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(vulkanSwapChainImages.size());

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(vulkanSwapChainImages.size());

        if (vkCreateDescriptorPool(vulkanDevice, &poolInfo, nullptr, &vulkanDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    //sampler
    {
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(vulkanDevice, &samplerInfo, nullptr, &vulkanTextureSampler))
        {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    //descriptor
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(vulkanDevice, &layoutInfo, nullptr, &vulkanDescriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        std::vector<VkDescriptorSetLayout> layouts(vulkanSwapChainImages.size(), vulkanDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = vulkanDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(vulkanSwapChainImages.size());
        allocInfo.pSetLayouts = layouts.data();

        vulkanDescriptorSets.resize(vulkanSwapChainImages.size());
        if (vkAllocateDescriptorSets(vulkanDevice, &allocInfo, vulkanDescriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < vulkanSwapChainImages.size(); ++i)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = vulkanUniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(transform);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = vulkanTextureImageView;
            imageInfo.sampler = vulkanTextureSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = vulkanDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;
            descriptorWrites[0].pImageInfo = nullptr;
            descriptorWrites[0].pTexelBufferView = nullptr;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = vulkanDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = nullptr;
            descriptorWrites[1].pImageInfo = &imageInfo;
            descriptorWrites[1].pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(vulkanDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    DefineDrawBehavior();

    //create semaphore
    {
        vulkanImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        vulkanRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(vulkanSwapChainImages.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (vkCreateSemaphore(vulkanDevice, &semaphoreInfo, nullptr,
                    &vulkanImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vulkanDevice, &semaphoreInfo, nullptr,
                    &vulkanRenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vulkanDevice, &fenceInfo, nullptr,
                    &inFlightFences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create semaphores!");
            }
        }
    }
}

void Graphic::update(float /*dt*/)
{
    vkWaitForFences(vulkanDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(vulkanDevice, vulkanSwapChain,
        UINT64_MAX, vulkanImageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        vkWaitForFences(vulkanDevice, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    //update uniform buffer
    {
        static float time = 0; 
        time += 0.0001f;

        transform ubo{};
        ubo.objectMat = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.worldToCamera = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.cameraToNDC = glm::perspective(glm::radians(45.0f), Settings::GetAspectRatio(), 0.1f, 10.0f);
        ubo.cameraToNDC[1][1] *= -1;

        void* data;
        vkMapMemory(vulkanDevice, vulkanUniformBuffersMemory[imageIndex], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(vulkanDevice, vulkanUniformBuffersMemory[imageIndex]);
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { vulkanImageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vulkanCommandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = { vulkanRenderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(vulkanDevice, 1, &inFlightFences[currentFrame]);

    if (vkQueueSubmit(application->GetGraphicQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { vulkanSwapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(application->GetPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || application->framebufferSizeUpdate)
    {
        application->framebufferSizeUpdate = false;
        RecreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }
     
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Graphic::close()
{
    vkDestroyBuffer(vulkanDevice, vulkanIndexBuffer, nullptr);
    vkFreeMemory(vulkanDevice, vulkanIndexBufferMemory, nullptr);

    vkDestroyBuffer(vulkanDevice, vulkanVertexBuffer, nullptr);
    vkFreeMemory(vulkanDevice, vulkanVertexBufferMemory, nullptr);

    for (size_t i = 0; i < vulkanSwapChainImages.size(); ++i)
    {
        vkDestroyBuffer(vulkanDevice, vulkanUniformBuffers[i], nullptr);
        vkFreeMemory(vulkanDevice, vulkanUniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorSetLayout(vulkanDevice, vulkanDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(vulkanDevice, vulkanDescriptorPool, nullptr);

    vkDestroySampler(vulkanDevice, vulkanTextureSampler, nullptr);
    vkDestroyImageView(vulkanDevice, vulkanTextureImageView, nullptr);

    vkDestroyImage(vulkanDevice, vulkanTextureImage, nullptr);
    vkFreeMemory(vulkanDevice, vulkanTextureImageMemory, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(vulkanDevice, vulkanRenderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(vulkanDevice, vulkanImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(vulkanDevice, inFlightFences[i], nullptr);
    }

    CloseSwapChain();
}

Graphic::~Graphic() {}

void Graphic::SetupSwapChain()
{
    //create swap chain
    {
        uint32_t imageCount;
        vulkanSwapChain = application->CreateSwapChain(imageCount, vulkanSwapChainImageFormat, vulkanSwapChainExtent);

        vkGetSwapchainImagesKHR(vulkanDevice, vulkanSwapChain, &imageCount, nullptr);
        vulkanSwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(vulkanDevice, vulkanSwapChain, &imageCount, vulkanSwapChainImages.data());
    }

    //image views
    {
        vulkanSwapChainImageViews.resize(vulkanSwapChainImages.size());

        for (size_t i = 0; i < vulkanSwapChainImages.size(); i++)
        {
            vulkanSwapChainImageViews[i] = createImageView(vulkanSwapChainImages[i], vulkanSwapChainImageFormat);
        }
    }

    //create renderpass
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = vulkanSwapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(vulkanDevice, &renderPassInfo, nullptr, &vulkanRenderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    
}

void Graphic::CloseSwapChain()
{
    for (auto frameBuffer : vulkanSwapChainFramebuffers)
    {
        vkDestroyFramebuffer(vulkanDevice, frameBuffer, nullptr);
    }

    vkFreeCommandBuffers(vulkanDevice, application->GetCommandPool(), static_cast<uint32_t>(vulkanCommandBuffers.size()), vulkanCommandBuffers.data());

    graphicPipeline->close();
    delete graphicPipeline;

    vkDestroyRenderPass(vulkanDevice, vulkanRenderPass, nullptr);

    for (auto imageView : vulkanSwapChainImageViews)
    {
        vkDestroyImageView(vulkanDevice, imageView, nullptr);
    }

    vkDestroySwapchainKHR(vulkanDevice, vulkanSwapChain, nullptr);
}

void Graphic::RecreateSwapChain()
{
    while (Settings::windowWidth == 0 || Settings::windowHeight == 0)
    {
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(vulkanDevice);

    CloseSwapChain();
    SetupSwapChain();
    DefineDrawBehavior();
}

void Graphic::DefineDrawBehavior()
{
    //create graphic pipeline
    {
        graphicPipeline = new GraphicPipeline(vulkanDevice);
        graphicPipeline->init(vulkanRenderPass, vulkanDescriptorSetLayout);
    }

    //create framebuffer
    {
        vulkanSwapChainFramebuffers.resize(vulkanSwapChainImageViews.size());

        for (size_t i = 0; i < vulkanSwapChainImageViews.size(); ++i)
        {
            VkImageView attachments[] = {
                vulkanSwapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = vulkanRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = Settings::windowWidth;
            framebufferInfo.height = Settings::windowHeight;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(vulkanDevice, &framebufferInfo, nullptr, &vulkanSwapChainFramebuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    //create command buffer
    {
        vulkanCommandBuffers.resize(vulkanSwapChainFramebuffers.size());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = application->GetCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)vulkanCommandBuffers.size();

        if (vkAllocateCommandBuffers(vulkanDevice, &allocInfo, vulkanCommandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        for (size_t i = 0; i < vulkanCommandBuffers.size(); ++i)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;
            beginInfo.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(vulkanCommandBuffers[i], &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = vulkanRenderPass;
            renderPassInfo.framebuffer = vulkanSwapChainFramebuffers[i];

            renderPassInfo.renderArea.offset = { 0,0 };
            renderPassInfo.renderArea.extent = { Settings::windowWidth, Settings::windowHeight };

            VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(vulkanCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(vulkanCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                graphicPipeline->GetPipeline());

            VkBuffer vertexBuffers[] = { vulkanVertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(vulkanCommandBuffers[i], 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(vulkanCommandBuffers[i], vulkanIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

            vkCmdBindDescriptorSets(vulkanCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipeline->GetPipelinLayout(),
                0, 1, &vulkanDescriptorSets[i], 0, nullptr);

            vkCmdDrawIndexed(vulkanCommandBuffers[i], 6, 1, 0, 0, 0);

            vkCmdEndRenderPass(vulkanCommandBuffers[i]);

            if (vkEndCommandBuffer(vulkanCommandBuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to recored command buffer!");
            }
        }
    }
}

void Graphic::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vulkanDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create vertex buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vulkanDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vulkanDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    vkBindBufferMemory(vulkanDevice, buffer, bufferMemory, 0);
}

void Graphic::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

uint32_t Graphic::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(application->GetPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type");
}

void Graphic::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    if (vkCreateImage(vulkanDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vulkanDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vulkanDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(vulkanDevice, image, imageMemory, 0);
}

VkCommandBuffer Graphic::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = application->GetCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vulkanDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Graphic::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(application->GetGraphicQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(application->GetGraphicQueue());

    vkFreeCommandBuffers(vulkanDevice, application->GetCommandPool(), 1, &commandBuffer);
}

void Graphic::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 
        0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

void Graphic::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0,0,0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

VkImageView Graphic::createImageView(VkImage image, VkFormat format)
{
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView result;

    if (vkCreateImageView(vulkanDevice, &createInfo, nullptr, &result) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image views!");
    }

    return result;
}
