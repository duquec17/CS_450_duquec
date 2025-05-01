#include "VKSetup.hpp"
#include "VKRender.hpp"
#include "VKImage.hpp"
#include "VKUtility.hpp"
#include "VKUniform.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "MeshData.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/transform.hpp"
#include "glm/gtx/string_cast.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <vulkan/vulkan_structs.hpp>

// Hold information for a vertex
struct Vertex {
    glm::vec3 pos;
    glm::vec4 color;
    glm::vec3 normal;
};

// Hold data for a point light
struct PointLight {
    alignas(16)glm::vec4 pos;
    alignas(16)glm::vec4 vpos;
    alignas(16)glm::vec4 color;
};

// Hold scene data
struct SceneData {
    vector<VulkanMesh> allMeshes;
    const aiScene *scene = nullptr;
    float rotAngle = 0.0f;
    
    glm::vec3 eye = glm::vec3(0,0,1);
    glm::vec3 lookAt;
    glm::vec2 mousePos;
    glm::mat4 viewMat;
    glm::mat4 projMat;

    PointLight light {
        glm::vec4(0.5f, 0.5f, 0.5f, 1.0f),
        glm::vec4(0.0f),
        glm::vec4(1.0f,1.0f,1.0f,1.0f)
    };

    float metallic = 0.0f;
    float roughness = 0.1f;
};

// Hold Vertex shader UBO host data
struct UBOVertex {
    alignas(16) glm::mat4 viewMat;
    alignas(16) glm::mat4 projMat;
};

// Hold vertex shader push constants
struct UPushVertex {
    alignas(16)glm::mat4 modelMat;
    alignas(16)glm::mat4 normMat;
};

// Hold fragment shader UBO host data
struct UBOFragment {
    PointLight light;
    alignas(4) float metallic = 0.0f;
    alignas(4) float roughness = 0.1f;
};

// Global instance of struct
SceneData sceneData;
UPushVertex uPush;

// Function for generating a transformation to rotate around Z
glm::mat4 makeRotateZ(float rotAngle, glm::vec3 offset){
    float radians = glm::radians(rotAngle);
    glm::mat4 translationNeg = glm::translate(-offset);
    glm::mat4 rotation = glm::rotate(radians, glm::vec3(0.0f,0.0f, 1.0f));
    glm::mat4 translationPos = glm::translate(offset);
    return translationPos * rotation * translationNeg;
}

// Function for generating a transformation to rotate point and axis
glm::mat4 makeLocalRotate(glm::vec3 offset, glm::vec3 axis, float angle) {
    float radians = glm::radians(angle);

    glm::mat4 translationNeg = glm::translate(glm::mat4(1.0f), -offset); // Translate by negative offset
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radians, axis);   // Rotate around arbitrary axis
    glm::mat4 translationPos = glm::translate(glm::mat4(1.0f), offset); // Translate back by offset
    
    return translationPos * rotation * translationNeg;
}

// Function for Mouse cursor movement callback
static void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
    // Retrieve scene data
    SceneData* sceneData = static_cast<SceneData*>(glfwGetWindowUserPointer(window));

    // Calculate relative mouse position
    glm::vec2 currentMousePos(xpos, ypos);
    glm::vec2 relMouse = currentMousePos - sceneData->mousePos;

    sceneData->mousePos = currentMousePos;

    // Get frame buffer size
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    if (width > 0 && height > 0) {
        // Scale rlative mouse motion to rotate camera
        relMouse.x /= static_cast<float>(width);
        relMouse.y /= static_cast<float>(height);

        // Relative Y motion
        glm::mat4 rotateY = makeLocalRotate(
            sceneData->eye,
            glm::vec3(0.0f, 1.0f, 0.0f),
            30.0f * -relMouse.x
        );
        
        // Camera rotation and transformations
        glm::vec3 cameraDirection = glm::normalize(sceneData->lookAt - sceneData->eye);

        // Compute local x-axis
        glm::vec3 localXAxis = glm::normalize(glm::cross(cameraDirection, glm::vec3 (0.0f, 1.0f, 0.0f)));

        // Relative x motion
        glm::mat4 rotateX = makeLocalRotate(
            sceneData->eye,
            localXAxis,
            30.0f * -relMouse.y
        );

        // Apply rotations
        glm::vec4 lookAtV = glm::vec4(sceneData->lookAt, 1.0f);
        lookAtV = rotateX * rotateY * lookAtV;

        // Update lookAt point
        sceneData->lookAt = glm::vec3(lookAtV);
    }
}

// New class that inherits from VlkrEngine
class Assign05RenderEngine : public VulkanRenderEngine{
    protected:
        UBOVertex hostUBOVert;
        UBOData deviceUBOVert;
        UBOFragment hostUBOFrag;
        UBOData deviceUBOFrag;

        vk::DescriptorPool descriptorPool;
        vector<vk::DescriptorSet> descriptorSets;

    // Constructor
    public:
        Assign05RenderEngine(VulkanInitData &vkInitData): VulkanRenderEngine(vkInitData){}

    // Overrides initialize function
    virtual bool initialize(VulkanInitRenderParams *params) override {
        if(!VulkanRenderEngine::initialize(params)){
            return false;
        }

        // Create deviceUBOVert
        deviceUBOVert = createVulkanUniformBufferData(
            vkInitData.device, vkInitData.physicalDevice, 
            sizeof(UBOVertex), MAX_FRAMES_IN_FLIGHT);

        // Create deviceUBOVert Frag
        deviceUBOFrag = createVulkanUniformBufferData(
            vkInitData.device, vkInitData.physicalDevice, 
            sizeof(UBOFragment), MAX_FRAMES_IN_FLIGHT);

        // Create descriptor pool
        std::vector<vk::DescriptorPoolSize> poolSizes = {
            {vk::DescriptorType::eUniformBuffer, 2 * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)}
        };
        
        vk::DescriptorPoolCreateInfo poolCreateInfo = {};
        poolCreateInfo.setPoolSizes(poolSizes)
                      .setMaxSets(MAX_FRAMES_IN_FLIGHT);
        
        descriptorPool = vkInitData.device.createDescriptorPool(poolCreateInfo);

        // Create descriptor sets
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, pipelineData.descriptorSetLayouts[0]);
        vk::DescriptorSetAllocateInfo allocInfo = {};
        allocInfo.setDescriptorPool(descriptorPool)
                 .setSetLayouts(layouts);
        descriptorSets = vkInitData.device.allocateDescriptorSets(allocInfo);

        // Configure descriptor sets
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vk::DescriptorBufferInfo bufferInfo = {};
            bufferInfo.setBuffer(deviceUBOVert.bufferData[i].buffer)
                      .setOffset(0)
                      .setRange(sizeof(UBOVertex));

            vk::WriteDescriptorSet write = {};
            write.setDstSet(descriptorSets[i])
                 .setDstBinding(0)
                 .setDstArrayElement(0)
                 .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                 .setDescriptorCount(1)
                 .setBufferInfo(bufferInfo);

            vk::DescriptorBufferInfo bufferFragInfo = {};
            bufferFragInfo.setBuffer(deviceUBOVert.bufferData[i].buffer)
                          .setOffset(0)
                          .setRange(sizeof(UBOFragment));

            vk::WriteDescriptorSet descFragWrites = {};
            descFragWrites.setDstSet(descriptorSets[i])
                          .setDstBinding(1)
                          .setDstArrayElement(0)
                          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                          .setDescriptorCount(1)
                          .setBufferInfo(bufferFragInfo);

            vkInitData.device.updateDescriptorSets({write, descFragWrites}, {});
        }

        return true;
    };

    // Change Override getDiscriptorSetLayOuts
    virtual std::vector<vk::DescriptorSetLayout> getDescriptorSetLayouts() override {
        vk::DescriptorSetLayoutBinding binding, allBindings = {};
        
        // Vertex shader UBO binding
        binding.setBinding(0)
               .setDescriptorType(vk::DescriptorType::eUniformBuffer)
               .setDescriptorCount(1)
               .setStageFlags(vk::ShaderStageFlagBits::eVertex)
               .setPImmutableSamplers(nullptr);

        // Fragment shader UBO binding
        allBindings.setBinding(1)
                   .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                   .setDescriptorCount(1)
                   .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                   .setPImmutableSamplers(nullptr);

        // Create a vector to store both bindings
        std::vector<vk::DescriptorSetLayoutBinding> allBindingsVec = {binding, allBindings};

        vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.setBindings(allBindingsVec);

        vk::DescriptorSetLayout layout = vkInitData.device.createDescriptorSetLayout(layoutInfo);
        return {layout};
    }

    // Override AttributeDescData
    virtual AttributeDescData getAttributeDescData() override {
        // Create attribDesc
        AttributeDescData attribDescData;
        
        // Set attribDesc
        attribDescData.bindDesc = 
        vk::VertexInputBindingDescription(0, sizeof(Vertex),
        vk::VertexInputRate::eVertex);

        // Clear attrib
        attribDescData.attribDesc.clear();

        // Instance 1 (Position)
        attribDescData.attribDesc.push_back(
            vk::VertexInputAttributeDescription(
                0,
                0,
                vk::Format:: eR32G32B32Sfloat,
                offsetof(Vertex, pos)
            )
        );

        // Instance 2 (color)
        attribDescData.attribDesc.push_back(
            vk::VertexInputAttributeDescription(
                1,
                0,
                vk::Format:: eR32G32B32A32Sfloat,
                offsetof(Vertex, color)
            )
        );

        // Instance 3 (normal)
        attribDescData.attribDesc.push_back(
            vk::VertexInputAttributeDescription(
                2,
                0,
                vk::Format:: eR32G32B32Sfloat,
                offsetof(Vertex, normal)
            )
        );

        return attribDescData;
    }

    // Update uniform buffers
    virtual void updateUniformBuffers(SceneData *sceneData, vk::CommandBuffer &commandBuffer) {
        hostUBOVert.viewMat = sceneData->viewMat;
        hostUBOVert.projMat = sceneData->projMat;
        hostUBOVert.projMat[1][1] *= -1; // Invert Y-axis for Vulkan

        memcpy(deviceUBOVert.mapped[currentImage], &hostUBOVert, sizeof(hostUBOVert));

        // Copy values from sceneData into appropriate fields
        hostUBOFrag.light = sceneData->light;
        hostUBOFrag.metallic = sceneData->metallic;
        hostUBOFrag.roughness = sceneData->roughness;

        memcpy(deviceUBOFrag.mapped[this->currentImage], &hostUBOFrag, sizeof(hostUBOFrag));

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, pipelineData.pipelineLayout, 0,
            descriptorSets[currentImage], {});
    }

    // Override recordCommandBuffer
    virtual void recordCommandBuffer(void *userData,
                                     vk::CommandBuffer &commandBuffer,
                                     unsigned int frameIndex) override {
        SceneData *sceneData = static_cast<SceneData *>(userData);

        commandBuffer.begin(vk::CommandBufferBeginInfo());

        vk::Extent2D extent = vkInitData.swapchain.extent;

        std::array<vk::ClearValue, 2> clearValues = {
            vk::ClearColorValue(std::array<float, 4>{0.6f, 0.8f, 0.3f, 1.0f}),
            vk::ClearDepthStencilValue(1.0f, 0.0f)
        };

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo(
                renderPass, framebuffers[frameIndex],
                {{0, 0}, extent}, clearValues),
            vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineData.graphicsPipeline);

        vk::Viewport viewport = {0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};
        commandBuffer.setViewport(0, viewport);

        vk::Rect2D scissor = {{0, 0}, extent};
        commandBuffer.setScissor(0, scissor);

        updateUniformBuffers(sceneData, commandBuffer);

        renderScene(commandBuffer, sceneData, sceneData->scene->mRootNode, glm::mat4(1.0f), 0);

        commandBuffer.endRenderPass();
        commandBuffer.end();
    }

    // Destructor
    virtual~Assign05RenderEngine(){
        vkInitData.device.destroyDescriptorPool(descriptorPool);
        cleanupVulkanUniformBufferData(vkInitData.device, deviceUBOVert); // Vertex cleaner
        cleanupVulkanUniformBufferData(vkInitData.device, deviceUBOFrag); // Frag cleaner
    };

    virtual vector<vk::PushConstantRange>getPushConstantRanges()override{
        // Create a push constant range for the vertex shader
        vk::PushConstantRange vertexPushConstantRange;
        vertexPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vertexPushConstantRange.offset = 0;
        vertexPushConstantRange.size = sizeof(UPushVertex);

        // Return the appropriate vector push constant ranges
        return {vertexPushConstantRange};
    }

    // Function for rendering a scene recursively
    void renderScene(vk::CommandBuffer &commandBuffer,
                     SceneData *sceneData, aiNode *node, 
                     glm::mat4 parentMat, int level)
    {
        // Get the transformation for the current node
        aiMatrix4x4 aiTrans = node->mTransformation;
        glm::mat4 nodeT;

        // Convert the transformation to a glm mat 4 nodeT
        aiMatToGLM4(aiTrans, nodeT);

        // Compute the current model matrix
        glm::mat4 modelMat = parentMat * nodeT;

        // Get location of current node
        glm::vec3 pos = glm::vec3(modelMat[3]);
        glm::mat4 R = makeRotateZ(sceneData->rotAngle, pos);
        glm::mat4 tmpModel = R * modelMat;

        // Compute normal matrix
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(sceneData->viewMat * tmpModel)));

        UPushVertex uPush;
        uPush.modelMat = tmpModel;
        uPush.normMat = glm::mat4(normalMatrix);

        commandBuffer.pushConstants<UPushVertex>(
            this->pipelineData.pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            uPush);

        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            unsigned int meshIndex = node->mMeshes[i];
            if (meshIndex < sceneData->allMeshes.size()) {
                recordDrawVulkanMesh(commandBuffer, sceneData->allMeshes[meshIndex]);
            }
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            renderScene(commandBuffer, sceneData, node->mChildren[i], modelMat, level + 1);
        }
    }
};

void keyCallBack(GLFWwindow* window, int key, int scanCode, int action, int mods){
    // If the action is either GLFW_Press or Repeat check for keys
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        SceneData* sceneData = static_cast<SceneData*>(glfwGetWindowUserPointer(window));
        
        // Define movement speed
        float speed = 0.1f;

        // Camera rotation and transformations
        glm::vec3 cameraDirection = glm::normalize(sceneData->lookAt - sceneData->eye);

        // Define global Y-axis
        glm::vec3 globalYAxis = glm::vec3(0.0f, 1.0f, 0.0f);

        // Compute local X-axis (right direction)
        glm::vec3 localXAxis = glm::normalize(glm::cross(cameraDirection, globalYAxis));

        switch(key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;

            case GLFW_KEY_J:
                sceneData->rotAngle += 1.0f;
                break;
            
            case GLFW_KEY_K:
                sceneData->rotAngle -= 1.0f;
                break;

            case GLFW_KEY_W:
                sceneData->eye += cameraDirection * speed;
                sceneData->lookAt += cameraDirection * speed;
                break;
            
            case GLFW_KEY_S:
                sceneData->eye -= cameraDirection * speed;
                sceneData->lookAt -= cameraDirection * speed;
                break;

            case GLFW_KEY_D:
                sceneData->eye += localXAxis * speed;
                sceneData->lookAt += localXAxis * speed;
                break;
            
            case GLFW_KEY_A:
                sceneData->eye -= localXAxis * speed;
                sceneData->lookAt -= localXAxis * speed;
                break;

            case GLFW_KEY_1:
                sceneData->light.color = glm::vec4 (1,1,1,1); // White
                break;

            case GLFW_KEY_2:
                sceneData->light.color = glm::vec4 (1,0,0,1); // Red
                break;
            
            case GLFW_KEY_3:
                sceneData->light.color = glm::vec4 (0,1,0,1); // Green
                break;
            
            case GLFW_KEY_4:
                sceneData->light.color = glm::vec4 (0,0,1,1); // Blue
                break;

            case GLFW_KEY_V:
                sceneData->metallic = std::max(0.0f, sceneData->metallic - 0.1f);
                break;

            case GLFW_KEY_B:
                sceneData->metallic = std::min(1.0f, sceneData->metallic + 0.1f);
                break;

            case GLFW_KEY_N:
                sceneData->roughness = std::max(0.1f, sceneData->roughness - 0.1f);
                break;

            case GLFW_KEY_M:
                sceneData->roughness = std::min(0.7f, sceneData->roughness + 0.1f);
                break;
        }
    }
}



void extractMeshData(aiMesh *mesh, Mesh<Vertex>&m) {
    // Clear out the mesh's vertices and indices
    m.vertices.clear();
    m.indices.clear();

    // Loop through all vertices in the aiMesh
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        // Create vertex
        Vertex vertex;

        // Grab the vertex position information from mesh and store it in the vertex's position
        vertex.pos = glm::vec3(
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        );

        // Set the color of the vertex to any color other than (0,0,1) or the background
        vertex.color = glm::vec4(1.0f,1.0f,0.0f,1.0f);

        // Set the normal using data from mesh
        if (mesh->HasNormals()) {
            vertex.normal = glm::vec3(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );
        } else {
            vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f); // Default normal if not available
        }

        // Add the vertex to the mesh's vertices list
        m.vertices.push_back(vertex);
    }

    // Loop through all faces in the aiMesh
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        // Grab the aiFace face from the mesh
        aiFace &face = mesh->mFaces[i];

        //Loop through the number of indices for this face
        for (unsigned int k = 0; k < face.mNumIndices; k++) {
            m.indices.push_back(face.mIndices[k]);
        }
    }
}

int main(int argc, char **argv) {
    // Start message
    cout << "BEGIN Model FORGING!!!" << endl;

    // The model to load will be provided on the command line
    // Use sampleModels sphere as default model path
    string modelPath = "sampleModels/bunnyteatime.glb";
    if (argc >= 2){
        modelPath = string(argv[1]);
    }

    Assimp::Importer importer;

    // Load the model using Assimp to get an aiScene
    sceneData.scene = importer.ReadFile(
                                  modelPath,
                                  aiProcess_Triangulate |
                                  aiProcess_FlipUVs |
                                  aiProcess_GenNormals |
                                  aiProcess_JoinIdenticalVertices);

    // Check to make sure the model loaded correctly
    if (!sceneData.scene || sceneData.scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !sceneData.scene->mRootNode){
        cerr << "Error loading model: " << importer.GetErrorString() << endl;
        return -1;
    }

    // Print success msg
    cout << "Model loaded successfully" << modelPath << endl;

    // Set name
    string appName = "Assign05";
    string windowTitle = "Assign05";
    int windowWidth = 800;
    int windowHeight = 600;

    // Create GLFW window
    GLFWwindow* window = createGLFWWindow(windowTitle, windowWidth, windowHeight);

    // After GLFW window creation
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    sceneData.mousePos = glm::vec2(mx, my);

    // Hide cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Set the mouse motion cursor callback
    glfwSetCursorPosCallback(window, mouse_position_callback);
    
    // Set window user pointer
    glfwSetWindowUserPointer(window, &sceneData);

    // Set Key callBack function
    glfwSetKeyCallback(window, keyCallBack);

    // Setup up Vulkan via vk-bootstrap
    VulkanInitData vkInitData;
    initVulkanBootstrap(appName, window, vkInitData);

    // Setup basic forward rendering process
    string vertSPVFilename = "build/compiledshaders/" + appName + "/shader.vert.spv";
    string fragSPVFilename = "build/compiledshaders/" + appName + "/shader.frag.spv";

    // Create render engine
    VulkanInitRenderParams params = {vertSPVFilename, fragSPVFilename};

    // Before your drawing loop
    VulkanRenderEngine *renderEngine = new Assign05RenderEngine(vkInitData);
    renderEngine->initialize(&params);

    VulkanMesh vulkanMesh;

    // Create a mesh vertex obj. inside the loop
    for (unsigned int i = 0; i < sceneData.scene->mNumMeshes; i++) {
        aiMesh *aiMesh = sceneData.scene->mMeshes[i];
        Mesh<Vertex> mesh;
        extractMeshData(aiMesh, mesh);

        vulkanMesh = createVulkanMesh(vkInitData, renderEngine->getCommandPool(), mesh);
        sceneData.allMeshes.push_back(vulkanMesh);
    }

    float timeElapsed = 1.0f;
    int framesRendered = 0;
    auto startCountTime = getTime();
    float fpsCalcWindow = 5.0f;

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        float aspectRatio = (height > 0) ? static_cast<float>(width) / height : 1.0f;
        
        // Update proj matrix 
        sceneData.projMat = glm::perspective(glm::radians(90.0f), aspectRatio, 0.01f, 50.0f);
        //sceneData.projMat[1][1] *= -1; // Flip Y-coordinate for Vulkan's NDC

        // Update view matrix using glm::lookAt
        sceneData.viewMat = glm::lookAt(sceneData.eye, sceneData.lookAt, glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Update light's view position
        sceneData.light.vpos = (sceneData.viewMat * sceneData.light.pos);

        // Get start time
        auto startTime = getTime();

        glfwSwapBuffers(window);

        // Poll events for window
        glfwPollEvents();

        // Draw frame
        renderEngine->drawFrame(&sceneData);

        // Increment frame count
        framesRendered++;

        // Get end and elapsed
        auto endTime = getTime();
        float frameTime = getElapsedSeconds(startTime, endTime);

        float timeSoFar = getElapsedSeconds(startCountTime, getTime());

        if(timeSoFar >= fpsCalcWindow) {
            float fps = framesRendered / timeSoFar;
            cout << "FPS: " << fps << endl;

            startCountTime = getTime();
            framesRendered = 0;
        }
    }

    // Make sure all queues on GPU are done
    vkInitData.device.waitIdle();

    // Cleanup & After drawing loop
    //cleanupVulkanMesh(vkInitData, mesh);
    for (auto &vulkanMesh : sceneData.allMeshes) {
        cleanupVulkanMesh(vkInitData, vulkanMesh);
    }
    sceneData.allMeshes.clear();

    delete renderEngine;
    cleanupVulkanBootstrap(vkInitData);
    cleanupGLFWWindow(window);

    cout << "FORGING DONE!!!" << endl;
    return 0;
}