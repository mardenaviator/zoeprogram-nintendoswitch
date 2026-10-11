#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

//#include "font.hpp"
//#include <freetype2/freetype/freetype.h>

#include <EGL/egl.h>    // EGL library
#include <EGL/eglext.h> // EGL extensions
//#include <glad/glad.h>  // glad library (OpenGL loader)
//#include <assimp/Importer.hpp>
#include "model.hpp"
//#include "shader.hpp"
#include "camera.hpp"

#include <bullet/btBulletDynamicsCommon.h>

#define ENABLE_NXLINK
//-----------------------------------------------------------------------------
// nxlink support
//-----------------------------------------------------------------------------

#ifndef ENABLE_NXLINK
#define TRACE(fmt,...) ((void)0)
#else
#include <unistd.h>
#define TRACE(fmt,...) printf("%s: " fmt "\n", __PRETTY_FUNCTION__, ## __VA_ARGS__)

static int s_nxlinkSock = -1;

static void initNxLink()
{
    if (R_FAILED(socketInitializeDefault()))
        return;

    s_nxlinkSock = nxlinkStdio();
    if (s_nxlinkSock >= 0)
        TRACE("printf output now goes to nxlink server");
    else
        socketExit();
}

static void deinitNxLink()
{
    if (s_nxlinkSock >= 0)
    {
        close(s_nxlinkSock);
        socketExit();
        s_nxlinkSock = -1;
    }
}

extern "C" void userAppInit()
{
    initNxLink();
}

extern "C" void userAppExit()
{
    deinitNxLink();
}

#endif

//-----------------------------------------------------------------------------
// EGL initialization
//-----------------------------------------------------------------------------

static EGLDisplay s_display;
static EGLContext s_context;
static EGLSurface s_surface;

static bool initEgl(NWindow* win)
{
    // Connect to the EGL default display
    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!s_display)
    {
        TRACE("Could not connect to display! error: %d", eglGetError());
        goto _fail0;
    }

    // Initialize the EGL display connection
    eglInitialize(s_display, nullptr, nullptr);

    // Select OpenGL (Core) as the desired graphics API
    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
    {
        TRACE("Could not set API! error: %d", eglGetError());
        goto _fail1;
    }

    // Get an appropriate EGL framebuffer configuration
    EGLConfig config;
    EGLint numConfigs;
    static const EGLint framebufferAttributeList[] =
    {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE,     8,
        EGL_GREEN_SIZE,   8,
        EGL_BLUE_SIZE,    8,
        EGL_ALPHA_SIZE,   8,
        EGL_DEPTH_SIZE,   24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    eglChooseConfig(s_display, framebufferAttributeList, &config, 1, &numConfigs);
    if (numConfigs == 0)
    {
        TRACE("No config found! error: %d", eglGetError());
        goto _fail1;
    }

    // Create an EGL window surface
    s_surface = eglCreateWindowSurface(s_display, config, win, nullptr);
    if (!s_surface)
    {
        TRACE("Surface creation failed! error: %d", eglGetError());
        goto _fail1;
    }

    // Create an EGL rendering context
    static const EGLint contextAttributeList[] =
    {
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
        EGL_CONTEXT_MINOR_VERSION_KHR, 3,
        EGL_NONE
    };
    s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, contextAttributeList);
    if (!s_context)
    {
        TRACE("Context creation failed! error: %d", eglGetError());
        goto _fail2;
    }

    // Connect the context to the surface
    eglMakeCurrent(s_display, s_surface, s_surface, s_context);
    return true;

_fail2:
    eglDestroySurface(s_display, s_surface);
    s_surface = nullptr;
_fail1:
    eglTerminate(s_display);
    s_display = nullptr;
_fail0:
    return false;
}

static void deinitEgl()
{
    if (s_display)
    {
        eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s_context)
        {
            eglDestroyContext(s_display, s_context);
            s_context = nullptr;
        }
        if (s_surface)
        {
            eglDestroySurface(s_display, s_surface);
            s_surface = nullptr;
        }
        eglTerminate(s_display);
        s_display = nullptr;
    }
}

//-----------------------------------------------------------------------------
// Main program
//-----------------------------------------------------------------------------


int main(int argc, char* argv[])
{
   
    // Initialize EGL on the default window
    if (!initEgl(nwindowGetDefault()))
        return EXIT_FAILURE;

    // Load OpenGL routines using glad
    gladLoadGL();
    stbi_set_flip_vertically_on_load(true);


    Shader shader("source/vertexshader.txt", "source/fragmentshader.txt");
    Model ourModel("data/zoetown/ZOETOWN.obj");
  
    Camera camera;
    
   // Font font;
  //  font.initializeFont(48);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    glEnable(GL_DEPTH_TEST);
    auto* collisionConfiguration = new btDefaultCollisionConfiguration();
    auto* dispatcher = new btCollisionDispatcher(collisionConfiguration);
    auto* broadphase = new btDbvtBroadphase();
    auto* solver = new btSequentialImpulseConstraintSolver();

// 2. Instantiate the world
    auto* dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);

// 3. Set global environment parameters (e.g., Earth's gravity)
    dynamicsWorld->setGravity(btVector3(0, 0, 0)); 

    // 1. Создаём пустой контейнер для треугольников
btTriangleMesh* triMesh = new btTriangleMesh();

// 2. Проходим по всем мешам модели
for (auto& mesh : ourModel.meshes) {
    // indices хранит номера вершин. Идём тройками: i, i+1, i+2 = один треугольник
    for (unsigned i = 0; i < mesh.indices.size(); i += 3) {

        // Достаём три точки треугольника
        glm::vec3 p0 = mesh.vertices[mesh.indices[i+0]].Position;
        glm::vec3 p1 = mesh.vertices[mesh.indices[i+1]].Position;
        glm::vec3 p2 = mesh.vertices[mesh.indices[i+2]].Position;

        // Добавляем треугольник в Bullet-сетку
        triMesh->addTriangle(
            btVector3(p0.x, p0.y, p0.z),
            btVector3(p1.x, p1.y, p1.z),
            btVector3(p2.x, p2.y, p2.z));
    }
}

// 3. Оборачиваем сетку в форму с BVH-ускорением
btBvhTriangleMeshShape* shape = new btBvhTriangleMeshShape(triMesh, true);

// 4. Позиция тела в мире — ставим туда же, где модель
btTransform startTransform;
startTransform.setIdentity();
startTransform.setOrigin(btVector3(0, 0, 0)); // позиция модели в мире

btDefaultMotionState* motion = new btDefaultMotionState(startTransform);

// 5. mass = 0 → тело статичное, оно не падает и не двигается
btRigidBody::btRigidBodyConstructionInfo info(0.0f, motion, shape);
btRigidBody* body = new btRigidBody(info);

// 6. Добавляем в мир — теперь камера будет об него биться
dynamicsWorld->addRigidBody(body);

btCapsuleShape* camShape = new btCapsuleShape(0.4f, 1.2f); // radius, height// радиус "игрока"
btVector3 inertia(0,0,0);
camShape->calculateLocalInertia(1.0f, inertia);

btDefaultMotionState* camMotion = new btDefaultMotionState(
    btTransform(btQuaternion(0,0,0,1), btVector3(0, 5, 0)));

btRigidBody::btRigidBodyConstructionInfo camInfo(
    1.0f, camMotion, camShape, inertia);

btRigidBody* camBody = new btRigidBody(camInfo);
camBody->setAngularFactor(0); // чтобы не крутился
dynamicsWorld->addRigidBody(camBody);




    // Main graphics loop
    while (appletMainLoop())
    {
        static uint64_t lastTime = armGetSystemTick();
        uint64_t currentTime = armGetSystemTick();
        float deltaTime = (float)(currentTime - lastTime) / (float)armGetSystemTickFreq();
        lastTime = currentTime;

        // Get and process input
        padUpdate(&pad);
        u32 kDown = padGetButtonsDown(&pad);
        u32 kHeld = padGetButtons(&pad); 

        if (kDown & HidNpadButton_Plus)  break;

        camera.ProcessMouseMovement(&pad, deltaTime);
        camera.ProcessKeyboard(kHeld, &pad, deltaTime);
        
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)1280/ (float)720, 0.1f, 100.0f);

        glm::mat4 view = camera.GetViewMatrix();
    
        

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
       
       
        model = glm::rotate(model, glm::radians(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        

        shader.setMat4("model", model);
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        ourModel.Draw(shader);
        
        
    

        btTransform t = camBody->getWorldTransform();
t.setOrigin(btVector3(camera.Position.x, camera.Position.y, camera.Position.z));
camBody->setWorldTransform(t);
camBody->getMotionState()->setWorldTransform(t);
camBody->setLinearVelocity(btVector3(0, 0, 0));    // ← сброс скорости
camBody->setAngularVelocity(btVector3(0, 0, 0));
camBody->activate(true);     

dynamicsWorld->stepSimulation(deltaTime, 10);

// После шага — прочитать итоговую позицию (Bullet вытолкнул камеру из стены)
btVector3 finalPos = camBody->getWorldTransform().getOrigin();
camera.Position = glm::vec3(finalPos.x(), finalPos.y(), finalPos.z());


        
        eglSwapBuffers(s_display, s_surface);
    }

   delete camBody;
    delete camMotion;
delete camShape;
delete body;
    delete motion;
    delete shape;
delete triMesh;
delete dynamicsWorld;
delete solver;
delete broadphase;
delete dispatcher;
delete collisionConfiguration;

    // Deinitialize EGL
    deinitEgl();
    return EXIT_SUCCESS;
}
