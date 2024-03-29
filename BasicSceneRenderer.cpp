#include "BasicSceneRenderer.h"
#include "Image.h"
#include "Prefabs.h"
#include "glm/gtx/string_cast.hpp"
#include <iostream>

int move_direction = 1;
int game_state = 0;

BasicSceneRenderer::BasicSceneRenderer()
    : mLightingModel(PER_VERTEX_DIR_LIGHT)
    , mCamera(NULL)
    , mProjMatrix(1.0f)
    , mActiveEntityIndex(0)
    , mDbgProgram(NULL)
    , mAxes(NULL)
    , mVisualizePointLights(true) {
}

void BasicSceneRenderer::initialize()
{
    // print usage instructions
    std::cout << "Usage:" << std::endl;
    std::cout << "  Camera control:           WASD + Mouse" << std::endl;
    std::cout << "  Rotate active entity:     Arrow keys" << std::endl;
    std::cout << "  Reset entity orientation: R" << std::endl;
    std::cout << "  Translate active entity:  IJKL (world space)" << std::endl;
    std::cout << "  Translate active entity:  TFGH (local space)" << std::endl;
    std::cout << "  Cycle active entity:      X/Z" << std::endl;
    std::cout << "  Toggle point light vis.:  Tab" << std::endl;

    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // enable blending (needed for textures with alpha channel)

    mPrograms.resize(NUM_LIGHTING_MODELS);
    mPrograms[PER_VERTEX_DIR_LIGHT] = new ShaderProgram("shaders/PerVertexDirLight-vs.glsl",
                                                                "shaders/PerVertexDirLight-fs.glsl");
    mPrograms[BLINN_PHONG_PER_FRAGMENT_DIR_LIGHT] = new ShaderProgram("shaders/BlinnPhongPerFragment-vs.glsl",
                                                                      "shaders/BlinnPhongPerFragmentDirLight-fs.glsl");
    mPrograms[BLINN_PHONG_PER_FRAGMENT_POINT_LIGHT] = new ShaderProgram("shaders/BlinnPhongPerFragment-vs.glsl",
                                                                        "shaders/BlinnPhongPerFragmentPointLight-fs.glsl");
    mPrograms[BLINN_PHONG_PER_FRAGMENT_MULTI_LIGHT] = new ShaderProgram("shaders/BlinnPhongPerFragment-vs.glsl",
                                                                        "shaders/BlinnPhongPerFragmentMultiLight-fs.glsl");

    //
    // Create meshes
    //

    mMeshes.push_back(CreateTexturedCube(1));
    mMeshes.push_back(CreateChunkyTexturedCylinder(0.5f, 1, 8));
    mMeshes.push_back(CreateSmoothTexturedCylinder(0.5f, 1, 15));

    float roomWidth = 140;
    float roomHeight = 24;
    float roomDepth = 52;
    float roomTilesPerUnit = 0.25f;


    // front and back walls
    Mesh* fbMesh = CreateTexturedQuad(roomWidth, roomHeight, roomWidth * roomTilesPerUnit, roomHeight * roomTilesPerUnit);
    mMeshes.push_back(fbMesh);
    // left and right walls
    Mesh* lrMesh = CreateTexturedQuad(roomDepth, roomHeight, roomDepth * roomTilesPerUnit, roomHeight * roomTilesPerUnit);
    mMeshes.push_back(lrMesh);
    // ceiling and floor
    Mesh* cfMesh = CreateTexturedQuad(roomWidth, roomDepth, roomWidth * roomTilesPerUnit, roomDepth * roomTilesPerUnit);
    mMeshes.push_back(cfMesh);

    //
    // Load textures
    //

    std::vector<std::string> texNames;
    texNames.push_back("textures/CarvedSandstone.tga");
    texNames.push_back("textures/rocky.tga");
    texNames.push_back("textures/bricks_overpainted_blue_9291383.tga");
    texNames.push_back("textures/water_drops_on_metal_3020602.tga");
    texNames.push_back("textures/skin.tga");
    texNames.push_back("textures/white.tga");
    texNames.push_back("textures/yo.tga");
    texNames.push_back("textures/black.tga");

    for (unsigned i = 0; i < texNames.size(); i++)
        mTextures.push_back(new Texture(texNames[i], GL_REPEAT, GL_LINEAR));

    //
    // Create materials
    //

    // add a material for each loaded texture (with default tint)
    for (unsigned i = 0; i < texNames.size(); i++)
        mMaterials.push_back(new Material(mTextures[i]));

    //
    // set extra material properties
    //

    // water drops (sharp and strong specular highlight)
    mMaterials[3]->specular = glm::vec3(1.0f, 1.0f, 1.0f);
    mMaterials[3]->shininess = 128;

    // skin (washed out and faint specular highlight)
    mMaterials[4]->specular = glm::vec3(0.3f, 0.3f, 0.3f);
    mMaterials[4]->shininess = 8;

    // white
    mMaterials[5]->specular = glm::vec3(0.75f, 0.75f, 0.75f);
    mMaterials[5]->shininess = 64;

    // yo
    mMaterials[6]->specular = glm::vec3(1.0f, 0.0f, 1.0f);  // magenta highlights
    mMaterials[6]->shininess = 16;

    // black
    mMaterials[7]->specular = glm::vec3(1.0f, 0.5f, 0.0f);  // orange hightlights
    mMaterials[7]->shininess = 16;

    //
    // Create Entities
    //

    unsigned numRows = mMaterials.size();
    float spacing = 3;
    float z = 0.5f * spacing * numRows;
    for (unsigned i = 2; i < mMaterials.size(); i++) {
        // cube
        mEntities.push_back(new Entity(mMeshes[0], mMaterials[i], Transform(-4.0f, 0.0f, z)));
        // chunky cylinder
        mEntities.push_back(new Entity(mMeshes[1], mMaterials[i], Transform( 0.0f, 0.0f, z)));
        // smooth cylinder
        mEntities.push_back(new Entity(mMeshes[2], mMaterials[i], Transform( 4.0f, 0.0f, z)));

        // next row
        z -= spacing;
    }

    //
    // Create room
    //

    // back wall
    //mEntities.push_back(new Entity(fbMesh, mMaterials[1], Transform(0, 0, -0.5f * roomDepth)));
    //// front wall
    //mEntities.push_back(new Entity(fbMesh, mMaterials[1], Transform(0, 0, 0.5f * roomDepth, glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)))));
    //// left wall
    //mEntities.push_back(new Entity(lrMesh, mMaterials[1], Transform(-0.5f * roomWidth, 0, 0, glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)))));
    //// right wall
    //mEntities.push_back(new Entity(lrMesh, mMaterials[1], Transform(0.5f * roomWidth, 0, 0, glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)))));
    // floor
    mEntities.push_back(new Entity(cfMesh, mMaterials[0], Transform(0, -0.5f * roomHeight, 0, glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)))));
    // ceiling
    mEntities.push_back(new Entity(cfMesh, mMaterials[0], Transform(0, 0.5f * roomHeight, 0, glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)))));

    //
    // create the camera
    //

    mCamera = new Camera(this);
    mCamera->setPosition(66, -10, 2);
    mCamera->lookAt(0, 0, 7);
    mCamera->setSpeed(2);

    // create shader program for debug geometry
    mDbgProgram = new ShaderProgram("shaders/vpc-vs.glsl",
                                    "shaders/vcolor-fs.glsl");

    // create geometry for axes
    mAxes = CreateAxes(2);

    CHECK_GL_ERRORS("initialization");
}

void BasicSceneRenderer::shutdown()
{
    for (unsigned i = 0; i < mPrograms.size(); i++)
        delete mPrograms[i];
    mPrograms.clear();

    delete mDbgProgram;
    mDbgProgram = NULL;

    delete mCamera;
    mCamera = NULL;

    for (unsigned i = 0; i < mEntities.size(); i++)
        delete mEntities[i];
    mEntities.clear();

    for (unsigned i = 0; i < mMeshes.size(); i++)
        delete mMeshes[i];
    mMeshes.clear();

    for (unsigned i = 0; i < mMaterials.size(); i++)
        delete mMaterials[i];
    mMaterials.clear();
    
    for (unsigned i = 0; i < mTextures.size(); i++)
        delete mTextures[i];
    mTextures.clear();

    delete mDbgProgram;
    mDbgProgram = NULL;
    
    delete mAxes;
    mAxes = NULL;
}

void BasicSceneRenderer::resize(int width, int height)
{
    glViewport(0, 0, width, height);

    // compute new projection matrix
    mProjMatrix = glm::perspective(glm::radians(50.f), width / (float)height, 0.1f, 1000.0f);
}

void BasicSceneRenderer::draw()
{

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // activate current program
    ShaderProgram* prog = mPrograms[mLightingModel];
    prog->activate();

    // send projection matrix
    prog->sendUniform("u_ProjectionMatrix", mProjMatrix);

    // send the texture sampler id to shader
    prog->sendUniformInt("u_TexSampler", 0);

    // get the view matrix from the camera
    glm::mat4 viewMatrix = mCamera->getViewMatrix();

    // direction to light
    glm::vec4 lightDir = glm::normalize(glm::vec4(1, 3, 2, 0));

    // send light direction in eye space
    prog->sendUniform("u_LightDir", glm::vec3(viewMatrix * lightDir));

    // send light color/intensity
    prog->sendUniform("u_LightColor", glm::vec3(1.0f, 1.0f, 1.0f));

    if (game_state == 0) {

    }
    else {
        for (unsigned i = 0; i < mEntities.size(); i++) {

            Entity* ent = mEntities[i];

            // use the entity's material
            const Material* mat = ent->getMaterial();
            glBindTexture(GL_TEXTURE_2D, mat->tex->id());   // bind texture
            prog->sendUniform("u_Tint", mat->tint);     // send tint color

            // send the Blinn-Phong parameters, if required
            if (mLightingModel > PER_VERTEX_DIR_LIGHT) {
                prog->sendUniform("u_MatEmissiveColor", mat->emissive);
                prog->sendUniform("u_MatSpecularColor", mat->specular);
                prog->sendUniform("u_MatShininess", mat->shininess);
            }

            // compute modelview matrix
            glm::mat4 modelview = viewMatrix * ent->getWorldMatrix();

            // send the entity's modelview and normal matrix
            prog->sendUniform("u_ModelviewMatrix", modelview);
            prog->sendUniform("u_NormalMatrix", glm::transpose(glm::inverse(glm::mat3(modelview))));

            // use the entity's mesh
            const Mesh* mesh = ent->getMesh();
            mesh->activate();
            mesh->draw();
        }

        //
        // draw local axes for current entity
        //

        mDbgProgram->activate();
        mDbgProgram->sendUniform("u_ProjectionMatrix", mProjMatrix);

        Entity* activeEntity = mEntities[mActiveEntityIndex];
        mDbgProgram->sendUniform("u_ModelviewMatrix", viewMatrix * activeEntity->getWorldMatrix());
        mAxes->activate();
        mAxes->draw();

        CHECK_GL_ERRORS("drawing");
    }
}

static float t;
void SplineInit()
{
	t = 0.0f;
}

glm::vec3 SplinePointOnCurve(float dt, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3)
{
	//
	//	The spline passes through all of the control points.
	//	The spline is C1 continuous, meaning that there are no discontinuities in the tangent direction and magnitude.
	//	The spline is not C2 continuous.  The second derivative is linearly interpolated within each segment, causing the curvature to vary linearly over the length of the segment.
	//	Points on a segment may lie outside of the domain of P1 -> P2.
	glm::vec3 vOut = glm::vec3(0.0f, 0.0f, 0.0f);

	// update state
	t += dt;
	if (t > 1.0f)
		t = 1.0f;
	float t2 = t * t;
	float t3 = t2 * t;

	vOut = 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);

	return vOut;
}
bool BasicSceneRenderer::update(float dt)
{
    const Keyboard* kb = getKeyboard();

    if (kb->keyPressed(KC_ESCAPE))
        return false;

    if (game_state == 0) {
        if (kb->keyPressed(KC_RETURN))
            game_state = 1;

        // update the camera
        mCamera->update(dt);
    }
    else {

        // get the entity to manipulate
        //Entity* activeEntity = mEntities[mActiveEntityIndex];

        float speed = 15;
        float disp = speed * dt;

        for (unsigned int a = 0; a < sizeof(mEntities); a = a + 1)
        {
            if (move_direction == 1)
                mEntities[a]->translate(0, 0, disp);
            else
                mEntities[a]->translate(0, 0, -disp);

            if (ceil(mEntities[a]->getPosition().z) == 25.00 || floor((mEntities[a]->getPosition().z) == 25.00) || ceil(mEntities[a]->getPosition().z) == -25.00 || floor((mEntities[a]->getPosition().z) == 25.00)) {
                move_direction = move_direction * -1;
            }
        }

        // change lighting models
        if (kb->keyPressed(KC_1))
            mLightingModel = PER_VERTEX_DIR_LIGHT;
        if (kb->keyPressed(KC_2))
            mLightingModel = BLINN_PHONG_PER_FRAGMENT_DIR_LIGHT;
        if (kb->keyPressed(KC_3))
            mLightingModel = BLINN_PHONG_PER_FRAGMENT_POINT_LIGHT;
        if (kb->keyPressed(KC_4))
            mLightingModel = BLINN_PHONG_PER_FRAGMENT_MULTI_LIGHT;

        // toggle visualization of point lights
        if (kb->keyPressed(KC_TAB))
            mVisualizePointLights = !mVisualizePointLights;

        //
        // start spline
        //
        static bool bSplineStart = false;
        if (kb->keyPressed(KC_S))
        {
            SplineInit();
            bSplineStart = true;
        }
        if (bSplineStart)
        {
            glm::vec3 vPos = SplinePointOnCurve(dt, mEntities[0]->getPosition(),
                mEntities[1]->getPosition(),
                mEntities[2]->getPosition(),
                mEntities[3]->getPosition());

            mEntities[4]->setPosition(vPos);
        }

        // update the camera
        mCamera->update(dt);
    }

    return true;
}
