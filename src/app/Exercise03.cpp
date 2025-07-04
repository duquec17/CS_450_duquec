#include <iostream>
#include <string>
#include <vulkan/vulkan.hpp>
#include "VkBootstrap.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "VKUtility.hpp"
#include "VKBuffer.hpp"
using namespace std;

struct PointVertex {
    glm::vec3 pos;
};

vector<PointVertex> vertices = {
    {{-0.5f, -0.5f, 0.5f}},
    {{ 0.5f, -0.5f, 0.5f}},
    {{ 0.5f,  0.5f, 0.5f}},
    {{-0.5f,  0.5f, 0.5f}}
};

vector<unsigned int> indices = { 0, 2, 1, 2, 0, 3};

int main(int argc, char **argv) {

    if(argc >= 2) {
        string filename = string(argv[1]);
        cout << "FILENAME: " << filename << endl;
    }

    cout << "BEGIN THE EXERCISE!" << endl;

    vkb::InstanceBuilder instBuilder;
    auto instRes = instBuilder.request_validation_layers()
                            .use_default_debug_messenger()
                            .build();
    vkb::Instance vkbInstance = instRes.value();
    vk::Instance instance = vk::Instance { vkbInstance.instance };

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "BEHOLD", 
                                            nullptr, nullptr);

    VkSurfaceKHR vkSurface = nullptr;
    auto surfRes = glfwCreateWindowSurface(vkbInstance.instance,
                                            window, nullptr,
                                            &vkSurface);
    vk::SurfaceKHR surface = vk::SurfaceKHR { vkSurface };

    vkb::PhysicalDeviceSelector phySelect { vkbInstance };
    auto phyRet = phySelect.set_surface(vkSurface)
                            .set_minimum_version(1,1)
                            .select();
    vkb::PhysicalDevice vkbPhyDevice = phyRet.value();
    vk::PhysicalDevice phyDevice
         = vk::PhysicalDevice { vkbPhyDevice.physical_device };

    cout << "DEVICE: " << phyDevice.getProperties().deviceName << endl;

    vkb::DeviceBuilder devBuilder { vkbPhyDevice };
    auto devRet = devBuilder.build();
    vkb::Device vkbDevice = devRet.value();
    vk::Device device = vk::Device { vkbDevice.device };

    auto graphQRet = vkbDevice.get_queue(vkb::QueueType::graphics);
    auto presQRet = vkbDevice.get_queue(vkb::QueueType::present);

    vk::Queue graphQueue = vk::Queue { graphQRet.value() };
    vk::Queue presentQueue = vk::Queue { presQRet.value() };

    auto graphIndex 
        = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    auto presentIndex
        = vkbDevice.get_queue_index(vkb::QueueType::present).value();

    cout << "Graphics queue: " << graphIndex << endl;
    cout << "Present queue: " << presentIndex << endl;

    vkb::SwapchainBuilder swapBuilder { vkbDevice };
    VkSurfaceFormatKHR surfaceFormat;
    surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
    surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    auto swapRet = swapBuilder
        .set_desired_format(surfaceFormat)
        .build();
    vkb::Swapchain vkbSwapchain = swapRet.value();

    vk::SwapchainKHR swapchain
        = vk::SwapchainKHR { vkbSwapchain.swapchain };
    vk::Format swapformat { vkbSwapchain.image_format };
    vk::Extent2D swapextent { vkbSwapchain.extent };
    vector<VkImageView> oldViews
        = vkbSwapchain.get_image_views().value();
    vector<vk::ImageView> swapviews;
    for(int i = 0; i < oldViews.size(); i++) {
        swapviews.push_back(
            vk::ImageView { oldViews.at(i)});
    }

    vector<vk::AttachmentDescription> attachDesc;
    attachDesc.push_back(vk::AttachmentDescription(
        {},
        swapformat,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::ePresentSrcKHR
    ));

    vk::AttachmentReference colorAttachRef(
        0, vk::ImageLayout::eColorAttachmentOptimal
    );

    vk::SubpassDescription subpassDesc(
        vk::SubpassDescriptionFlags(),
        vk::PipelineBindPoint::eGraphics,
        {}, colorAttachRef, {}, {}
    );

    vk::RenderPass pass
        = device.createRenderPass(
            vk::RenderPassCreateInfo(
                {}, attachDesc, subpassDesc
            )
        );

    auto vertSrc = readBinaryFile(
        "build/compiledshaders/ProfExercises03/shader.vert.spv");
    auto fragSrc = readBinaryFile(
        "build/compiledshaders/ProfExercises03/shader.frag.spv");

    vk::ShaderModule vertShader = createVulkanShaderModule(device, vertSrc);
    vk::ShaderModule fragShader = createVulkanShaderModule(device, fragSrc);

    vk::PipelineShaderStageCreateInfo shaderStages[] = {
        vk::PipelineShaderStageCreateInfo(
            {}, vk::ShaderStageFlagBits::eVertex, vertShader, "main"
        ),
        vk::PipelineShaderStageCreateInfo(
            {}, vk::ShaderStageFlagBits::eFragment, fragShader, "main"
        )
    };

    vk::DeviceSize vertBufferSize = sizeof(vertices[0])*vertices.size();
    vk::DeviceSize indBufferSize = sizeof(indices[0])*indices.size();

    VulkanBuffer vkVertices
         = createVulkanBuffer(
                phyDevice, device,
                vertBufferSize, 
                vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible
                | vk::MemoryPropertyFlagBits::eHostCoherent);
    VulkanBuffer vkIndices
         = createVulkanBuffer(
                phyDevice, device,
                indBufferSize, 
                vk::BufferUsageFlagBits::eIndexBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible
                | vk::MemoryPropertyFlagBits::eHostCoherent);
    
    copyDataToVulkanBuffer( device, 
                            vkVertices.memory, 
                            vertBufferSize,
                            vertices.data());
    copyDataToVulkanBuffer( device, 
                            vkIndices.memory, 
                            indBufferSize,
                            indices.data());

    vk::VertexInputBindingDescription vertBindDesc 
        = vk::VertexInputBindingDescription(
            0, sizeof(vertices[0]), vk::VertexInputRate::eVertex);

    vector<vk::VertexInputAttributeDescription> vertAttribDesc 
        = {
            vk::VertexInputAttributeDescription(
                0, 0, 
                vk::Format::eR32G32B32Sfloat,
                offsetof(PointVertex, pos))
        };
    
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
        {}, vertBindDesc, vertAttribDesc
    );

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        {}, vk::PrimitiveTopology::eTriangleList, false
    );

    vk::Viewport viewport(
        0, 0, (float)swapextent.width, (float)swapextent.height,
        0.0f, 1.0f
    );
    vk::Rect2D scissors({0,0}, swapextent);
    vk::PipelineViewportStateCreateInfo viewportInfo(
        {}, viewport, scissors
    );

    vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo dynamicInfo(
        {}, dynamicStates
    );

    vk::PipelineRasterizationStateCreateInfo
        rasterizer {};
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode
     = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace
     = vk::FrontFace::eCounterClockwise;

    vk::PipelineColorBlendAttachmentState colorBlend {};
    colorBlend.colorWriteMask = 
        vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo colorBlending(
        {}, false, vk::LogicOp::eCopy, colorBlend
    );
    
    vk::PipelineDepthStencilStateCreateInfo depthStencil (
        {}, true, true, vk::CompareOp::eLess,
        false, false, {}, {}
    );

    vk::PipelineLayoutCreateInfo layoutInfo(
        {}, {}, {}
    );
    vk::PipelineLayout pipelineLayout
    = device.createPipelineLayout(layoutInfo);

    vk::PipelineMultisampleStateCreateInfo multisample(
        {}, vk::SampleCountFlagBits::e1
    );

    vk::PipelineCache pipelineCache
    = device.createPipelineCache({});

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        vk::PipelineCreateFlags(),
        shaderStages,
        &vertexInputInfo,
        &inputAssembly,
        0,
        &viewportInfo,
        &rasterizer,
        &multisample,
        &depthStencil,
        &colorBlending,
        &dynamicInfo,
        pipelineLayout,
        pass
    );
    auto pipeRet = device.createGraphicsPipeline(
        pipelineCache, pipelineInfo
    );
    vk::Pipeline graphicsPipeline = pipeRet.value;
    
    device.destroyShaderModule(vertShader);
    device.destroyShaderModule(fragShader);   

    vector<vk::Framebuffer> framebuffers;
    framebuffers.resize(swapviews.size());
    for(int i = 0; i < framebuffers.size(); i++) {
        vector<vk::ImageView> attach = {
            swapviews.at(i)
        };
        framebuffers[i] = device.createFramebuffer(
            vk::FramebufferCreateInfo(
                {},
                pass,
                attach,
                swapextent.width,
                swapextent.height,
                1
            )
        );
    }  

    vk::CommandPool commandPool 
     = device.createCommandPool(
        vk::CommandPoolCreateInfo(
        vk::CommandPoolCreateFlags(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer
        ),
        graphIndex
        )
     );

    vk::CommandBuffer commandBuffer
      = device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo(
            commandPool,
            vk::CommandBufferLevel::ePrimary,
            1
        )
    ).front();

    vk::Fence inFlightFence
    = device.createFence(
        vk::FenceCreateInfo(
            vk::FenceCreateFlagBits::eSignaled)
    );

    vk::Semaphore imageSem 
    = device.createSemaphore(vk::SemaphoreCreateInfo());
    vk::Semaphore renderSem 
    = device.createSemaphore(vk::SemaphoreCreateInfo());
    

     
    // CREATION TODO

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        device.waitForFences(1, &inFlightFence, 
                            true, UINT64_MAX);
        device.resetFences(1, &inFlightFence);

        auto result = device.acquireNextImageKHR(
            swapchain, UINT64_MAX, 
            imageSem, nullptr
        );

        unsigned int frameIndex = result.value;

        commandBuffer.reset();
        commandBuffer.begin(vk::CommandBufferBeginInfo());
        array<vk::ClearValue, 1> clearValues {};
        clearValues[0].color
         = vk::ClearColorValue(0.9f, 0.9f, 0.0f, 1.0f);

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo(
                pass, framebuffers[frameIndex],
                { {0,0}, swapextent},
                clearValues
            ),
            vk::SubpassContents::eInline
        );

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            graphicsPipeline
        );

        vk::Viewport viewports[] =
        {
            {0,0,
            (float)swapextent.width,
            (float)swapextent.height,
            0.0f, 1.0f}
        };
        commandBuffer.setViewport(0, viewports);

        vk::Rect2D scissors[] =
        {
            {{0,0}, swapextent}
        };
        commandBuffer.setScissor(0, scissors);  

        // RENDER TODO
        vk::Buffer vertBuffers[] = {vkVertices.buffer};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(
            0, vertBuffers, offsets);

        commandBuffer.bindIndexBuffer(
            vkIndices.buffer, 0, 
            vk::IndexType::eUint32);

        commandBuffer.drawIndexed(
            indices.size(), 1,0,0,0);

        commandBuffer.endRenderPass();
        commandBuffer.end();

        vk::Semaphore waitList[] = {imageSem};
        vk::Semaphore sigList[] = {renderSem};

        vk::PipelineStageFlags waitStages[]
        = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

        vk::SubmitInfo submitInfo(
            waitList, 
            waitStages,
            commandBuffer,
            sigList
        );

        graphQueue.submit(submitInfo, inFlightFence);

        vk::SwapchainKHR swapchains[] = {swapchain};
        uint32_t imageIndices[] = {frameIndex};
        vk::PresentInfoKHR presentInfo(
            sigList,
            swapchains,
            imageIndices
        );

        presentQueue.presentKHR(presentInfo);
    }

    // CLEANUP TODO
    device.destroySemaphore(imageSem);
    device.destroySemaphore(renderSem);
    device.destroyFence(inFlightFence);
    device.destroyCommandPool(commandPool);
    for(auto framebuffer: framebuffers) {
        device.destroyFramebuffer(framebuffer);
    }
    framebuffers.clear();

    device.destroyPipelineCache(pipelineCache);
    device.destroyPipelineLayout(pipelineLayout);
    device.destroyPipeline(graphicsPipeline);
    cleanupVulkanBuffer(device, vkIndices);
    cleanupVulkanBuffer(device, vkVertices);

    device.destroyRenderPass(pass);
    for(int i = 0; i < swapviews.size(); i++) {
        device.destroyImageView(swapviews.at(i));
    }
    swapviews.clear();
    device.destroySwapchainKHR(swapchain);
    device.destroy();
    instance.destroySurfaceKHR(surface);
    glfwDestroyWindow(window);
    glfwTerminate();
    vkb::destroy_instance(vkbInstance);

    return 0;
}