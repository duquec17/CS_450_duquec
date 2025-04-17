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

// Hold information for a vertex
struct Vertex {
    glm::vec3 pos;
    glm::vec4 color;
};

// Hold scene data
struct SceneData {
    vector<VulkanMesh> allMeshes;
    const aiScene *scene = nullptr;
    float rotAngle = 0.0f;
    
    glm::vec3 eye;
    glm::vec3 lookAt;
    glm::vec2 mousePos;
    glm::mat4 currentViewMat;
    glm::mat4 currentProjMat;
};

// Hold Vertex shader UBO host data
struct UBOVertex {
    alignas(16) glm::mat4 viewjMat;
    alignas(16) glm::mat4 projMat;
};

// Hold vertex shader push constants
struct UPushVertex {
    alignas(16)glm::mat4 modelMat;
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
    float radi = glm::radians(angle);
    glm::mat4 negTrans = glm::translate(-offset);
    glm::mat4 rotAxi = glm::rotate(radi, glm::vec3(0.0f,0.0f, 1.0f));
    glm::mat4 transPos = glm::translate(offset);
    return negTrans * rotAxi * transPos;
}

// New class that inherits from VlkrEngine
class Assign04RenderEngine : public VulkanRenderEngine{
    // Constructor
    public:
        Assign04RenderEngine(VulkanInitData &vkInitData):
        VulkanRenderEngine(vkInitData){};

    // Overrides initialize function
    virtual bool initialize(VulkanInitRenderParams *params) override {
        if(!VulkanRenderEngine::initialize(params)){return false;}
        return true;
    };

    // Destructor
    virtual~Assign04RenderEngine(){};

    /// Override recordCommandBuffer function
    virtual void recordCommandBuffer(void *userData,
                                     vk::CommandBuffer &commandBuffer,
                                     unsigned int frameIndex) override {
        // Void data is assumed to be vector of meshes only
        vector<VulkanMesh> *allMeshes = static_cast<vector<VulkanMesh>*>(userData);

        // Cast the userData as a SceneData pointer
        SceneData *sceneData = static_cast<SceneData*>(userData);

        // Begin commands
        commandBuffer.begin(vk::CommandBufferBeginInfo());

        // Get the extents of the buffers (since we'll use it a few times)
        vk::Extent2D extent = vkInitData.swapchain.extent;

        // Begin render pass
        array<vk::ClearValue, 2> clearValues {};
        clearValues[0].color = vk::ClearColorValue(0.6f, 0.1f, 0.7f, 1.0f);
        clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0.0f);

        commandBuffer.beginRenderPass(vk::RenderPassBeginInfo(
            this->renderPass,
            this->framebuffers[frameIndex],
            { {0,0}, extent },
            clearValues),
            vk::SubpassContents::eInline);

        // Bind pipeline
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            this->pipelineData.graphicsPipeline);

        // Set up viewport and scissors
        vk::Viewport viewports[] = {{0, 0, (float)extent.width, (float)extent.height, 0.0f, 1.0f}};
        commandBuffer.setViewport(0, viewports);

        vk::Rect2D scissors[] = {{{0,0}, extent}};
        commandBuffer.setScissor(0, scissors);

        // Loopthrough and record on each mesh in sceneData->allMeshes
        //for (auto& mesh : sceneData->allMeshes) {
            //recordDrawVulkanMesh(commandBuffer, allMeshes->at(0));
        //}

        // Instead of loop with recordDrawVulkan, call renderScene
        renderScene(commandBuffer, sceneData, sceneData->scene->mRootNode, glm::mat4(1.0f), 0);

        // Stop render pass
        commandBuffer.endRenderPass();

        // End command buffer
        commandBuffer.end();
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

        UPushVertex uPush;
        uPush.modelMat = tmpModel;

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

        //Set the color of the vertex to any color other than (0,0,1) or the background
        vertex.color = glm::vec4(
            (float)(i % 256) / 255.0f, // Red
            (float)((i / 256) % 256) / 255.0f, // Green
            0.5f, // Blue
            1.0f // Alpha
        );

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
    cout << "BEGIN Model FORGING!!!" << endl;

    // The model to load will be provided on the command line
    // Use sampleModels sphere as default model path
    string modelPath = "sampleModels/teapot.obj";
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
    string appName = "Assign04";
    string windowTitle = "Assign04";
    int windowWidth = 800;
    int windowHeight = 600;

    // Create GLFW window
    GLFWwindow* window = createGLFWWindow(windowTitle, windowWidth, windowHeight);

    // 
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
    VulkanInitRenderParams params = {
        vertSPVFilename, fragSPVFilename
    };

    // Before your drawing loop
    VulkanRenderEngine *renderEngine = new Assign04RenderEngine(vkInitData);
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

    /* Comment out the current code that creates hostMesh, VulkanMesh, & list
    // Create very simple quad on host    
    Mesh<SimpleVertex> hostMesh = {
        {
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}
        },
        { 0, 2, 1, 2, 0, 3 }
    };

    // Create Vulkan mesh
    VulkanMesh mesh = createVulkanMesh(vkInitData, renderEngine->getCommandPool(), hostMesh);
    vector<VulkanMesh> allMeshes {
        { mesh }
    };
    */

    float timeElapsed = 1.0f;
    int framesRendered = 0;
    auto startCountTime = getTime();
    float fpsCalcWindow = 5.0f;

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        // Get start time
        auto startTime = getTime();

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
    for (auto &VulkanMesh : sceneData.allMeshes) {
        cleanupVulkanMesh(vkInitData, vulkanMesh);
    }
    sceneData.allMeshes.clear();

    delete renderEngine;
    cleanupVulkanBootstrap(vkInitData);
    cleanupGLFWWindow(window);

    cout << "FORGING DONE!!!" << endl;
    return 0;
}