#pragma warning(disable:4819)

#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>


#include "cube.h"
#include "shader.h"
using namespace std;


GLFWwindow* window;

//Camera camera(glm::vec3(0.0, 0.0, 3.0));
float gYaw = 0.0f;
float gPitch = 0.0f;
float gRadius = 10.0f;

float mYaw = 0.0f;
float mPitch = 0.0f;
float mRadius = 10.0f;

glm::vec3 mPosition = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 mTargetPos = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 mUp = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 mRight = glm::vec3(0.0f, 0.0f, 0.0f);

const float MOUSE_SENSITIVITY = 0.25f;

float fov = 45.0f;
double screen_width = 1000.0;
double screen_height = 1000.0;
double lastX = screen_width/2.0;
double lastY = screen_height/2.0;
float x_offset = 0;
float y_offset = 0;
bool first_mouse = true;

glm::vec4 bg_color = glm::vec4(0.1f, 0.1f, 0.1f, 0.1f);

void init();
unsigned int readVolume(std::string file_name);
unsigned int readTFFtoTexture(void);
void mouseCallback(GLFWwindow *window, double x_pos, double y_pos);
void checkFramebufferStatus();
void RCSetUniforms(Shader shader, glm::mat4 MVP, unsigned int tff, unsigned int bf, unsigned int vol);
unsigned int genBackFaceTextureBuffer(int tex_width, int tex_height);
unsigned int gen2DFramebuffer(unsigned int tex_obj, int tex_width, int tex_height);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);


int main()
{   
    string file_name = "./data/256x256x39_16bitsigned_BE.raw";
    glm::vec3 volume_size = glm::vec3(256, 256, 39);

    init();

    Shader eep_shader("./shaders/cube.vert", "./shaders/cube.frag");
    Shader rc_shader("./shaders/raycasting.vert", "./shaders/raycasting.frag");
    Cube eep_cube(eep_shader);
    Cube rc_cube(rc_shader);

    // 볼륨 렌더링에 필요한 텍스쳐 설정
    unsigned int vol_tex_obj = readVolume("./data/256x256x39_16bitsigned_BE.raw"); // 렌더패스 2에서 사용할 볼륨 데이터를 3D 텍스쳐로 만들어 사용
    unsigned int tff_tex_obj = readTFFtoTexture(); // 렌더패스 2에서 볼륨 데이터의 인텐시티 값에 매칭될 색상값을 저장한 데이터 파일, 1D 텍스쳐에 저장
    unsigned int bf_tex_obj = genBackFaceTextureBuffer(screen_width, screen_height); // 렌더패스 1에서 결과 이미지(화면상에 렌더링 될 프래그먼트 컬러값을 프래그먼트의 월드좌표상 좌표로 저장)
    unsigned int back_buffer = gen2DFramebuffer(bf_tex_obj, screen_width, screen_height); // 백버퍼

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(180.0f) , glm::vec3(0.0, 1.0, 0.0));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0, 0.0 ,0.0));

        model = glm::translate(model, glm::vec3(-1.0, -1.0, -1.0));
       
        //spacing
        model = glm::scale(model, glm::vec3(1.4532f, 1.4532f, 4.0f));

        glm::mat4 view;

        mTargetPos = glm::vec3(0.0f, 0.0f, 0.0f);
        mYaw = glm::radians(gYaw);
        mPitch = glm::radians(gPitch);
        mPitch = glm::clamp(mPitch, -glm::pi<float>() / 2.0f + 0.1f, glm::pi<float>() / 2.0f - 0.1f);

        //update camera vectors;
        mPosition.x = mTargetPos.x + mRadius * cosf(mPitch) * sinf(mYaw);
        mPosition.y = mTargetPos.y + mRadius * sinf(mPitch);
        mPosition.z = mTargetPos.z + mRadius * cosf(mPitch) * cosf(mYaw);

        //set Radius
        mRadius = glm::clamp(gRadius, 2.0f, 80.0f);

        view = glm::lookAt(mPosition, mTargetPos, mUp);

        //translate
        view = glm::translate(view, glm::vec3(x_offset*0.1f, y_offset*0.1f, 0.0f));

        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)screen_width/(float)screen_height, 0.1f, 1000.0f);
        glm::mat4 MVP = projection * view * model;

        //front face 제거. 최종 이미지를 back_buffer에 binding된 2D 텍스쳐에 draw.
        glEnable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, back_buffer);
        glViewport(0,0, screen_width, screen_height);
        glClearColor(bg_color[0], bg_color[1], bg_color[2] ,bg_color[3]);
        glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);
        eep_shader.use();
        eep_shader.setMat4("MVP", MVP);
        eep_cube.draw(GL_FRONT);
        glUseProgram(0);

        //back face 제거. volume, tff texture를 쉐이더 유니폼 변수에 전달. 2D 텍스쳐 이미지도 함께 전달.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0,0, screen_width, screen_height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(bg_color[0], bg_color[1], bg_color[2] ,bg_color[3]);

        RCSetUniforms(rc_shader, MVP, tff_tex_obj, bf_tex_obj , vol_tex_obj);
        rc_cube.draw(GL_BACK);
        glUseProgram(0);
        //GL_ERROR();

        glfwSwapBuffers(window);

    }
    glDeleteTextures(1, &bf_tex_obj);
    glDeleteTextures(1, &tff_tex_obj);
    glDeleteTextures(1, &vol_tex_obj);
    glDeleteFramebuffers(1,&back_buffer);

    return EXIT_SUCCESS;
}

void checkFramebufferStatus()
{
    GLenum var = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(var != GL_FRAMEBUFFER_COMPLETE){
        std::cout << "Framebuffer is not complete." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Framebuffer is complete." <<std::endl;
}

unsigned int genBackFaceTextureBuffer(int tex_width, int tex_height)
{
    unsigned int bf_tex;

    glGenTextures(1, &bf_tex);
    glBindTexture(GL_TEXTURE_2D,bf_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, tex_width, tex_height, 0, GL_RGBA, GL_FLOAT, NULL);

    return bf_tex;
}

unsigned int gen2DFramebuffer(unsigned int tex_obj, int tex_width, int tex_height)
{
    unsigned int depth_buffer;

    glGenRenderbuffers(1, &depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, tex_width, tex_height);

    unsigned int fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_obj, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);

    checkFramebufferStatus();

    glEnable(GL_DEPTH_TEST);

    return fbo;
}

void RCSetUniforms(Shader shader, glm::mat4 MVP, unsigned int tff, unsigned int bf, unsigned int vol)
{
    shader.use();
    shader.setMat4("MVP", MVP);

    shader.setVec2("Resolution", glm::vec2(screen_width, screen_height));
    shader.setFloat("Step_Size", 0.5/256.0);
    shader.setVec4("Bg_Color", bg_color);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, tff);
    shader.setInt("TFF", 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bf);
    shader.setInt("ExitPoints", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, vol);
    shader.setInt("Volume", 2);
}

void mouseCallback(GLFWwindow* window, double x_pos, double y_pos)
{
    //rotation
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == 1) {
        gYaw -= ((float)x_pos - lastX) * MOUSE_SENSITIVITY;
        gPitch += ((float)y_pos - lastY) * MOUSE_SENSITIVITY;
    }

    //translate
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == 1) {
        x_offset += (x_pos - lastX)*MOUSE_SENSITIVITY;
        y_offset += (lastY - y_pos)*MOUSE_SENSITIVITY;
    }
    
    lastX = x_pos;
    lastY = y_pos;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
   fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 200.0f)
        fov = 200.0f;
}

void init()
{
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(screen_width, screen_height, "Volume Rendering 20180280 HeeyeonKIM", NULL, NULL);

    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glViewport(0, 0, screen_width, screen_height);


    //GLEW 초기화
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW.\n" << std::endl;
        glfwTerminate();
        return;
    }

}

unsigned int readVolume(std::string file_name) {

    unsigned int vol_tex_obj;
    const char* volume = file_name.c_str();

    GLuint width = 256;
    GLuint height = 256;
    GLuint depth = 39;

    FILE* fp;

    if (fopen_s(&fp, volume, "rb"))
    {
        std::cout << "ERROR : Failed to open " << volume << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "SUCCESS : Opened " << volume << std::endl;

    size_t size = width * height * depth;

    signed short* data = new signed short[size];

    std::cout << "volume size = " << size << std::endl;


    if (fread(data, sizeof(signed short), size, fp) != size)
    {
        std::cout << "ERROR : Failed to read " << volume << std::endl;
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    glGenTextures(1, &vol_tex_obj);
    glBindTexture(GL_TEXTURE_3D, vol_tex_obj);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    std::cout << "Width, Height, Depth : " << width << ", " << height << ", " << depth << std::endl;
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, width, height, depth, 0, GL_RED, GL_SHORT, data);

    delete[]data;
    std::cout << "Volume Loaded on Texture " << std::endl;

    return vol_tex_obj;
}

unsigned int readTFFtoTexture(void) {
    unsigned int tff_tex_obj;
    GLbyte tfRGBA[256][4];
    for (int i = 0; i < 200; i++) {
        tfRGBA[i][0] = (GLubyte)(0.0);
        tfRGBA[i][1] = (GLubyte)(0.0);
        tfRGBA[i][2] = (GLubyte)(0.0);
        tfRGBA[i][3] = (GLubyte)(0.0);
    }
    for (int i = 119; i < 256; i++) {
        tfRGBA[i][0] = (GLubyte)(255 * ((float)(i - 119) / (256 - 119)));
        tfRGBA[i][1] = (GLubyte)(255 * ((float)(i - 119) / (256 - 119)));
        tfRGBA[i][2] = (GLubyte)(255 * ((float)(i - 119) / (256 - 119)));
        tfRGBA[i][3] = (GLubyte)(255 * 0.1);
    }


    GLbyte tfRGBA2[512][4];
    for (int i = 0; i < 119; i++) {
        tfRGBA2[i][0] = (GLubyte)(0.0);
        tfRGBA2[i][1] = (GLubyte)(0.0);
        tfRGBA2[i][2] = (GLubyte)(0.0);
        tfRGBA2[i][3] = (GLubyte)(0.0);
    }
    for (int i = 119; i < 325; i++) {
        tfRGBA2[i][0] = (GLubyte)(255 * ((float)(i - 119) / (325 - 119)));
        tfRGBA2[i][1] = (GLubyte)(255 * ((float)(i - 119) / (325 - 119)));
        tfRGBA2[i][2] = (GLubyte)(255 * ((float)(i - 119) / (325 - 119)));
        tfRGBA2[i][3] = (GLubyte)(255 * 0.1);
    }
    for (int i = 325; i < 512; i++) {
        tfRGBA2[i][0] = (GLubyte)(255.0);
        tfRGBA2[i][1] = (GLubyte)(255.0);
        tfRGBA2[i][2] = (GLubyte)(255.0);
        tfRGBA2[i][3] = (GLubyte)(255 * 1.0);
    }

    glGenTextures(1, &tff_tex_obj);
    glBindTexture(GL_TEXTURE_1D, tff_tex_obj);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, tfRGBA);
    //glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, tfRGBA2);


    return tff_tex_obj;
}
