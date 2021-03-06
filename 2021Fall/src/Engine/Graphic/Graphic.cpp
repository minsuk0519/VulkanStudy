#include "Graphic.hpp"
#include "Engine/Misc/settings.hpp"
#include "Engine/Common/Application.hpp"
#include "VertexInfo.hpp"
#include "Renderpass.hpp"
#include "DescriptorSet.hpp"
#include "Engine/Memory/Buffer.hpp"
#include "Engine/Memory/Image.hpp"
#include "Engine/Entity/Camera.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Entity/Light.hpp"
#include "Engine/Entity/Object.hpp"
#include "Engine/Graphic/Descriptor.hpp"
#include "Engine/Level/LevelManager.hpp"

//standard library
#include <stdexcept>
#include <unordered_map>
#include <iostream>

//3rd party library
#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

#define INSTANCE_COUNT 1

Graphic::Graphic(VkDevice device, Application* app) : System(device, app, "Graphic") {}

void Graphic::init()
{
    VulkanMemoryManager::Init(vulkanDevice);

    SetupSwapChain();

    descriptorManager = new DescriptorManager(vulkanDevice);
    descriptorManager->init();

    //uniform
    {
        VkDeviceSize bufferSize = sizeof(Cameratransform);
        uint32_t uniform = VulkanMemoryManager::CreateUniformBuffer(UNIFORM_CAMERA_TRANSFORM, bufferSize);

        bufferSize = 128;// sizeof(ObjectUniform);
        const uint32_t MAX_OBJECT_SIZE = 20;
        uniform = VulkanMemoryManager::CreateUniformBuffer(UNIFORM_OBJECT_MATRIX, bufferSize, MAX_OBJECT_SIZE);

        bufferSize = 128;
        uniform = VulkanMemoryManager::CreateUniformBuffer(UNIFORM_LIGHT_OBJECT_MATRIX, bufferSize, MAX_LIGHT);

        bufferSize = sizeof(GUISetting);
        uniform = VulkanMemoryManager::CreateUniformBuffer(UNIFORM_GUI_SETTING, bufferSize);

        bufferSize = MAX_LIGHT * LIGHTDATA_ALLIGNMENT + sizeof(int);
        uniform = VulkanMemoryManager::CreateUniformBuffer(UNIFORM_LIGHTDATA, bufferSize);

        bufferSize = LIGHTPROJ_ALLIGNMENT;// sizeof(LightProj);
        uniform = VulkanMemoryManager::CreateUniformBuffer(UNIFORM_LIGHTPROJ, bufferSize, MAX_LIGHT);
    }

    //make quad
    {
        std::vector<PosTexVertex> vert = {
            {{-1.0f, -1.0f}, {0.0f, 0.0f}},
            {{ 1.0f, -1.0f}, {1.0f, 0.0f}},
            {{-1.0f,  1.0f}, {0.0f, 1.0f}},
            {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
        };

        std::vector<uint32_t> indices = {
            0, 1, 2, 1, 3, 2,
        };

        uint32_t vertex = VulkanMemoryManager::CreateVertexBuffer(vert.data(), vert.size() * sizeof(PosTexVertex));
        uint32_t index = VulkanMemoryManager::CreateIndexBuffer(indices.data(), indices.size() * sizeof(uint32_t));

        drawtargets.push_back({ {{vertex, index, 6}} });
    }

    //create vertex & index buffer
    {
        std::vector<PosNormal> vert;
        std::vector<uint32_t> indices;

        std::vector<tinyobj::shape_t> shapes;
        tinyobj::attrib_t attrib;

        //loadModel(attrib, shapes, "data/models/bmw/", "bmw.obj");
        //loadModel(attrib, shapes, "data/models/dragon/", "dragon.obj");
        loadModel(attrib, shapes, "data/models/bunny/", "bunny.obj");

        std::unordered_map<PosNormal, uint32_t> uniqueVertices{};

        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                PosNormal vertex{};

                vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };

                //remove duplicate vertices
                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vert.size());
                    vert.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }

        //vertex
        size_t vertexbuffermemorysize = vert.size() * sizeof(PosNormal);
        uint32_t vertex = VulkanMemoryManager::CreateVertexBuffer(vert.data(), vertexbuffermemorysize);

        //index
        size_t indexbuffermemorysize = indices.size() * sizeof(uint32_t);
        uint32_t index = VulkanMemoryManager::CreateIndexBuffer(indices.data(), indexbuffermemorysize);

        std::vector<glm::vec3> transform_matrices;
        transform_matrices.reserve(INSTANCE_COUNT);
        float scale = 1.5f;
        int midpoint = 13;
        for (int i = 0; i < INSTANCE_COUNT; ++i)
        {
            glm::vec3 vec = glm::vec3(3.0f - scale * (i / midpoint), 0.0f, scale * 2 * (i % midpoint));
            //vec = glm::vec3(0.0f, 0.0f, 0.0f);
            transform_matrices.push_back(vec);
        }

        size_t instance_size = transform_matrices.size() * sizeof(glm::vec3);
        uint32_t instance = VulkanMemoryManager::CreateVertexBuffer(transform_matrices.data(), instance_size);

        drawtargets.push_back({ {{vertex, index, static_cast<uint32_t>(indices.size())}}, instance, INSTANCE_COUNT });

        drawtargets.push_back({ {{vertex, index, static_cast<uint32_t>(indices.size())}} });
    }

    {
        std::vector<PosNormal> vert = {
            {{-1.0f, -1.0f, -1.0f},{0.0f, 0.0f, -1.0f}},
            {{-1.0f, 1.0f, -1.0f},{0.0f, 0.0f, -1.0f}},
            {{1.0f, -1.0f, -1.0f},{0.0f, 0.0f, -1.0f}},
            {{1.0f, 1.0f, -1.0f},{0.0f, 0.0f, -1.0f}},

            {{-1.0f, -1.0f, 1.0f},{0.0f, 0.0f, 1.0f}},
            {{1.0f, -1.0f, 1.0f},{0.0f, 0.0f, 1.0f}},
            {{-1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 1.0f}},
            
            {{-1.0f, -1.0f, -1.0f},{0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f},{0.0f, -1.0f, 0.0f}},
            {{-1.0f, -1.0f, 1.0f},{0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, 1.0f},{0.0f, -1.0f, 0.0f}},
            
            {{-1.0f, 1.0f, -1.0f},{0.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f},{0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f},{0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f},{0.0f, 1.0f, 0.0f}},
            
            {{-1.0f, -1.0f, -1.0f},{-1.0f, 0.0f, 0.0f}},
            {{-1.0f, -1.0f, 1.0f},{-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f},{-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f},{-1.0f, 0.0f, 0.0f}},
            
            {{1.0f, -1.0f, -1.0f},{1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f},{1.0f, 0.0f, 0.0f}},
            {{1.0f, -1.0f, 1.0f},{1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f},{1.0f, 0.0f, 0.0f}},
        };

        std::vector<uint32_t> indices = {
            0, 1, 2,
            1, 3, 2,

            4, 5, 6,
            5, 7, 6,

            8, 9, 10,
            9, 11, 10,

            12, 13, 14,
            13, 15, 14,

            16, 17, 18,
            17, 19, 18,

            20, 21, 22,
            21, 23, 22,
        };

        size_t vertexbuffermemorysize = vert.size() * sizeof(PosNormal);
        uint32_t vertex = VulkanMemoryManager::CreateVertexBuffer(vert.data(), vertexbuffermemorysize);

        //index
        size_t indexbuffermemorysize = indices.size() * sizeof(uint32_t);
        uint32_t index = VulkanMemoryManager::CreateIndexBuffer(indices.data(), indexbuffermemorysize);

        glm::vec3 zero = glm::vec3(0, 0, 0);
        size_t instance_size = sizeof(glm::vec3);
        uint32_t instance = VulkanMemoryManager::CreateVertexBuffer(&zero, instance_size);

        drawtargets.push_back({ {{vertex, index, static_cast<uint32_t>(indices.size())}}, instance, 1 });
    }

    //create texture image
    {
        //int texWidth, texHeight, texChannels;
        //stbi_uc* pixels = stbi_load("data/models/viking_room/viking_room.png", &texWidth,
        //    &texHeight, &texChannels, STBI_rgb_alpha);

        //if (!pixels)
        //{
        //    throw std::runtime_error("failed to load texture image!");
        //}

        //images.push_back(VulkanMemoryManager::CreateTextureImage(texWidth, texHeight, pixels));

        //stbi_image_free(pixels);

        for(uint32_t i = 0; i < MAX_LIGHT; ++i) images.push_back(VulkanMemoryManager::CreateShadowMapBuffer());
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
        samplerInfo.minLod = static_cast<float>(textureMipLevels / 2);
        samplerInfo.maxLod = static_cast<float>(textureMipLevels);

        if (vkCreateSampler(vulkanDevice, &samplerInfo, nullptr, &vulkanTextureSampler))
        {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    AllocateCommandBuffer();

    DefineDrawBehavior();

    //create semaphore
    {
        vulkanImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        vulkanRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(swapchainImageSize, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (vkCreateSemaphore(vulkanDevice, &semaphoreInfo, nullptr, &vulkanImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vulkanDevice, &semaphoreInfo, nullptr, &vulkanRenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vulkanDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create semaphores!");
            }
        }
    }
}

void Graphic::postinit() {}

void Graphic::update(float dt)
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
        //time += dt * 0.01f;

        VulkanMemoryManager::MapMemory(UNIFORM_GUI_SETTING, &guiSetting);
    }

    ////pre render
    //{
    //    VkSubmitInfo submitInfo{};
    //    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    //    submitInfo.commandBufferCount = static_cast<uint32_t>(vulkanCommandBuffers.size());
    //    submitInfo.pCommandBuffers = vulkanCommandBuffers.data();

    //    if (vkQueueSubmit(application->GetGraphicQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
    //    {
    //        throw std::runtime_error("failed to submit draw command buffer!");
    //    }
    //    vkQueueWaitIdle(application->GetGraphicQueue());
    //}
    
    //pre render
    {
        for (auto uniform : drawinfos)
        {
            auto uniforminfo = uniform.second;
            uint32_t drawsize = static_cast<uint32_t>(uniforminfo.size());
            for (uint32_t index = 0; index < drawsize; ++index)
            {
                VulkanMemoryManager::MapMemory(uniform.first, uniforminfo[index].uniformdata, uniforminfo[index].uniformsize, uniforminfo[index].uniformoffset * index);
            }
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        submitInfo.commandBufferCount = 2;
        submitInfo.pCommandBuffers = &vulkanCommandBuffers[CMD_INDEX::CMD_BASE];

        if (vkQueueSubmit(application->GetGraphicQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        vkQueueWaitIdle(application->GetGraphicQueue());

        for (auto uniform : drawinfos)
        {
            uniform.second.clear();
        }
        drawinfos.clear();
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { vulkanImageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    std::array<VkCommandBuffer, 1> bufferlist = { vulkanCommandBuffers[imageIndex + CMD_INDEX::CMD_POST] };

    submitInfo.commandBufferCount = static_cast<uint32_t>(bufferlist.size());
    submitInfo.pCommandBuffers = bufferlist.data();

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
    vkDestroySampler(vulkanDevice, vulkanTextureSampler, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(vulkanDevice, vulkanRenderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(vulkanDevice, vulkanImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(vulkanDevice, inFlightFences[i], nullptr);
    }

    for (auto image : images)
    {
        image->close();
        delete image;
    }
    images.clear();

    descriptorManager->close();
    delete descriptorManager;

    VulkanMemoryManager::Close();

    CloseSwapChain();
}

Graphic::~Graphic() {}

void Graphic::drawGUI()
{
    if (ImGui::CollapsingHeader("Info##Graphic"))
    {
        ImGui::Text("Swapchain num : % d", swapchainImageSize);

        ImGui::Text("Samples : %d", vulkanMSAASamples);
    }

    if (ImGui::CollapsingHeader("Setting##Graphic"))
    {
        if (ImGui::Button("PositionTexture##GraphicSetting"))
        {
            guiSetting.deferred_type = GUI_ENUM::DEFERRED_POSITION;
        } ImGui::SameLine();
        if (ImGui::Button("NormalTexture##GraphicSetting"))
        {
            guiSetting.deferred_type = GUI_ENUM::DEFERRED_NORMAL;
        } ImGui::SameLine();
        if (ImGui::Button("DiffuseTexture##GraphicSetting"))
        {
            guiSetting.deferred_type = GUI_ENUM::DEFERRED_ALBEDO;
        } ImGui::SameLine();
        if (ImGui::Button("Light##GraphicSetting"))
        {
            guiSetting.deferred_type = GUI_ENUM::DEFERRED_LIGHT;
        }
        if (ImGui::CollapsingHeader("Shadow##SettingGraphic"))
        {
            ImGui::DragFloat("ShadowBias", &guiSetting.shadowbias, 0.001f, 0.0f, 10.0f);
            ImGui::DragFloat("ShadowFarPlane", &guiSetting.shadowfar_plane, 0.01f, 0.01f, 1000.0f);
            ImGui::DragFloat("ShadowDiskRadius", &guiSetting.shadowdiskRadius, 0.01f, 0.01f, 1000.0f);
        }
    }

    std::array<bool, GUI_ENUM::LIGHT_COMPUTE_MAX> lightcomputationbool = { false };

    lightcomputationbool[guiSetting.computation_type] = true;

    if (ImGui::RadioButton("PBR", lightcomputationbool[GUI_ENUM::LIGHT_COMPUTE_PBR])) guiSetting.computation_type = GUI_ENUM::LIGHT_COMPUTE_PBR;
    ImGui::SameLine();
    if (ImGui::RadioButton("Basic", lightcomputationbool[GUI_ENUM::LIGHT_COMPUTE_BASIC])) guiSetting.computation_type = GUI_ENUM::LIGHT_COMPUTE_BASIC;

    if (ImGui::Button("Reload Swapchain"))
    {
        application->framebufferSizeUpdate = true;
    }
}

void Graphic::AllocateCommandBuffer()
{
    vulkanCommandBuffers.resize(CMD_INDEX::CMD_POST + swapchainImageSize);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = application->GetCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(vulkanCommandBuffers.size());

    if (vkAllocateCommandBuffers(vulkanDevice, &allocInfo, vulkanCommandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Graphic::SetupSwapChain()
{
    //create swap chain
    {
        vulkanSwapChain = application->CreateSwapChain(swapchainImageSize, vulkanSwapChainImageFormat, vulkanSwapChainExtent);

        VulkanMemoryManager::GetSwapChainImage(vulkanSwapChain, swapchainImageSize, swapchainImages, vulkanSwapChainImageFormat);
    }

    {
        vulkanMSAASamples = getMaxUsableSampleCount();
    }

    //create resources (multisampled color resource)
    {
        framebufferImages.resize(FrameBufferIndex::FRAMEBUFFER_MAX);

        framebufferImages[FrameBufferIndex::POSITIONATTACHMENT] = VulkanMemoryManager::CreateFrameBufferImage(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_FORMAT_R16G16B16A16_SFLOAT, vulkanMSAASamples);
        framebufferImages[FrameBufferIndex::NORMALATTACHMENT] = VulkanMemoryManager::CreateFrameBufferImage(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_FORMAT_R16G16B16A16_SFLOAT, vulkanMSAASamples);
        framebufferImages[FrameBufferIndex::ALBEDOATTACHMENT] = VulkanMemoryManager::CreateFrameBufferImage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM, vulkanMSAASamples);

        framebufferImages[FrameBufferIndex::POSITIONATTACHMENT_MSAA] = VulkanMemoryManager::CreateFrameBufferImage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT);
        framebufferImages[FrameBufferIndex::NORMALATTACHMENT_MSAA] = VulkanMemoryManager::CreateFrameBufferImage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT);
        framebufferImages[FrameBufferIndex::ALBEDOATTACHMENT_MSAA] = VulkanMemoryManager::CreateFrameBufferImage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT);

        //depth buffer
        vulkanDepthFormat = findSupportedFormat({
            VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT
            }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        framebufferImages[FrameBufferIndex::DEPTHATTACHMENT] = VulkanMemoryManager::CreateDepthBuffer(vulkanDepthFormat, vulkanMSAASamples);
    }
}

void Graphic::DefineShadowMap()
{
    //descriptor set
    {
        std::vector<DescriptorData> data;

        data.push_back(DescriptorData());
        data.back().bufferinfo = VulkanMemoryManager::GetUniformBuffer(UNIFORM_OBJECT_MATRIX)->GetDescriptorInfo();
        data.push_back(DescriptorData());
        data.back().bufferinfo = VulkanMemoryManager::GetUniformBuffer(UNIFORM_LIGHTPROJ)->GetDescriptorInfo();

        descriptorSets[DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_SHADOWMAP] = descriptorManager->CreateDescriptorSet(PROGRAM_ID::PROGRAM_ID_SHADOWMAP, data);
    }

    //renderpass/framebuffer
    {
        VkAttachmentDescription attachment{};
        attachment.format = VK_FORMAT_D16_UNORM;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        renderPasses[RENDERPASS_INDEX::RENDERPASS_DEPTHCUBEMAP] = new Renderpass(vulkanDevice);
        Renderpass::Attachment attach;

        attach.attachmentDescription = attachment;
        attach.type = Renderpass::AttachmentType::ATTACHMENT_DEPTH;
        attach.bindLocation = 0;
        
        for (uint32_t i = 0; i < MAX_LIGHT; ++i)
        {
            attach.imageViews.push_back(images[i]->GetImageView());
        }
        renderPasses[RENDERPASS_INDEX::RENDERPASS_DEPTHCUBEMAP]->addAttachment(attach);

        attach.imageViews.clear();
        renderPasses[RENDERPASS_INDEX::RENDERPASS_DEPTHCUBEMAP]->createRenderPass();
        renderPasses[RENDERPASS_INDEX::RENDERPASS_DEPTHCUBEMAP]->createFramebuffers(1024, 1024, 6, MAX_LIGHT);
    }

    {
        std::array<VkVertexInputBindingDescription, 2> bindingDescriptions;
        bindingDescriptions[0] = PosNormal::getBindingDescription();
        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        bindingDescriptions[1].stride = sizeof(glm::vec3);

        auto attributeDescriptions = PosNormal::getAttributeDescriptions();
        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 2;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = 0;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineLayout layout = descriptorManager->GetpipeLineLayout(PROGRAM_ID::PROGRAM_ID_SHADOWMAP);

        graphicPipelines[PROGRAM_ID::PROGRAM_ID_SHADOWMAP] = new GraphicPipeline(vulkanDevice);
        graphicPipelines[PROGRAM_ID::PROGRAM_ID_SHADOWMAP]->init(renderPasses[RENDERPASS_INDEX::RENDERPASS_DEPTHCUBEMAP]->getRenderpass(), layout, VK_SAMPLE_COUNT_1_BIT, vertexInputInfo, 1, descriptorManager->Getshadermodule(PROGRAM_ID::PROGRAM_ID_SHADOWMAP),
            1024, 1024, false);
    }

    //command buffer will be defined in objectmanager
}

void Graphic::DefinePostProcess()
{
    //descriptor set
    {
        std::vector<DescriptorData> data;

        data.push_back(DescriptorData());
        data.back().bufferinfo = VulkanMemoryManager::GetUniformBuffer(UNIFORM_CAMERA_TRANSFORM)->GetDescriptorInfo();
        data.push_back(DescriptorData());
        data.back().bufferinfo = VulkanMemoryManager::GetUniformBuffer(UNIFORM_GUI_SETTING)->GetDescriptorInfo();
        data.push_back(DescriptorData());
        data.back().bufferinfo = VulkanMemoryManager::GetUniformBuffer(UNIFORM_LIGHTDATA)->GetDescriptorInfo();

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.sampler = vulkanTextureSampler;

        for (int i = 0; i < COLORATTACHMENT_MAX; ++i)
        {
            imageInfo.imageView = framebufferImages[i + COLORATTACHMENT_MAX]->GetImageView();
            data.push_back(DescriptorData());
            data.back().imageinfo = imageInfo;
        }

        for (uint32_t i = 0; i < MAX_LIGHT; ++i)
        {
            imageInfo.imageView = images[i]->GetImageView();
            data.push_back(DescriptorData());
            data.back().imageinfo = imageInfo;
            data.back().arrayindex = MAX_LIGHT;
        }

        descriptorSets[DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_DEFERRED] = descriptorManager->CreateDescriptorSet(PROGRAM_ID::PROGRAM_ID_DEFERRED, data);
    }

    //renderpass/framebuffer
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

        renderPasses[RENDERPASS_INDEX::RENDERPASS_POST] = new Renderpass(vulkanDevice);
        Renderpass::Attachment attach;
        attach.type = Renderpass::AttachmentType::ATTACHMENT_COLOR;
        attach.attachmentDescription = colorAttachment;
        attach.bindLocation = 0;
        for (auto swapimage : swapchainImages)
        {
            attach.imageViews.push_back(swapimage->GetImageView());
        }
        renderPasses[RENDERPASS_INDEX::RENDERPASS_POST]->addAttachment(attach);

        renderPasses[RENDERPASS_INDEX::RENDERPASS_POST]->createRenderPass();
        renderPasses[RENDERPASS_INDEX::RENDERPASS_POST]->createFramebuffers(Settings::windowWidth, Settings::windowHeight, 1, static_cast<uint32_t>(swapchainImageSize));
    }

    //create graphic pipeline
    {
        VkVertexInputBindingDescription bindingDescription = PosTexVertex::getBindingDescription();
        auto attributeDescriptions = PosTexVertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineLayout layout = descriptorManager->GetpipeLineLayout(PROGRAM_ID::PROGRAM_ID_DEFERRED);

        graphicPipelines[PROGRAM_ID::PROGRAM_ID_DEFERRED] = new GraphicPipeline(vulkanDevice);
        graphicPipelines[PROGRAM_ID::PROGRAM_ID_DEFERRED]->init(renderPasses[RENDERPASS_INDEX::RENDERPASS_POST]->getRenderpass(), layout, VK_SAMPLE_COUNT_1_BIT, vertexInputInfo, 1, descriptorManager->Getshadermodule(PROGRAM_ID::PROGRAM_ID_DEFERRED),
            Settings::windowWidth, Settings::windowHeight, false);
    }

    //create commandbuffer for post rendering
    {
        for (size_t i = CMD_INDEX::CMD_POST; i < vulkanCommandBuffers.size(); ++i)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;
            beginInfo.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(vulkanCommandBuffers[i], &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            renderPasses[RENDERPASS_INDEX::RENDERPASS_POST]->beginRenderpass(vulkanCommandBuffers[i], static_cast<uint32_t>(i - CMD_INDEX::CMD_POST));

            vkCmdBindPipeline(vulkanCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelines[PROGRAM_ID::PROGRAM_ID_DEFERRED]->GetPipeline());

            descriptorSets[DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_DEFERRED]->BindDescriptorSet(vulkanCommandBuffers[i], descriptorManager->GetpipeLineLayout(PROGRAM_ID::PROGRAM_ID_DEFERRED), {});

            DrawDrawtarget(vulkanCommandBuffers[i], drawtargets[DRAWTARGET_INDEX::DRAWTARGET_RECTANGLE]);

            vkCmdEndRenderPass(vulkanCommandBuffers[i]);

            if (vkEndCommandBuffer(vulkanCommandBuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }
}

void Graphic::CloseSwapChain()
{
    for (auto descriptorset : descriptorSets)
    {
        descriptorset->close();
        delete descriptorset;
    }

    vkFreeCommandBuffers(vulkanDevice, application->GetCommandPool(), static_cast<uint32_t>(vulkanCommandBuffers.size()), vulkanCommandBuffers.data());
    vulkanCommandBuffers.clear();

    for (auto renderpass : renderPasses)
    {
        renderpass->close();
        delete renderpass;
    }

    for (auto pipeline : graphicPipelines)
    {
        pipeline->close();
        delete pipeline;
    }

    for (auto image : framebufferImages)
    {
        image->close();
        delete image;
    }
    framebufferImages.clear();

    for (auto image : swapchainImages)
    {
        image->close();
        delete image;
    }
    swapchainImages.clear();

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
    AllocateCommandBuffer();
    DefineDrawBehavior();

    LevelManager::GetCurrentLevel()->postinit();
}

void Graphic::DrawDrawtarget(const VkCommandBuffer& cmdBuffer, const DrawTarget& target)
{
    uint32_t size = static_cast<uint32_t>(target.vertexIndices.size());

    for (uint32_t i = 0; i < size; ++i)
    {
        VkBuffer vertexbuffer = VulkanMemoryManager::GetBuffer(target.vertexIndices[i].vertex)->GetBuffer();

        VkBuffer vertexBuffers[] = { vertexbuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

        VkBuffer indexbuffer = VulkanMemoryManager::GetBuffer(target.vertexIndices[i].index)->GetBuffer();
        vkCmdBindIndexBuffer(cmdBuffer, indexbuffer, 0, VK_INDEX_TYPE_UINT32);

        if (target.instancebuffer.has_value())
        {
            VkBuffer instancebuffer = VulkanMemoryManager::GetBuffer(target.instancebuffer.value())->GetBuffer();
            VkBuffer instanceBuffer[] = { instancebuffer };
            vkCmdBindVertexBuffers(cmdBuffer, 1, 1, instanceBuffer, offsets);
        }

        uint32_t instancenumber = target.instancenumber.value_or(1);
        vkCmdDrawIndexed(cmdBuffer, target.vertexIndices[i].indexSize, instancenumber, 0, 0, 0);
    }
}

void Graphic::DefineDrawBehavior()
{
    DefinePostProcess();
    DefineShadowMap();

    {
        std::vector<DescriptorData> data;

        data.push_back(DescriptorData());
        data.back().bufferinfo = VulkanMemoryManager::GetUniformBuffer(UNIFORM_CAMERA_TRANSFORM)->GetDescriptorInfo();
        data.push_back(DescriptorData());
        data.back().bufferinfo = VulkanMemoryManager::GetUniformBuffer(UNIFORM_OBJECT_MATRIX)->GetDescriptorInfo();

        descriptorSets[DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_OBJ] = descriptorManager->CreateDescriptorSet(PROGRAM_ID::PROGRAM_ID_BASERENDER, data);
    }
    
    {
        std::vector<DescriptorData> data;

        data.push_back(DescriptorData());
        data.back().bufferinfo = VulkanMemoryManager::GetUniformBuffer(UNIFORM_CAMERA_TRANSFORM)->GetDescriptorInfo();
        data.push_back(DescriptorData());
        data.back().bufferinfo = VulkanMemoryManager::GetUniformBuffer(UNIFORM_LIGHT_OBJECT_MATRIX)->GetDescriptorInfo();

        descriptorSets[DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_LIGHT_OBJ] = descriptorManager->CreateDescriptorSet(PROGRAM_ID::PROGRAM_ID_BASERENDER, data);
    }

    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = vulkanSwapChainImageFormat;
        colorAttachment.samples = vulkanMSAASamples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorAttachmentResolve{};
        colorAttachmentResolve.format = vulkanSwapChainImageFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = vulkanDepthFormat;
        depthAttachment.samples = vulkanMSAASamples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        renderPasses[RENDERPASS_INDEX::RENDERPASS_PRE] = new Renderpass(vulkanDevice);
        Renderpass::Attachment attach;

        attach.type = Renderpass::AttachmentType::ATTACHMENT_COLOR;
        attach.attachmentDescription = colorAttachment;
        for (int i = 0; i < COLORATTACHMENT_MAX; ++i)
        {
            attach.attachmentDescription.format = framebufferImages[i]->GetFormat();
            attach.bindLocation = i;
            attach.imageViews.push_back(framebufferImages[i]->GetImageView());
            renderPasses[RENDERPASS_INDEX::RENDERPASS_PRE]->addAttachment(attach);
            attach.imageViews.clear();
        }

        attach.type = Renderpass::AttachmentType::ATTACHMENT_RESOLVE;
        attach.attachmentDescription = colorAttachmentResolve;
        for (int i = 0; i < COLORATTACHMENT_MAX; ++i)
        {
            int msaaindex = i + COLORATTACHMENT_MAX;
            attach.attachmentDescription.format = framebufferImages[msaaindex]->GetFormat();
            attach.bindLocation = msaaindex;
            attach.imageViews.push_back(framebufferImages[msaaindex]->GetImageView());
            renderPasses[RENDERPASS_INDEX::RENDERPASS_PRE]->addAttachment(attach);
            attach.imageViews.clear();
        }

        attach.attachmentDescription = depthAttachment;
        attach.type = Renderpass::AttachmentType::ATTACHMENT_DEPTH;
        attach.bindLocation = DEPTHATTACHMENT;
        attach.imageViews.push_back(framebufferImages[DEPTHATTACHMENT]->GetImageView());
        renderPasses[RENDERPASS_INDEX::RENDERPASS_PRE]->addAttachment(attach);
        attach.imageViews.clear();

        renderPasses[RENDERPASS_INDEX::RENDERPASS_PRE]->createRenderPass();
        renderPasses[RENDERPASS_INDEX::RENDERPASS_PRE]->createFramebuffers(Settings::windowWidth, Settings::windowHeight, 1);
    }

    //create graphic pipeline
    {
        std::array<VkVertexInputBindingDescription, 2> bindingDescriptions;
        bindingDescriptions[0] = PosNormal::getBindingDescription();
        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        bindingDescriptions[1].stride = sizeof(glm::vec3);

        auto vertdesc = PosNormal::getAttributeDescriptions();
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;
        attributeDescriptions[0] = vertdesc[0];
        attributeDescriptions[1] = vertdesc[1];
        attributeDescriptions[2].binding = 1;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = 0;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineLayout layout = descriptorManager->GetpipeLineLayout(PROGRAM_ID::PROGRAM_ID_BASERENDER);

        graphicPipelines[PROGRAM_ID::PROGRAM_ID_BASERENDER] = new GraphicPipeline(vulkanDevice);
        graphicPipelines[PROGRAM_ID::PROGRAM_ID_BASERENDER]->init(renderPasses[RENDERPASS_INDEX::RENDERPASS_PRE]->getRenderpass(), layout, vulkanMSAASamples, vertexInputInfo, 3, descriptorManager->Getshadermodule(PROGRAM_ID::PROGRAM_ID_BASERENDER),
            Settings::windowWidth, Settings::windowHeight, true);
    }

    {
        std::array<VkVertexInputBindingDescription, 2> bindingDescriptions;
        bindingDescriptions[0] = PosNormal::getBindingDescription();
        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        bindingDescriptions[1].stride = sizeof(glm::vec3);

        auto vertdesc = PosNormal::getAttributeDescriptions();
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;
        attributeDescriptions[0] = vertdesc[0];
        attributeDescriptions[1] = vertdesc[1];

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineLayout layout = descriptorManager->GetpipeLineLayout(PROGRAM_ID::PROGRAM_ID_DIFFUSE);

        graphicPipelines[PROGRAM_ID::PROGRAM_ID_DIFFUSE] = new GraphicPipeline(vulkanDevice);
        graphicPipelines[PROGRAM_ID::PROGRAM_ID_DIFFUSE]->init(renderPasses[RENDERPASS_INDEX::RENDERPASS_PRE]->getRenderpass(), layout, vulkanMSAASamples, vertexInputInfo, 3, descriptorManager->Getshadermodule(PROGRAM_ID::PROGRAM_ID_DIFFUSE),
            Settings::windowWidth, Settings::windowHeight, true);
    }

}

VkFormat Graphic::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(application->GetPhysicalDevice(), format, &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported forma!");
}

void Graphic::loadModel(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, const std::string& path, const std::string& filename)
{
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, (path + filename).c_str(), path.c_str()))
    {
        throw std::runtime_error("failed to load obj file : " + warn + err);
    }
}

VkSampleCountFlagBits Graphic::getMaxUsableSampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties = application->GetDeviceProperties();

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;

    return VK_SAMPLE_COUNT_1_BIT;
}

void Graphic::RegisterObject(DESCRIPTORSET_INDEX descriptorsetid, PROGRAM_ID programid, DRAWTARGET_INDEX drawtargetid)
{
    vkCmdBindPipeline(vulkanCommandBuffers[currentCommandIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelines[programid]->GetPipeline());

    descriptorSets[descriptorsetid]->BindDescriptorSetNoIndex(vulkanCommandBuffers[currentCommandIndex], descriptorManager->GetpipeLineLayout(programid));

    DrawDrawtarget(vulkanCommandBuffers[currentCommandIndex], drawtargets[drawtargetid]);
}

void Graphic::RegisterObject(DESCRIPTORSET_INDEX descriptorsetid, PROGRAM_ID programid, DRAWTARGET_INDEX drawtargetid, std::vector<uint32_t> indices)
{
    vkCmdBindPipeline(vulkanCommandBuffers[currentCommandIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelines[programid]->GetPipeline());

    descriptorSets[descriptorsetid]->BindDescriptorSet(vulkanCommandBuffers[currentCommandIndex], descriptorManager->GetpipeLineLayout(programid), indices);

    DrawDrawtarget(vulkanCommandBuffers[currentCommandIndex], drawtargets[drawtargetid]);
}

void Graphic::AddDrawInfo(DrawInfo drawinfo, UniformBufferIndex uniformid)
{
    drawinfos[uniformid].push_back(drawinfo);
}

void Graphic::BeginCmdBuffer(CMD_INDEX cmdindex)
{
    currentCommandIndex = cmdindex;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(vulkanCommandBuffers[cmdindex], &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
}

void Graphic::BeginRenderPass(CMD_INDEX cmdindex, RENDERPASS_INDEX renderpassindex, uint32_t framebufferindex)
{
    renderPasses[renderpassindex]->beginRenderpass(vulkanCommandBuffers[cmdindex], framebufferindex);
}

void Graphic::EndCmdBuffer(CMD_INDEX cmdindex)
{
    if (vkEndCommandBuffer(vulkanCommandBuffers[cmdindex]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void Graphic::EndRenderPass(CMD_INDEX cmdindex)
{
    vkCmdEndRenderPass(vulkanCommandBuffers[cmdindex]);
}

void DrawTarget::AddVertex(VertexInfo info)
{
    vertexIndices.push_back(info);
}
