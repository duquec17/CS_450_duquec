#include <iostream>
#include <string>
#include <vulkan/vulkan.hpp>
#include "VkBootstrap.h"
#define GLFW_INCLUDE_NONE
#include <

using namespace std;

int main(int argc, char **arg) {

    if(argc >= 2) {
        string filename = string(arg[1]);
        cout << "FILENAME: " << filename << endl;
    }

    cout << "BEGIN THE EXERCISE!" << endl;

    vkb::InstanceBuilder instBuilder;
    auto instRes = instBuilder.request_validation_layers()
                            .use_default_debug_messenger()
                            .build();
    vkb::Instance vkbInstance = instRes.value();
    vk::Instance instance = vk::Instance {vkbInstance.instance};

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "BEHOLD",
                                            nullptr, nullptr);

    vkSurfaceKHR vkSurface = nullptr;
    auto surfRes = glfwCreateWindowSurface(vkbInstance.instance,
                                            window, nullptr,
                                            &vkSurface);
    vk::SurfaceKHR surface = vk::SurfaceKHR {vkSurface};
    
    vkb::PhysicalDeviceSelector phySelect {vkbInstance};
    auto pppphyRet = phySelect.set_surface(vkSurface)
                                .set_minimum_version(1,1)
                                .select();
    vkb::PhysicalDevice vkbPhyDevice =phyRet.value();
    vk::PhysicalDevice PhyDevice
        = vk::PhysicalDevice(vkbPhyDevice.physical_device);

    vkb::DeviceBuilder devBuilder {vkbPhyDevice};
    auto devRet = devBuilder.build();
    vkb::Device vkbDevice = devRet.value();
    vk::Device device = vk::Device {vkbDevice.device};

    auto graphQRet = vkbDevice.get_queue(vkb::QueueType::graphics);
    auto presQRet = vkbDevice.get_queue(vkb::QueueType::present);

    
    

    // CREATION TODO

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        // RENDER TODO
    }

    // TODO

    // CLEANUP TODO
    instance.destroySurfaceKHR(surface);
    glfwDestroyWindow(window);
    glfwTerminate();
    vkb::destroy_instance(vkbInstance);

    return 0;
}