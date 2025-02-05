#include <iostream>
#include <string>
#include <vulkan/vulkan.hpp>
#include "VkBootstrap.h"
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

    // TODO
    

    vkb::destroy_instance(vkbInstance);

    return 0;
}