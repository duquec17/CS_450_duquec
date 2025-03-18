#include "VKSetup.hpp"
#include "VKRender.hpp"
#include "VKImage.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "MeshData.hpp"

// Hold information for a vertex
struct Vertex {
    glm::vec3 pos;
    glm::vec4 color;
};

// Hold scene data
struct SceneData {
    vector<VulkanMesh> allMeshes;
    const aiScene *scene = nullptr;
};

// Global instance of struct
SceneData sceneData;

// New class that inherits from VlkrEngine
class Assign02RenderEngine : public VulkanRenderEngine{
    // Constructor
    public:
        Assign02RenderEngine(VulkanInitData &vkInitData):
        VulkanRenderEngine(vkInitData){};

    // Overrides initialize function
    virtual bool initialize(VulkanInitRenderParams *params) override {
        if(!VulkanRenderEngine::initialize(params)){return false;}
        return true;
    };

    // Destructor
    virtual~Assign02RenderEngine(){};

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
        for (auto& mesh : sceneData->allMeshes) {
            recordDrawVulkanMesh(commandBuffer, allMeshes->at(0));
        }

        // Stop render pass
        commandBuffer.endRenderPass();

        // End command buffer
        commandBuffer.end();
    };
};

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
    string modelPath = "sampleModels/sphere.obj";
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
    string appName = "Assign02";
    string windowTitle = "Assign02:duquec";
    int windowWidth = 800;
    int windowHeight = 600;

    // Create GLFW window
    GLFWwindow* window = createGLFWWindow(windowTitle, windowWidth, windowHeight);

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
    VulkanRenderEngine *renderEngine = new Assign02RenderEngine(vkInitData);
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